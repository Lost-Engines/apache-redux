#pragma once
#include <functional>
#include "cockpit.h"
#include "instrument_font.h"
#include "math/geometry.h"
#include "math/trigonometry.h"
#include "projection2.h"

namespace apache::render {
class landscape;
struct model_instance;
struct sky_lighting;
enum class mfd_panel { pilot_left, pilot_right, gunner_left, gunner_right, gunner_middle };
struct engine_instruments {
  double torque_left{0};
  double torque_right{0};
  double temperature_left{0};
  double temperature_right{0};
  double rpm_left{0};
  double rpm_right{0};
  double rotor_rpm{0};
  double collective{0};
};
struct weapon_pair {
  unsigned int type{0};
  std::array<unsigned int, 2> rounds{0, 0};
};
struct weapon_instruments {
  std::array<weapon_pair, 2> pairs;
  std::array<unsigned int, 2> wingtip{0, 0};
  unsigned int gun{0};
  unsigned int chaff{0};
  unsigned int flares{0};
  bool armed{false};
  bool auto_chaff{false};
  bool auto_ecm{false};
};
enum class radar_mode { inactive, ground, air, tactical };
struct radar_contact {
  int32_t x{0};
  int32_t y{0};
  unsigned int symbol{0};
  bool selected{false};
};
struct radar_instruments {
  radar_mode mode{radar_mode::inactive};
  int32_t range{0x76ac00};
  int32_t corner_range{0xa7d3ce};
  int32_t mark_length{0x3f4aa};
  uint32_t heading{0};
  bool failed{false};
  bool frozen{false};
  bool tads{false};
  bool locked{false};
  bool scan_all{false};
  unsigned int threat{4};
  unsigned int range_index{2};
  bool has_target{false};
  std::string target_name;
  int32_t target_distance{0};
};
struct waypoint_mark {
  int32_t x{0};
  int32_t y{0};
  uint8_t label{0};
};
struct multiplayer_scores {
  std::vector<std::pair<std::string, int32_t>> players; // Local player first, then connection order.
  int32_t timer{0};
};
using target_preview_renderer = std::function<std::string_view(rectangle)>;
class mfd_renderer {
  cockpit const &panels;
  instrument_font font;
  bool high;
public:
  mfd_renderer(cockpit const &panels, resources::executable_data const &executable);
  rectangle area(mfd_panel panel) const;
  void corners(framebuffer &target, bool gunner) const;
  void off(framebuffer &target, rectangle area) const;
  void damaged(framebuffer &target, rectangle area, uint32_t &seed) const;
  void engines(framebuffer &target, rectangle area, engine_instruments const &state) const;
  void flir(framebuffer &target, rectangle area, landscape &world, model_instance *object, math::vector3 const &aim, math::vector3 const &observer, sky_lighting const &lighting, target_preview_renderer const &preview = {}) const;
  void weapons(framebuffer &target, rectangle area, weapon_instruments const &state) const;
  void radar(framebuffer &target, rectangle area, radar_instruments const &state, std::span<radar_contact const> contacts, math::trigonometry const &trig) const;
  void flight_plan(framebuffer &target, rectangle area, std::span<waypoint_mark const> route, size_t current, int32_t x, int32_t y, uint32_t heading, int32_t airspeed, math::trigonometry const &trig) const;
  void scores(framebuffer &target, rectangle area, multiplayer_scores &state, int32_t delta) const;
  void moving_map(framebuffer &target, rectangle area, landscape const &world, int32_t x, int32_t y, uint32_t heading, int32_t range) const;
  void status(framebuffer &target, rectangle area, uint32_t faults, bool ground_radar, bool air_radar) const;
};
} // namespace apache::render
