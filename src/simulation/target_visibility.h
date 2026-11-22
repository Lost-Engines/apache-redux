#pragma once
#include "collisions.h"
#include "terrain_visibility.h"

namespace apache::simulation {
enum class target_kind : unsigned int {
  none,
  scenery,
  mobile,
  fixed
};
struct target_identity {
  target_kind kind{target_kind::none};
  size_t layer{0};
  uint32_t index{0};
};
struct target_reference {
  target_identity identity;
  math::vector3 position{};
  mutable int32_t classification{0};
};
bool line_reaches_target(math::vector3 const &start, math::vector3 const &end, target_identity const &source, target_identity const &target, resources::terrain const &terrain, scenery_collisions const &scenery, render::mobile_scene const &mobiles, resources::model_archive const &models, math::trigonometry const &trig);
} // namespace apache::simulation
