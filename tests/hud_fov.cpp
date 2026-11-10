#include "frontend/mission_session.h"
#include "render/display_layout.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool value, char const *message) { if(!value) throw std::runtime_error{message}; }
apache::render::rectangle ink(apache::render::framebuffer const &image, uint8_t colour) {
  apache::render::rectangle bounds{static_cast<int>(image.image.width),static_cast<int>(image.image.height),-1,-1};
  for(unsigned int y{0}; y < image.image.height; ++y) for(unsigned int x{0}; x < image.image.width; ++x)
    if(image.image.pixels[size_t{y}*image.image.width+x] == colour) {
      bounds.left=std::min(bounds.left,static_cast<int>(x)); bounds.right=std::max(bounds.right,static_cast<int>(x));
      bounds.top=std::min(bounds.top,static_cast<int>(y)); bounds.bottom=std::max(bounds.bottom,static_cast<int>(y));
    }
  require(bounds.right >= bounds.left, "HUD component disappeared");
  return bounds;
}
}
int main(int argc, char **argv) try {
  if(argc < 2 || argc > 3) throw std::runtime_error{"Usage: hud_fov_check DATA_DIRECTORY [CAPTURE_DIRECTORY]"};
  using namespace apache;
  resources::data_store data{argv[1]};
  frontend::mission_options options; options.detail=0; options.invulnerable=true; options.no_crashes=true;
  auto session{std::make_unique<frontend::mission_session>(data,frontend::mission_session::quick_start(data,3),options)};
  session->step(10);
  for(bool paused : {false,true}) {
    session->paused=paused;
    auto const time{session->runtime.timing.now};
    auto const position{session->runtime.pilot->readings().position.position};
    session->post(1); session->post(0x15); session->step(10);
    require(session->quit_prompt && !session->finished, "Escape must open confirmation without consuming a queued Yes");
    session->draw(); session->step(10);
    require(session->runtime.timing.now == time && session->runtime.pilot->readings().position.position == position, "confirmation advanced the simulation");
    session->post(1); session->step(10); session->draw();
    require(!session->quit_prompt && !session->finished && session->paused == paused, "Escape must cancel and retain the pause state");
    require(session->runtime.timing.now == time, "cancelling confirmation advanced the simulation");
    session->post(1); session->step(10); session->post(0x31); session->step(10);
    require(!session->quit_prompt && !session->finished, "No must cancel confirmation");
    session->post(0x2001); session->step(10); session->post(0x31); session->step(10);
    require(!session->quit_prompt && !session->finished, "legacy Ctrl+Escape stopped working");
  }
  session->post(1); session->step(10); session->post(0x15); session->step(10);
  require(session->finished, "Yes must end the sortie");
  std::cout << "Escape opens, cancels and confirms without advancing flight\n";

  auto const &executable{session->runtime.executable}; auto const &trig{session->runtime.trig};
  for(unsigned int mode : {0u,1u}) {
    unsigned int const width{mode ? 640u : 320u}, height{mode ? 480u : 240u};
    render::helmet_display helmet{executable,trig,mode};
    simulation::view_modes views{executable};
    for(unsigned int view : {0u,2u,5u,12u,13u}) {
      views.mode=view; auto const raw{views.viewport(mode)};
      render::rectangle const viewport{raw[0],raw[1],raw[0]+raw[2]-1,raw[1]+raw[3]-1};
      render::helmet_instruments state; state.view=view; state.mode=1; state.weapons.selected=2;
      state.weapons.armed=true; state.weapons.box_half_size=simulation::weapon_definitions()[2].target_box_half_size;
      render::framebuffer layer{width,height}; layer.coverage.assign(layer.image.pixels.size(),0);
      helmet.draw_component(layer,state,render::helmet_component::targeting_box);
      for(auto size : {configuration::resolution{640,480},{1437,911},{3840,2160}}) {
        render::display_layout reference{size.width,size.height,width,height,viewport,90};
        render::framebuffer original{size.width,size.height}; reference.composite(layer,original);
        auto const before{ink(original,state.colour)};
        for(double fov : {60.,120.,150.}) {
          render::display_layout layout{size.width,size.height,width,height,viewport,fov};
          render::framebuffer image{size.width,size.height}; layout.composite_hud(layer,image,viewport,fov);
          auto const after{ink(image,state.colour)};
          double const ratio{render::projection{0,0,640,480,fov}.lateral_scale()};
          double const expected{(before.right-before.left+1)*ratio};
          require(std::abs((after.right-after.left+1)-expected) <= 5, "acquisition box does not scale with the FOV tangent");
          double const centre{size.width/2.0};
          require(std::abs((after.left+after.right)/2.0-centre) <= 12, "acquisition box moved away from the viewing axis");
        }
      }
    }
  }
  std::cout << "Acquisition boxes follow FOV in both artwork modes through 4K\n";

  std::vector<uint8_t> panel_pixels;
  std::optional<std::array<int32_t,4>> reference_target;
  for(double fov : {90.,120.,150.}) {
    options.detail=4; options.graphics.field_of_view=fov;
    auto flight{std::make_unique<frontend::mission_session>(data,frontend::mission_session::quick_start(data,3),options)};
    flight->step(10); flight->paused=true; flight->resize(1920,1080);
    flight->runtime.player_armament->inventory.current=2;
    flight->runtime.player_armament->inventory.helmet_mode=1;
    flight->runtime.player_armament->inventory.armed=true;
    flight->draw();
    auto const &weapon{flight->instruments->state.helmet.weapons};
    require(weapon.target_exists && weapon.target_valid, "comparison scene must have a visible target");
    std::array<int32_t,4> const target{weapon.target.x,weapon.target.y,static_cast<int32_t>(weapon.lock_status),weapon.target_range};
    if(reference_target) require(target == *reference_target, "FOV changed the reference target coordinates or acquisition state");
    else reference_target=target;
    render::display_layout const layout{1920,1080,640,480,{0,0,639,247},fov};
    std::vector<uint8_t> panel;
    for(int y{layout.scene.bottom+1}; y <= layout.artwork.bottom; ++y) {
      auto const start{flight->frame.image.pixels.begin()+ptrdiff_t(y)*1920+layout.artwork.left};
      panel.insert(panel.end(),start,start+layout.artwork.right-layout.artwork.left+1);
    }
    if(panel_pixels.empty()) panel_pixels=panel;
    else require(panel == panel_pixels, "changing FOV changed cockpit artwork or panel instruments");
    if(argc == 3) {
      std::filesystem::create_directories(argv[2]);
      resources::write_ppm((std::filesystem::path{argv[2]}/("hud-"+std::to_string(static_cast<int>(fov))+".ppm")).string(),flight->frame.image);
    }
  }
  std::cout << "Full cockpit panel pixels stay identical across FOV changes\n";
  return EXIT_SUCCESS;
} catch(std::exception const &error) { std::cerr << error.what() << '\n'; return EXIT_FAILURE; }
