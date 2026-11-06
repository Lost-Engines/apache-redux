#pragma once
#include "mission_combat.h"
#include "countermeasures.h"
#include "projectile_tracking.h"

namespace apache::simulation {
struct mission_weapon_callbacks {
  std::function<void(size_t, projectile_record const &)> launched;
  std::function<void(size_t, projectile_record &, projectile_event_info const &)> event;
  std::function<void()> bomb_released;
  std::function<void(size_t, unsigned int, int32_t)> incoming;
  std::function<bool(size_t)> grounded;
};
struct actor_decoys {
  std::array<int32_t, 2> deadlines{};
  std::array<shared_target, 2> targets{std::make_shared<target_reference>(), std::make_shared<target_reference>()};
};
class mission_weapons {
  resources::mission_definition const &mission;
  mission_actors &actors;
  mission_spatial &spatial;
  mission_targets &targets;
  mission_combat &combat;
  resources::model_archive const &models;
  resources::terrain const &terrain;
  scenery_collisions const &scenery;
  render::mobile_scene &mobiles;
  math::trigonometry const &trig;
  combat_tables const &tables;
  combat_random &war_random, &weapon_random;
  int difficulty;
  std::array<actor_type, 1315> types;
  std::array<weapon_info, 55> weapons;
  std::function<mobile_target_state(uint32_t)> previous_state;
  mission_weapon_callbacks events;
  std::optional<size_t> construct(projectile_request const &request);
  void launch(size_t index);
  void event(size_t index, projectile_record &round, projectile_event_info const &event);
  bool grounded(size_t actor) const;
  bool decoy(size_t actor, unsigned int weapon);
  void incoming(size_t index, projectile_record &round);
public:
  tracking_environment environment;
  bool night{false};
  effects visuals;
  projectiles rounds;
  std::vector<actor_decoys> decoys;
  countermeasure_controls player_countermeasures;
  std::function<bool(uint16_t)> player_key_event;
  std::function<void(unsigned int)> countermeasure_released;
  bool release_countermeasure(unsigned int weapon, uint16_t key);
  mission_weapons(resources::mission_definition const &mission, mission_actors &actors, mission_spatial &spatial, mission_targets &targets, mission_combat &combat, resources::model_archive const &models, resources::terrain const &terrain, scenery_collisions const &scenery, render::mobile_scene &mobiles, math::trigonometry const &trig, combat_tables const &tables, combat_random &war, combat_random &weapon, int difficulty, mission_weapon_callbacks events = {});
  ~mission_weapons();
  mission_weapons(mission_weapons const &) = delete;
  mission_weapons &operator=(mission_weapons const &) = delete;
  std::optional<size_t> launch_player(projectile_request const &request, bool untracked);
  bool player_line_of_sight(target_reference const &source, target_reference const &target) const;
  bool fire(size_t actor);
  bool fire_remote(size_t actor, unsigned int weapon, uint32_t seed, math::vector3 const &gun_direction, std::optional<target_reference> const &target = {});
  firing_status fire_ground(target_reference const &source, target_reference const &target, unsigned int weapon);
  shared_target target_slot(uint32_t mobile, simulation::target_slot slot) const;
  mobile_target_state state(uint32_t mobile) const;
  std::optional<size_t> projectile_index(uint32_t mobile) const;
  void update(projectile_time const &time);
};
} // namespace apache::simulation
