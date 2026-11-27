#include "capture_options.h"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include "frontend/mission_session.h"
#include "render/display_layout.h"

namespace {
namespace frontend = apache::frontend;
namespace render = apache::render;
namespace simulation = apache::simulation;
void require(bool const value, char const *const message) {
  if(!value) throw std::runtime_error{message};
}
void press(frontend::mission_session &session, uint16_t const key) {
  session.post(key);
  session.step(1);
}
void check_displays(frontend::mission_session &session, bool const gunner, uint32_t const seed) {
  auto state{session.instruments->state};
  state.damaged_display_seed = seed;
  bool const high{session.panels.video_mode() != 0};
  render::framebuffer layer{high ? 640u : 320u, high ? 480u : 240u};
  layer.coverage.assign(layer.image.pixels.size(), 0);
  auto const &actual{session.frame};
  render::framebuffer expected{actual.image.width, actual.image.height};
  expected.clear(208);
  auto const corners{render::hud_mfd_areas(actual.image.width, actual.image.height, gunner)};
  std::array const areas{session.avionics.mfd_area(gunner ? render::mfd_panel::gunner_left : render::mfd_panel::pilot_left),
    session.avionics.mfd_area(gunner ? render::mfd_panel::gunner_right : render::mfd_panel::pilot_right), session.avionics.mfd_area(render::mfd_panel::gunner_middle)};
  render::target_preview_renderer const preview{[&](render::rectangle const source)->std::string_view {
    expected.clip = corners[source.left == areas[2].left ? 2 : source.left == areas[1].left ? 1 : 0];
    expected.horizontal_fov = 90;
    if(state.target_model) return session.world.draw_flir(expected, *state.target_model, state.target_camera_aim, state.helmet.aircraft.position, session.visual.lighting);
    return "No Target";
  }};
  session.avionics.draw_mfds(layer, state, gunner, session.world, session.visual.lighting, preview);
  for(size_t side{0}; side != (gunner ? 3u : 2u); ++side) {
    render::composite_region(layer, areas[side], expected, corners[side]);
    auto const box{corners[side]};
    for(int y{box.top}; y <= box.bottom; ++y) {
      for(int x{box.left}; x <= box.right; ++x) {
        size_t const at{size_t(y) * actual.image.width + x};
        require(actual.image.pixels[at] == expected.image.pixels[at], "HUD MFD differs from the live avionics renderer");
        require(actual.hud_coverage.empty() || !actual.hud_coverage[at], "HUD strokes draw over an MFD");
      }
    }
  }
  // The unobstructed screen edges must match the cockpit projection all the
  // way down to the bottom, including the newly exposed area below the HUD.
  auto const viewport{session.views->modes.viewport(session.panels.video_mode(), gunner ? 5 : 0)};
  render::rectangle const reference{viewport[0], viewport[1], viewport[0] + viewport[2] - 1, viewport[1] + viewport[3] - 1};
  auto const &graphics{session.launch_options().graphics};
  render::display_layout const cockpit{actual.image.width, actual.image.height, layer.image.width, layer.image.height, reference, graphics.field_of_view};
  expected.reset_clip();
  expected.horizontal_fov = graphics.field_of_view;
  expected.projection_reference = cockpit.scene;
  session.world.draw(expected, session.camera.camera, session.visual);
  for(unsigned int y{0}; y != actual.image.height; ++y) {
    size_t const left{size_t(y) * actual.image.width};
    size_t const right{left + actual.image.width - 1};
    require(actual.image.pixels[left] == expected.image.pixels[left] && actual.image.pixels[right] == expected.image.pixels[right], "MFD view moved the HUD without matching the cockpit horizon or left part of the world blank");
  }
}
} // anonymous namespace
int main(int argc, char **argv) try {
  auto const command_line{apache::testing::read_capture_options(argc, argv, "HUD MFD checks", "capture-directory", false)};
  if(command_line.help) return EXIT_SUCCESS;
  apache::resources::data_store data{command_line.directory};
  if(command_line.capture.has_value()) std::filesystem::create_directories(command_line.capture->string());
  for(unsigned int mode : {0u, 1u}) {
    frontend::mission_options options;
    options.video_mode = mode;
    options.detail = 4;
    auto session{std::make_unique<frontend::mission_session>(data, frontend::mission_session::quick_start(data, 3), options)};
    session->paused = true;
    press(*session, 0x23);
    require(session->views->modes.mode == 12 && session->hud_mfds == frontend::hud_mfd_mode::none, "first H must enter the plain helmet view");
    session->views->helmet.heading = 0x1234567;
    session->views->helmet.pitch = -0x2345678;
    auto const look{session->views->helmet};
    auto const crew{session->views->modes.crew};
    auto &pages{session->instruments->state.displays.pages};
    pages = {{{simulation::mfd_page::map, simulation::mfd_page::engines}, {simulation::mfd_page::weapons, simulation::mfd_page::tactical}}};
    auto const gunner_pages{pages[1]};
    press(*session, 0x23);
    require(session->hud_mfds == frontend::hud_mfd_mode::pilot, "second H must show pilot MFDs");
    press(*session, 0x1a);
    require(pages[0][0] != simulation::mfd_page::map && pages[1] == gunner_pages, "brackets must change the displayed pilot MFD only");
    press(*session, 0x201a);
    require(pages[0][0] == simulation::mfd_page::off, "Ctrl+bracket must turn a floating MFD off");
    press(*session, 0x201a);
    require(pages[0][0] != simulation::mfd_page::off, "Ctrl+bracket must turn a floating MFD on");
    auto const pilot_pages{pages[0]};
    press(*session, 0x23);
    require(session->hud_mfds == frontend::hud_mfd_mode::gunner, "third H must show gunner MFDs");
    press(*session, 0x401b);
    require(pages[1][1] != gunner_pages[1] && pages[0] == pilot_pages, "Shift+bracket must change the displayed gunner MFD only");
    press(*session, 0x23);
    require(session->hud_mfds == frontend::hud_mfd_mode::none && session->views->modes.mode == 12, "fourth H must return to the plain helmet view");
    require(session->views->helmet.heading == look.heading && session->views->helmet.pitch == look.pitch && session->views->modes.crew == crew, "MFD cycling reset the head direction or changed crew controls");
    press(*session, 0x23);
    press(*session, 0x3c);
    require(session->views->modes.mode == 0 && session->hud_mfds == frontend::hud_mfd_mode::none, "leaving H must reset the overlay selection");
    press(*session, 0x23);
    require(session->hud_mfds == frontend::hud_mfd_mode::none, "re-entering H must start with the plain helmet view");
    // Preserve a real moving-model target for native FLIR tests.
    auto const target{std::ranges::find_if(session->runtime.actors.units, [](auto const &unit){return unit.object.id == 3;})};
    require(target != session->runtime.actors.units.end(), "quickstart needs an A-10 target");
    size_t const target_index{static_cast<size_t>(target - session->runtime.actors.units.begin())};
    session->runtime.sensors->state.current = *session->runtime.spatial.targets[target_index];
    using enum simulation::mfd_page;
    std::array<std::array<simulation::mfd_page, 2>, 4> const displays{{{map, engines}, {weapons, status}, {radar, tactical}, {flir, flight_plan}}};
    std::array<apache::configuration::resolution, 4> const sizes{{{640, 480}, {1437, 911}, {800, 1000}, {3840, 2160}}};
    for(size_t i{0}; i != sizes.size(); ++i) {
      session->resize(sizes[i].width, sizes[i].height);
      auto graphics{options.graphics};
      graphics.field_of_view = i % 2 ? 150 : 60;
      session->configure_graphics(graphics);
      pages[0] = displays[i];
      pages[1] = {displays[i][1], displays[i][0]};
      for(bool const gunner : {false, true}) {
        press(*session, 0x23);
        require(session->hud_mfds == (gunner ? frontend::hud_mfd_mode::gunner : frontend::hud_mfd_mode::pilot), "H overlay order changed during resolution tests");
        auto const seed{session->instruments->state.damaged_display_seed};
        session->draw();
        check_displays(*session, gunner, seed);
        if(command_line.capture.has_value() && i == 3 && mode == 1) session->frame.write_ppm((std::filesystem::path{command_line.capture->string()} / (gunner ? "hud-gunner.ppm" : "hud-pilot.ppm")).string());
      }
      press(*session, 0x23);
      session->draw();
    }
    // Both seats retain their original failure and off states in the overlays.
    session->runtime.pilot->set_faults(0x190);
    pages[0] = {map, off};
    pages[1] = {weapons, tactical};
    for(bool const gunner : {false, true}) {
      press(*session, 0x23);
      auto const seed{session->instruments->state.damaged_display_seed};
      session->draw();
      check_displays(*session, gunner, seed);
      require(pages[gunner][gunner ? 1 : 0] == damaged, "floating MFD ignored its seat's damage bit");
    }
    std::cout << (mode ? "640" : "320") << " artwork: H cycle, shared controls, pages, native previews, cockpit horizon, resolution/FOV and damage passed\n";
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << error.what() << '\n';
  return EXIT_FAILURE;
}
