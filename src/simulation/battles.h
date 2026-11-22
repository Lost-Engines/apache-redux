#pragma once
#include "actor_targeting.h"

namespace apache::simulation {
struct battle_side {
  std::optional<size_t> task;
  std::array<int32_t, 2> opponent_position{};
  int32_t initial{0};
  int32_t release_time{0};
};
struct battle_cell {
  bool active{false};
  std::array<battle_side, 2> sides;
};
std::array<battle_cell, 5> initialise_battles(resources::mission_definition const &mission, std::span<actor_group const> groups);
std::vector<size_t> task_actor_order(std::span<actor_group const> groups, size_t task);
int32_t battle_survival(actor_state const &leader, int32_t alive, int32_t initial);
struct battle_callbacks {
  std::function<int32_t(size_t, int32_t)> survival;
  std::function<void(size_t)> release;
  std::function<void(size_t)> combat;
  std::function<void(size_t, bool)> finish;
  std::function<int32_t()> random;
};
void update_battles(std::array<battle_cell, 5> &cells, int32_t now, battle_callbacks const &callbacks);
std::optional<size_t> random_task_survivor(std::span<size_t const> order, std::span<actor_state const> actors, int32_t alive, std::function<int32_t()> const &random);
struct battle_acquisition_context {
  int32_t frame{0};
  int32_t serial{0};
  bool active{false};
};
struct battle_acquisition_callbacks {
  std::function<shared_target()> select;
  std::function<math::vector3(target_reference const &)> position;
  std::function<int32_t(int32_t, int32_t)> height;
  std::function<shared_target(math::vector3 const &)> fixed;
};
void acquire_battle_target(actor_state &actor, actor_armament &armament, actor_type const &type, std::array<weapon_info, 55> const &weapons, math::trigonometry const &trig, uint32_t &global_seed, battle_acquisition_context const &context, battle_acquisition_callbacks const &callbacks);
struct battle_combat_callbacks {
  std::function<void(size_t)> acquire;
  std::function<void(size_t)> fire;
  std::function<int32_t(size_t)> deadline;
  std::function<void(size_t, int32_t)> set_deadline;
  std::function<int32_t()> random;
};
void update_battle_formation(std::span<size_t const> order, std::span<actor_state const> actors, std::span<actor_armament const> armaments, int32_t now, battle_combat_callbacks const &callbacks);
void release_battle_formation(std::span<size_t const> order, std::span<actor_armament> armaments);
} // namespace apache::simulation
