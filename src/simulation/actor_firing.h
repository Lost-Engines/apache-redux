#pragma once
#include "firing_solution.h"
#include "projectiles.h"

namespace apache::simulation {
struct actor_armament {
  std::array<int32_t, 5> deadlines{};
  int32_t ground_maximum{0};
  int32_t ground_minimum{0};
  int32_t air_maximum{0};
  int32_t air_minimum{0};
  std::shared_ptr<target_reference> target;
  std::optional<size_t> round;
};
void refresh_weapon_ranges(actor_state &actor, actor_armament &armament, actor_type const &type, std::array<weapon_info, 55> const &weapons);
void refresh_weapon_supply(actor_state &actor, actor_armament &armament, actor_type const &type, std::array<weapon_info, 55> const &weapons);
int32_t choose_actor_weapon(actor_state &actor, actor_armament const &armament, actor_type const &type, std::array<weapon_info, 55> const &weapons, movement_target const &target, int32_t task);
void release_actor_target(actor_state &actor, actor_armament &armament);
struct actor_firing_context {
  int32_t now{0};
  int32_t weather{0};
  int32_t task{0};
  int32_t source_height{0};
  bool night{false};
  target_reference source;
  std::optional<target_reference> player;
  movement_target target;
};
struct actor_firing_callbacks {
  std::function<bool(math::vector3 const &, math::vector3 const &)> visible;
  std::function<std::optional<size_t>(projectile_request const &)> construct;
  std::function<void(size_t)> launch;
  std::function<void(size_t)> remove_flame;
  std::function<void()> bomb_released;
  std::function<void()> target_released;
  std::function<void(target_reference const &, target_reference const &)> fired_at;
};
bool fire_actor(actor_state &actor, actor_armament &armament, actor_type const &type, std::array<weapon_info, 55> const &weapons, math::trigonometry const &trig, forces_info const &skill, combat_random &random, actor_firing_context const &context, actor_firing_callbacks const &callbacks);
} // namespace apache::simulation
