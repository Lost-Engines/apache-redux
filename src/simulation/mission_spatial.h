#pragma once
#include <unordered_map>
#include "mission_radar.h"
#include "targets.h"

namespace apache::simulation {
// Owns the links between mission actors, render/collision objects and combat cells.
// The actors and scene must outlive this registry.
class mission_spatial {
  resources::mission_definition const &mission;
  mission_actors &actors;
  render::mobile_scene &mobiles;
  std::unordered_map<uint32_t, size_t> actor_indices;
  war_contact contact(size_t index) const;
  void placed(uint32_t id, size_t sector);
  void removed(uint32_t id, size_t sector);
public:
  war_grid grid;
  std::vector<shared_target> targets;
  std::optional<size_t> player;
  mission_spatial(resources::mission_definition const &mission, mission_scenery const &scenery, mission_actors &actors, render::mobile_scene &mobiles, resources::terrain const &terrain);
  ~mission_spatial();
  mission_spatial(mission_spatial const &) = delete;
  mission_spatial &operator=(mission_spatial const &) = delete;
  std::optional<size_t> actor_index(uint32_t id) const;
  uint32_t mobile_id(size_t actor) const {
    return targets.at(actor)->identity.index;
  }
  void move(actor_state &actor, math::vector3 const &position);
  void change_status(size_t actor, uint32_t status);
  void place(size_t actor);
  void unplace(size_t actor);
};
} // namespace apache::simulation
