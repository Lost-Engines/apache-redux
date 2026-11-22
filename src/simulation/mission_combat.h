#pragma once
#include "actor_reactions.h"
#include "battles.h"
#include "combat_air_patrol.h"
#include "mission_targets.h"

namespace apache::simulation {
struct mission_combat_callbacks {
  std::function<bool(target_reference const &, target_reference const &)> visible;
  std::function<void(size_t)> fire;
  std::function<firing_status(target_reference const &, target_reference const &, unsigned int)> ground_fire;
  std::function<void(unsigned int, bool)> sound;
  std::function<void(std::string_view)> message;
};
class mission_combat {
  resources::mission_definition const &mission;
  mission_scenery &scenery;
  mission_actors &actors;
  mission_spatial &spatial;
  mission_targets &targets;
  resources::terrain const &terrain;
  math::trigonometry const &trig;
  combat_tables const &tables;
  std::span<std::vector<resources::formation_member> const> formations;
  actor_environment const &ground;
  combat_random &war_random, &weapon_random;
  uint32_t *previous_shared_seed;
  std::array<actor_type, 1315> types;
  std::array<weapon_info, 55> weapons;
  std::vector<size_t> group_indices;
  std::vector<std::vector<size_t>> task_order;
  std::vector<unsigned int> patrol_slots;
  mission_combat_callbacks events;
  int32_t now{0}, delta{0}, difficulty{0};
  uint32_t frame{0};
  actor_targeting_context context(size_t index) const;
  void acquire(size_t index, int mode);
  void patrol(size_t index, actor_targeting &local, actor_targeting_context const &context);
  void scan_patrol();
  patrol_radar_callbacks radar_callbacks() const;
  void notify(size_t receiver, shared_target const &source, bool fired);
  void update_statics();
  void update_ground_battles();
  void set_deadline(size_t index, int32_t value);
  int32_t deadline(size_t index);
public:
  std::vector<actor_armament> armaments;
  std::vector<actor_attention> attention;
  std::vector<int32_t> alive;
  std::array<battle_cell, 5> battles;
  patrol_contacts patrols;
  combat_warning_times warnings;
  int32_t bombing_message_time{0};
  math::vector3 player_position{};
  uint32_t player_heading{0};
  shared_target player_target;
  mission_combat(resources::mission_definition const &mission, mission_scenery &scenery, mission_actors &actors, mission_spatial &spatial, mission_targets &targets, resources::terrain const &terrain, math::trigonometry const &trig, combat_tables const &tables, std::span<std::vector<resources::formation_member> const> formations, actor_environment const &ground, combat_random &war, combat_random &weapon, int difficulty, mission_combat_callbacks callbacks);
  ~mission_combat();
  mission_combat(mission_combat const &) = delete;
  mission_combat &operator=(mission_combat const &) = delete;
  void set_time(int32_t clock, int32_t step, uint32_t frame_number);
  int32_t weapon(size_t index);
  void check_air_lock(size_t actor);
  std::optional<movement_target> movement_aim(size_t actor) const;
  void fire_at_waypoint(size_t actor);
  void notify_fired_at(target_reference const &receiver, shared_target const &source);
  void kill(size_t actor);
  void update();
};
} // namespace apache::simulation
