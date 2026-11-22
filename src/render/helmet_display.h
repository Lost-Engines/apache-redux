#pragma once
#include <string>
#include "math/geometry.h"
#include "math/trigonometry.h"
#include "instrument_font.h"
#include "projection.h"
#include "projection2.h"

namespace apache::render {
enum class helmet_component {
  reticle,
  heading,
  pitch,
  bank,
  altitude,
  waypoint,
  airspeed,
  engines,
  viewing_device,
  field_of_view,
  layout,
  aiming_box,
  impact,
  targeting_box,
  rocket_sight,
  intercept,
  lock_diamond,
  point_direction,
  weapons,
  targeting,
  tads_box
};
struct helmet_weapon_instruments {
  unsigned int selected{1};
  std::string name{"M230"};
  int32_t rounds{0};
  int32_t impact_time{-1};
  unsigned int lock_status{0};
  bool armed{false};
  bool landed{false};
  bool target_exists{false};
  bool target_locked{false};
  bool target_designated{false};
  bool target_aircraft{false};
  bool target_valid{false};
  vertex2 target;
  vertex2 impact;
  vertex2 intercept;
  math::vector3 target_position{0, 0, 0};
  math::vector3 impact_position{0, 0, 0};
  math::vector3 target_heading{0, INT32_MAX, 0};
  std::string target_name{"Target"};
  int32_t target_range{0};
  int32_t target_speed{0};
  int32_t box_half_size{0};
  int32_t rocket_elevation{0};
  bool tads_active{false};
  bool tads_valid{false};
  vertex2 tads;
};
struct helmet_instruments {
  unsigned int view{0};
  unsigned int mode{0};
  uint8_t colour{223};
  uint32_t faults{0};
  uint32_t heading{0};
  uint32_t command_heading{0};
  uint32_t pitch{0};
  uint32_t roll{0};
  int32_t climb_rate{0};
  int32_t radar_altitude{0};
  int32_t barometric_altitude{0};
  int32_t airspeed{0};
  int32_t torque{0};
  uint8_t waypoint_letter{'A'};
  int32_t waypoint_distance{0};
  int32_t waypoint_time{0};
  std::string viewing_device{"FLIR"};
  std::string magnification{"x1"};
  int32_t sight_heading{0};
  int32_t sight_pitch{0};
  bool layout_grid{false};
  math::pose aircraft;
  math::pose camera;
  helmet_weapon_instruments weapons;
};
class helmet_display {
  instrument_font font;
  math::trigonometry const &trig;
  bool high;
  std::array<rectangle, 20> viewports;
  rectangle sight_viewport;
public:
  helmet_display(resources::executable_data const &executable, math::trigonometry const &trig, unsigned int video_mode);
  void draw(framebuffer &target, helmet_instruments const &state) const;
  vertex2 unproject_point(unsigned int view, int32_t x, int32_t y) const;
  std::optional<vertex2> project_point(unsigned int view, math::pose const &camera, projection const &scene, math::vector3 const &position) const;
  void draw_component(framebuffer &target, helmet_instruments const &state, helmet_component component) const;
};
} // namespace apache::render
