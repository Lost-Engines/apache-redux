#pragma once
#include "projectiles.h"
#include "targets.h"
#include "terrain_visibility.h"

namespace apache::simulation {
using target_slot_lookup = std::function<std::shared_ptr<target_reference>(uint32_t, target_slot)>;
struct tracking_environment {
  int32_t now{0};
  int32_t weather{0};
  int32_t cloud_bottom{0};
  int32_t cloud_top{0};
  int32_t mode{0};
  int32_t decoy_modifier{0};
};
projectile_target_binding selected_projectile_target(projectile_record &round, weapon_info const &weapon, target_world const &world, target_slot_lookup const &slots);
target_reference *adjusted_projectile_target(projectile_record &round, target_world const &world, target_slot_lookup const &slots);
bool projectile_has_target(projectile_record &round, weapon_info const &weapon, target_world const &world, target_slot_lookup const &slots, int32_t now);
projectile_aim track_projectile(projectile_record &round, weapon_info const &weapon, target_world const &world, target_slot_lookup const &slots, resources::terrain const &terrain, tracking_environment const &environment, uint32_t &random_seed);
} // namespace apache::simulation
