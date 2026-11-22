#pragma once
#include "actor_movement.h"

namespace apache::simulation {
struct weapon_angles {
  int32_t heading{0};
  int32_t minimum_pitch{0};
  int32_t maximum_pitch{0};
};
weapon_angles firing_angles(weapon_info const &weapon);
std::array<int32_t, 2> relative_aim_angles(math::vector3 const &aim, math::vector3 const &forward, math::trigonometry const &trig);
struct firing_inputs {
  movement_target source;
  movement_target target;
  int32_t source_height{0};
  bool night{false};
};
enum class firing_status : int {
  lost_target = -1,
  blocked = 0,
  ready = 1
};
struct firing_solution {
  firing_status status{firing_status::blocked};
  math::pose pose;
};
firing_solution aim_weapon(weapon_info const &weapon, firing_inputs const &input, math::trigonometry const &trig, std::function<bool(math::vector3 const &, math::vector3 const &)> const &visible);
math::vector3 ballistic_aim(weapon_info const &weapon, math::vector3 const &source, movement_target const &target, bool moving_mobile);
std::optional<math::pose> laser_bomb_aim(math::pose const &source, math::vector3 const &target, weapon_info const &weapon, math::trigonometry const &trig);
} // namespace apache::simulation
