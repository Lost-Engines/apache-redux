#pragma once
#include "mission_actors.h"

namespace apache::simulation {
struct navigation_characteristics {
  int32_t turn_rate{0};
  int32_t maximum_bank{0};
  int32_t pitch_rate{0};
  int32_t acceleration{0};
  int32_t maximum_speed{0};
  int32_t minimum_speed{0};
  int32_t ground_attack_speed{0};
  int32_t air_attack_speed{0};
};
struct navigation_tables {
  std::array<navigation_characteristics, 5> aircraft;
  std::array<navigation_characteristics, 7> ships;
  explicit navigation_tables(resources::executable_data const &executable);
};
struct navigation_order {
  math::vector3 target{};
  int32_t waypoint_height{0};
  int32_t waypoint_speed{0};
  int32_t arrival_radius{0};
  std::optional<int32_t> previous_height;
  std::optional<int32_t> carrier_speed;
  bool has_target{false};
  bool player_task{false};
  bool arcade{false};
  bool has_waypoint{true};
  int32_t target_classification{0};
};
struct navigation_obstacle {
  math::vector3 position;
  int32_t height;
};
struct navigation_environment {
  std::function<int32_t(int32_t, int32_t)> height;
  int32_t tallest_object{0};
  std::function<std::optional<navigation_obstacle>(math::vector3 const &, math::vector3 const &)> obstacle;
  std::function<void(actor_state &, math::vector3 const &)> relocate;
};
struct navigation_result {
  bool arrived{false};
  bool outside_world{false};
};
navigation_result navigate_actor(actor_state &unit, navigation_order const &order, navigation_characteristics const &performance, math::trigonometry const &trig, navigation_environment const &world, int32_t delta_time);
} // namespace apache::simulation
