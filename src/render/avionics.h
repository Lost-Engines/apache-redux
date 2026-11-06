#pragma once
#include "helmet_display.h"
#include "mfd.h"
#include "panel_instruments.h"
#include "simulation/mfd_state.h"

namespace apache::render {
struct navigation_instruments { uint32_t heading; int32_t airspeed; };
struct avionics_instruments {
  helmet_instruments helmet;
  int32_t clock_time{0};
  std::optional<navigation_instruments> navigation;
  unsigned int accelerated_time{1};
  std::array<std::string, 3> communications;
  simulation::mfd_state displays;
  uint32_t damaged_display_seed{1};
  engine_instruments engines;
  weapon_instruments stores;
  radar_instruments radar;
  radar_instruments tactical;
  std::vector<radar_contact> radar_contacts;
  std::vector<radar_contact> tactical_contacts;
  std::vector<waypoint_mark> route;
  size_t waypoint{0};
  std::optional<multiplayer_scores> scores;
  int32_t delta{0};
  int32_t map_range{0x400000};
  model_instance *target_model{nullptr};
  math::vector3 target_camera_aim{0, 0, 0};
};
class avionics_renderer {
  math::trigonometry const &trig;
  panel_instruments instruments;
  mfd_renderer screens;
  helmet_display helmet;
  void multifunction(framebuffer &target, avionics_instruments &state, bool gunner, landscape &world, sky_lighting const &lighting, target_preview_renderer const &preview = {}) const;
public:
  avionics_renderer(cockpit const &panels, math::trigonometry const &trig, resources::executable_data const &executable);
  void draw(framebuffer &target, avionics_instruments &state, landscape &world, sky_lighting const &lighting, target_preview_renderer const &preview = {}) const;
};
} // namespace apache::render
