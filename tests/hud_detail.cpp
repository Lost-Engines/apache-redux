#include "capture_options.h"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include "frontend/mission_session.h"
#include "render/vector_hud.h"

namespace {
namespace frontend = apache::frontend;
namespace render = apache::render;
namespace resources = apache::resources;
void require(bool const value, char const *const message) {
  if(!value) throw std::runtime_error{message};
}
} // anonymous namespace
int main(int argc, char **argv) try {
  auto const command_line{apache::testing::read_capture_options(argc, argv, "HUD detail checks", "capture-directory", false)};
  if(command_line.help) return EXIT_SUCCESS;
  resources::data_store data{command_line.directory};
  if(command_line.capture.has_value()) std::filesystem::create_directories(command_line.capture->string());
  frontend::mission_options options;
  options.detail = 4;
  auto session{std::make_unique<frontend::mission_session>(data, frontend::mission_session::quick_start(data, 3), options)};
  session->paused = true;
  session->draw();
  auto const normal{session->world.palette.modified};
  bool const night_vision{session->night_vision};
  uint8_t const colour{session->instruments->state.helmet.colour};
  session->post(0x17);
  session->step(1);
  session->draw();
  require(session->instruments->state.helmet.colour != colour && session->night_vision == night_vision && session->world.palette.modified == normal, "I must change the HUD colour without changing night vision or the scene palette");
  session->post(0x2f);
  session->step(1);
  session->draw();
  require(session->night_vision != night_vision && session->world.palette.modified != normal && session->visual.infrared == (night_vision ? 0u : 1u), "V must toggle PNVS and the scene palette");
  session->post(0x2f);
  session->step(1);
  std::cout << "I changes HUD colour; V changes PNVS and the world palette\n";

  for(unsigned int mode : {0u, 1u}) {
    unsigned int const width{mode ? 640u : 320u};
    unsigned int const height{mode ? 480u : 240u};
    render::helmet_display helmet{session->runtime.executable, session->runtime.trig, mode};
    render::helmet_instruments state;
    state.view = 0;
    state.colour = 223;
    state.airspeed = 123;
    state.waypoint_distance = 543210;
    state.torque = 81;
    render::framebuffer native{3840, 2160};
    render::display_layout const layout{3840, 2160, width, height, {0, 0, static_cast<int>(width) - 1, static_cast<int>(height) - 1}, 90};
    for(auto component : {render::helmet_component::airspeed, render::helmet_component::waypoint, render::helmet_component::engines}) {
      render::framebuffer original{width, height};
      render::framebuffer lettering{width, height};
      original.coverage.assign(original.image.pixels.size(), 0);
      lettering.coverage.assign(lettering.image.pixels.size(), 0);
      helmet.draw_component(original, state, component);
      render::vector_hud vectors{native, layout.hud_transform({0, 0, static_cast<int>(width) - 1, static_cast<int>(height) - 1}, 90), state.colour};
      helmet.draw_component(lettering, state, component, &vectors);
      require(original.image.pixels == lettering.image.pixels && original.coverage == lettering.coverage && native.hud_coverage.empty(), "native HUD strokes changed the original lettering");
    }
  }
  std::cout << "Both original font sizes retain identical glyph pixels and placement\n";

  for(auto const dimensions : {std::array{640u, 480u}, {1437u, 911u}, {3840u, 2160u}}) {
    for(double fov : {60.0, 90.0, 150.0}) {
      auto graphics{options.graphics};
      graphics.field_of_view = fov;
      session->configure_graphics(graphics);
      session->resize(dimensions[0], dimensions[1]);
      session->draw();
      require(!session->frame.hud_coverage.empty(), "integrated cockpit did not draw native HUD geometry");
      auto const edges{std::ranges::count_if(session->frame.hud_coverage, [](auto alpha){return alpha > 0 && alpha < 255;})};
      require(edges > 100, "integrated cockpit lacks antialiased HUD edges");
      if(command_line.capture.has_value() && dimensions[0] == 3840 && fov == 90) session->frame.write_ppm((std::filesystem::path{command_line.capture->string()} / "cockpit-hud.ppm").string());
    }
  }
  // The original pitch ladder is the same geometry, now stroked after native projection.
  {
    render::framebuffer native{3840, 2160};
    native.image.colours = session->frame.image.colours;
    native.clear(167);
    render::framebuffer lettering{640, 480};
    lettering.coverage.assign(lettering.image.pixels.size(), 0);
    render::helmet_instruments state{session->instruments->state.helmet};
    state.mode = 1;
    state.view = 0;
    state.colour = 223;
    state.roll = 0x12345678;
    render::display_layout const layout{3840, 2160, 640, 480, {0, 0, 639, 247}, 90};
    render::vector_hud vectors{native, layout.hud_transform({0, 0, 639, 247}, 90), state.colour};
    session->avionics.draw_helmet(lettering, state, &vectors);
    layout.composite_hud(lettering, native, {0, 0, 639, 247}, 90);
    if(command_line.capture.has_value()) native.write_ppm((std::filesystem::path{command_line.capture->string()} / "banked-hud.ppm").string());
  }
  std::cout << "Integrated HUD smoothing passed at native, fractional and 4K sizes with custom FOVs\n";

  auto tree{render::construct_model(session->runtime.models, 15, {{0, 0, 0}, apache::math::identity})};
  render::framebuffer image{3840, 2160};
  image.image.colours = session->frame.image.colours;
  // Keep textures alive for the renderer, even though these trees use untextured faces.
  auto const textures{resources::read_object_textures(data.read("3DDATA/OBJTEXT.BIN"))};
  render::model_renderer trees{image, session->runtime.models, textures, session->world.palette};
  int near_width{0};
  int far_width{0};
  for(int const distance : {70000, 300000}) {
    apache::math::pose const camera{{0, -distance, 2048}, apache::math::identity};
    trees.style.thick_lines = false;
    image.clear(208);
    trees.draw(tree, camera);
    auto const before{image.image.pixels};
    render::projection const projection{image};
    auto const centre{projection.prepare({-20480, distance - 16384, -1024}).screen};
    uint8_t const colour{before[size_t(centre.y) * image.image.width + centre.x]};
    require(colour != 208, "tree check must sample an actual trunk");
    trees.style.thick_lines = true;
    image.clear(208);
    trees.draw(tree, camera);
    auto const trunk_width{[&](auto const &pixels){
      int left{centre.x};
      int right{centre.x};
      auto const row{size_t(centre.y) * image.image.width};
      while(left > 0 && pixels[row + left - 1] == colour) --left;
      while(right + 1 < static_cast<int>(image.image.width) && pixels[row + right + 1] == colour) ++right;
      return right - left + 1;
    }};
    int const old_width{trunk_width(before)};
    int const new_width{trunk_width(image.image.pixels)};
    require(new_width > old_width, "4K tree trunk still renders as a single pixel");
    if(distance == 70000) near_width = new_width;
    else far_width = new_width;
    std::cout << "Tree at " << distance << " units: " << old_width << " -> " << new_width << " pixels\n";
    if(command_line.capture.has_value()) image.write_ppm((std::filesystem::path{command_line.capture->string()} / (distance == 70000 ? "trees-near.ppm" : "trees-far.ppm")).string());
  }
  require(near_width > far_width, "tree trunks do not grow thicker with proximity");
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << error.what() << '\n';
  return EXIT_FAILURE;
}
