#pragma once
#include "mission_spatial.h"

namespace apache::simulation {
// Resolves target references against the same objects used by drawing and collision.
class mission_targets {
  resources::mission_definition const &mission;
  mission_scenery const &scenery;
  mission_actors const &actors;
  mission_spatial const &spatial;
  combat_random &random;
  sector_search_inputs sector(int32_t x, int32_t y, int32_t side) const;
public:
  int32_t now{0};
  std::function<mobile_target_state(uint32_t)> additional_state;
  target_world world;
  mission_targets(resources::mission_definition const &mission, mission_scenery const &scenery, mission_actors const &actors, mission_spatial const &spatial, combat_random &random, resources::terrain const &terrain, scenery_collisions const &structures, render::mobile_scene const &mobiles, resources::model_archive const &models, math::trigonometry const &trig);
  mission_targets(mission_targets const &) = delete;
  mission_targets &operator=(mission_targets const &) = delete;
  mobile_target_state mobile_state(uint32_t id) const;
  target_description describe(target_reference const &target) const {
    return world.describe(target, now);
  }
  shared_target in_sector(int32_t x, int32_t y, int32_t side, math::vector3 const &origin, search_kind kind, bool randomised = true);
  shared_target search(math::vector3 const &origin, int32_t side, int32_t minimum, int32_t maximum, search_kind kind, bool randomised = true);
};
} // namespace apache::simulation
