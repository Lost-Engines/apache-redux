#pragma once
#include "damage.h"

namespace apache::simulation {
struct blast_callbacks {
  std::function<damage_recipient(target_identity const &)> read;
  std::function<void(target_identity const &, damage_recipient const &)> write;
  std::function<void(target_identity const &, damage_action)> action;
  std::function<void(target_identity const &)> partial_explosion;
  std::function<void(target_identity const &)> auxiliary;
  std::function<void(math::vector3 const &)> weapon_explosion;
};
class blast_damage {
  unsigned int weapon_id;
  weapon_info weapon;
  damage_source source;
  damage_environment environment;
  damage_random &random;
  resources::model_archive const &models;
  scenery_collisions const &scenery;
  render::mobile_scene const &mobiles;
  blast_callbacks callbacks;
  int32_t damage_one(target_identity const &target, math::vector3 const &impact);
  void damage_ground(math::vector3 const &impact, int32_t radius, std::optional<target_identity> ignored = {});
  void damage_mobiles(math::vector3 const &impact, int32_t radius, std::optional<target_identity> ignored = {});
public:
  blast_limits limits;
  blast_damage(unsigned int id, damage_source const &origin, damage_environment const &settings, damage_random &rng, resources::model_archive const &definitions, scenery_collisions const &structures, render::mobile_scene const &objects, blast_callbacks handlers);
  void detonate(math::vector3 const &impact);
  void hit(target_identity const &target, math::vector3 const &impact);
};
} // namespace apache::simulation
