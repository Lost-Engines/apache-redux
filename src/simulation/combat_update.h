#pragma once
#include "actor_targeting.h"

namespace apache::simulation {
struct ground_defence {
  math::vector3 position{};
  uint32_t heading{0};
  int32_t deadline{0};
  int32_t shots{0};
  shared_target self;
  shared_target target;
};
struct ground_defence_callbacks {
  std::function<math::vector3(target_reference const &)> position;
  std::function<int32_t(target_reference const &)> status;
  std::function<int32_t(target_reference const &)> side;
  std::function<shared_target(math::vector3 const &, int32_t, int32_t, int32_t)> search;
  std::function<void(target_reference const &, uint16_t)> heading;
  std::function<firing_status(target_reference const &, target_reference const &, unsigned int)> shoot;
};
void point_ground_defence(ground_defence &object, math::trigonometry const &trig, ground_defence_callbacks const &callbacks);
void acquire_ground_defence_target(ground_defence &object, weapon_info const &weapon, int32_t now, uint32_t frame, forces_info const &skill, forces_info const &friendly_skill, combat_random &random, math::trigonometry const &trig, ground_defence_callbacks const &callbacks);
void fire_ground_defence(ground_defence &object, unsigned int weapon_id, weapon_info const &weapon, int32_t now, forces_info const &skill, combat_random &random, math::trigonometry const &trig, ground_defence_callbacks const &callbacks);
void visit_ground_defence_sectors(math::vector3 const &view, std::function<int32_t(int32_t, int32_t)> const &owner, std::function<void(int32_t, int32_t)> const &visit);
struct actor_combat_callbacks {
  std::function<int32_t(target_reference const &)> status;
  std::function<math::vector3(target_reference const &)> position;
  std::function<void()> air_acquire;
  std::function<void()> ground_acquire;
  std::function<void()> artillery_acquire;
  std::function<void()> fire;
  std::function<void()> released;
  std::function<unsigned int()> weapon;
  std::function<bool()> behind;
};
// 1 means acquisition ran without finding a target, allowing the formation scan to stop.
int update_actor_combat(actor_state &actor, actor_armament &armament, int32_t task, int32_t serial, uint32_t frame, std::array<weapon_info, 55> const &weapons, actor_combat_callbacks const &callbacks);
void update_actor_formations(std::span<actor_group const> groups, std::span<actor_state const> actors, std::span<resources::mission_task const> tasks, std::optional<size_t> player, std::function<int(size_t)> const &update);
} // namespace apache::simulation
