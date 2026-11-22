#pragma once
#include "actor_types.h"
#include "resources/executable.h"
#include "weapon_info.h"

namespace apache::simulation {
struct forces_info {
  int32_t acquire_ticks{0};
  int32_t damage_percent{0};
  int32_t range_percent{0};
  int32_t decoy_interval_percent{0};
  int32_t fire_interval_percent{0};
  int32_t burst_size_percent{0};
  int32_t decoy_modifier{0};
};
class combat_tables {
  std::array<double, 71> densities;
  std::array<forces_info, 12> forces;
public:
  explicit combat_tables(resources::executable_data const &executable);
  double density(int32_t altitude) const;
  forces_info const &skill(int level) const;
};
struct projectile_motion {
  int32_t speed{0};
  int32_t distance{0};
  int32_t age{0};
  int32_t altitude{0};
  uint32_t flags{1};
};
bool update_projectile_speed(projectile_motion &round, weapon_info const &weapon, double frame_time, combat_tables const &tables);
bool projectile_expired(projectile_motion const &round, weapon_info const &weapon);
struct damage_context {
  int32_t range{0};
  int32_t armour{0};
  int32_t radius_override{0};
  int32_t damage_override{0};
  bool alternate_armour{false};
  bool enemy_source{false};
  int32_t difficulty_percent{100};
  int32_t mode{0};
  int32_t role_random{0};
  int32_t round_random{0};
};
int32_t projectile_damage(unsigned int weapon_id, weapon_info const &weapon, damage_context const &context);
struct weapon_selection {
  uint32_t flags{0};
  int32_t task{0};
  int32_t range{0};
  int32_t air_minimum{0};
  int32_t air_maximum{0};
  std::array<int32_t, 5> ammunition{};
  int32_t weapon{0};
  int32_t slot{0};
};
void select_ground_weapon(weapon_selection &state, actor_type const &actor, std::array<weapon_info, 55> const &weapons);
void select_air_weapon(weapon_selection &state, actor_type const &actor, std::array<weapon_info, 55> const &weapons);
struct fire_conditions {
  bool has_target{false};
  uint32_t actor_flags{0};
  int32_t weapon_id{0};
  int32_t target_kind{0};
  int32_t task{0};
  int32_t ammunition{0};
  int32_t deadline{0};
  int32_t now{0};
  int32_t weather{0};
};
bool can_fire(fire_conditions const &conditions, weapon_info const &weapon);
struct combat_random {
  uint32_t value{1};
  uint32_t global_seed{1};
  uint32_t *shared_seed{nullptr};
  int32_t next();
};
int32_t firing_interval(weapon_info const &weapon, int32_t ammunition, forces_info const &skill, combat_random &random);
int32_t acquisition_delay(bool friendly, forces_info const &skill, forces_info const &friendly_skill, combat_random &random);
} // namespace apache::simulation
