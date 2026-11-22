#pragma once
#include "blast_damage.h"
#include "combat_effects.h"
#include "mission_weapons.h"

namespace apache::simulation {
struct mission_damage_callbacks {
  std::function<void(target_identity const &, target_identity const &)> killed;
  std::function<void(damage_action, target_identity const &)> player;
  std::function<void(target_identity const &)> player_killed;
  std::function<void(unsigned int, bool)> sound;
  std::function<void(std::string_view)> message;
  std::function<void(int32_t)> score;
};
class mission_damage {
  resources::mission_definition const &mission;
  mission_scenery &scenery;
  mission_actors &actors;
  mission_spatial &spatial;
  mission_targets &targets;
  mission_combat &combat;
  mission_weapons &weapons;
  resources::model_archive const &models;
  scenery_collisions const &structures;
  render::mobile_scene &mobiles;
  combat_random &war_random, &weapon_random;
  mission_damage_callbacks events;
  damage_recipient read(target_identity const &id) const;
  void write(target_identity const &id, damage_recipient const &state);
  void kill(target_identity const &id, target_identity const &source);
  void partial(target_identity const &id);
  void auxiliary(target_identity const &id, projectile_record const &round);
public:
  damage_environment environment;
  int32_t friendly_fire{0};
  bool invulnerable{false};
  craters marks;
  combat_effects explosions;
  mission_damage(resources::mission_definition const &mission, mission_scenery &scenery, mission_actors &actors, mission_spatial &spatial, mission_targets &targets, mission_combat &combat, mission_weapons &weapons, resources::model_archive const &models, resources::terrain const &terrain, scenery_collisions const &structures, render::mobile_scene &mobiles, math::trigonometry const &trig, combat_random &war, combat_random &weapon, forces_info const &skill, std::function<uint8_t(int32_t, int32_t)> surface, mission_damage_callbacks events = {});
  mission_damage(mission_damage const &) = delete;
  mission_damage &operator=(mission_damage const &) = delete;
  void destroy(target_identity const &target);
  void impact(projectile_record &round, projectile_event_info const &event);
};
} // namespace apache::simulation
