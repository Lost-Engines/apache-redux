#include "avionics.h"
#include <stdexcept>
#include "landscape.h"

namespace apache::render {
avionics_renderer::avionics_renderer(cockpit const &panels, math::trigonometry const &angles, resources::executable_data const &executable) : trig{angles}, instruments{panels, angles, executable}, screens{panels, executable}, helmet{executable, angles, panels.video_mode()} {}
void avionics_renderer::multifunction(framebuffer &target, avionics_instruments &state, bool const gunner, landscape &world, sky_lighting const &lighting, target_preview_renderer const &preview) const {
  auto const &flight{state.helmet};
  auto const navigation{state.navigation.value_or(navigation_instruments{flight.heading, flight.airspeed})};
  state.displays.apply_faults(gunner, flight.faults);
  auto const draw_target{[&](rectangle const area){ screens.flir(target, area, world, state.target_model, state.target_camera_aim, flight.aircraft.position, lighting, preview); }};
  for(size_t side{0}; side != 2; ++side) {
    auto const area{screens.area(static_cast<mfd_panel>((gunner ? 2 : 0) + side))};
    using enum simulation::mfd_page;
    switch(state.displays.pages[gunner][side]) {
    case off: screens.off(target, area); break;
    case damaged: screens.damaged(target, area, state.damaged_display_seed); break;
    case map: screens.moving_map(target, area, world, flight.aircraft.position[0], flight.aircraft.position[1], navigation.heading, state.map_range); break;
    case radar: screens.radar(target, area, state.radar, state.radar_contacts, trig); break;
    case tactical: screens.radar(target, area, state.tactical, state.tactical_contacts, trig); break;
    case flir: draw_target(area); break;
    case flight_plan: if(state.scores) screens.scores(target, area, *state.scores, state.delta); else screens.flight_plan(target, area, state.route, state.waypoint, flight.aircraft.position[0], flight.aircraft.position[1], navigation.heading, navigation.airspeed, trig); break;
    case weapons: screens.weapons(target, area, state.stores); break;
    case engines: screens.engines(target, area, state.engines); break;
    case status: screens.status(target, area, flight.faults, state.radar.mode == radar_mode::ground, state.radar.mode == radar_mode::air); break;
    }
  }
  if(gunner) {
    auto const area{screens.area(mfd_panel::gunner_middle)};
    if(flight.faults & 0x100) screens.damaged(target, area, state.damaged_display_seed);
    else draw_target(area);
  }
  screens.corners(target, gunner);
}
void avionics_renderer::draw_helmet(framebuffer &target, helmet_instruments const &state) const {
  if(state.view < 10 || state.view == 12 || state.view == 13 || state.view == 18) helmet.draw(target, state);
}
void avionics_renderer::draw(framebuffer &target, avionics_instruments &state, landscape &world, sky_lighting const &lighting, target_preview_renderer const &preview, bool const draw_hud) const {
  auto const &flight{state.helmet};
  switch(flight.view) {
  case 0:
    instruments.attitude(target, flight.aircraft.rotation); instruments.compass(target, flight.heading);
    instruments.airspeed(target, flight.airspeed); instruments.altimeter(target, flight.barometric_altitude);
    if(draw_hud) helmet.draw(target, flight);
    instruments.clock(target, state.clock_time); multifunction(target, state, false, world, lighting, preview);
    instruments.communications(target, false, state.communications); instruments.warnings(target, false, flight.faults);
    break;
  case 5:
    if(draw_hud) helmet.draw(target, flight);
    multifunction(target, state, true, world, lighting, preview);
    instruments.communications(target, true, state.communications); instruments.warnings(target, true, flight.faults);
    break;
  case 1: case 2: case 3: case 4: case 6: case 7: case 8: case 9: case 12: case 13: case 18:
    if(draw_hud) helmet.draw(target, flight);
    break;
  case 10: case 11: case 14: case 15: case 16: case 17: case 19: break;
  default: throw std::runtime_error{"Invalid avionics view"};
  }
  instruments.accelerated_time(target, state.accelerated_time);
}
} // namespace apache::render
