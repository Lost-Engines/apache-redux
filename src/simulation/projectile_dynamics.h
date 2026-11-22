#pragma once
#include "actor_movement.h"
#include "seeker.h"

namespace apache::simulation {
struct projectile_state {
  projectile_motion motion;
  math::pose pose;
  math::vector3 previous{};
  std::array<uint32_t, 3> angles{};
  seeker_state seeker;
};
struct projectile_step {
  math::vector3 displacement{};
  bool notify_target{false};
  std::optional<math::vector3> explosion;
};
struct closest_approach {
  int32_t distance{INT32_MAX};
  math::vector3 position{};
};
closest_approach closest_point(math::vector3 const &start, math::vector3 const &end, math::vector3 const &target, math::trigonometry const &trig);
void pitch_projectile_down(projectile_state &round, math::trigonometry const &trig, int32_t delta);
projectile_step calculate_projectile_move(projectile_state &round, weapon_info const &weapon, std::optional<movement_target> const &target, bool target_is_actor, math::trigonometry const &trig, int32_t delta);
struct terrain_intersection {
  bool hit{false};
  math::vector3 above{};
  math::vector3 below{};
};
terrain_intersection subdivide_terrain_intersection(math::vector3 const &start, math::vector3 const &end, std::function<int32_t(int32_t, int32_t)> const &height);
struct bounded_projectile_move {
  bool hit_terrain{false};
  math::vector3 end{};
  math::vector3 impact{};
  math::vector3 displacement{};
};
bounded_projectile_move bound_projectile_move(math::vector3 const &start, math::vector3 const &move, std::function<int32_t(int32_t, int32_t)> const &height);
} // namespace apache::simulation
