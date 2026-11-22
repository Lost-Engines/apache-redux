#include "battles.h"
#include <bit>
#include <stdexcept>

namespace apache::simulation {
std::array<battle_cell, 5> initialise_battles(resources::mission_definition const &mission, std::span<actor_group const> const groups) {
  std::array<battle_cell, 5> result{};
  for(size_t task{0}; task != mission.tasks.size(); ++task) {
    auto const &definition{mission.tasks[task]};
    if(definition.user || !definition.battle_cell) continue;
    if(definition.battle_cell < 1 || definition.battle_cell >= 5 || definition.side < 1 || definition.side > 2) throw std::runtime_error{"Invalid battle assignment"};
    auto &cell{result[static_cast<size_t>(definition.battle_cell)]};
    auto &side{cell.sides[static_cast<size_t>(definition.side - 1)]};
    if(side.task) throw std::runtime_error{"Duplicate battle task"};
    side.task = task;
    cell.active = true;
    for(auto const &group : groups) {
      if(group.task == task) {
        side.initial += static_cast<int32_t>(group.units.size());
        bool found{false};
        for(auto const &point : group.definition.waypoints) {
          if(point.type == 4) {
            side.opponent_position = {point.position[0], point.position[1]};
            found = true;
          }
        }
        if(!found) throw std::runtime_error{"Battle route has no attack waypoint"};
      }
    }
  }
  for(auto &cell : result) {
    if(cell.active) {
      if(!cell.sides[0].task || !cell.sides[1].task) throw std::runtime_error{"Unpaired battle task"};
      std::swap(cell.sides[0].opponent_position, cell.sides[1].opponent_position);
    }
  }
  return result;
}
std::vector<size_t> task_actor_order(std::span<actor_group const> const groups, size_t const task) {
  std::vector<size_t> result;
  for(auto const &group : groups) {
    if(group.task == task && !group.units.empty()) {
      result.push_back(group.units.front());
      for(size_t i{group.units.size()}; i-- > 1;) result.push_back(group.units[i]);
    }
  }
  return result;
}
int32_t battle_survival(actor_state const &leader, int32_t const alive, int32_t const initial) {
  if(!initial || (leader.classification != 6 && (leader.conditions & 6))) return -1;
  return static_cast<int32_t>(int64_t{std::bit_cast<int32_t>(static_cast<uint32_t>(alive) << 4)} / initial);
}
void update_battles(std::array<battle_cell, 5> &cells, int32_t const now, battle_callbacks const &callbacks) {
  bool allied_survives{true};
  for(size_t index{1}; index != cells.size(); ++index) {
    auto &cell{cells[index]};
    if(!cell.active || !cell.sides[0].task || !cell.sides[1].task) continue;
    auto &allied{cell.sides[0]};
    auto &enemy{cell.sides[1]};
    int32_t const a{callbacks.survival(*allied.task, allied.initial)};
    int32_t const b{callbacks.survival(*enemy.task, enemy.initial)};
    if(a == -1 || b == -1) continue;
    for(auto &side : cell.sides) {
      if(side.release_time < now) {
        callbacks.release(*side.task);
        side.release_time = math::add(now, callbacks.random() % 36000);
      }
    }
    callbacks.combat(*allied.task);
    callbacks.combat(*enemy.task);
    // This flag is initialized once per update, not once per battle cell.
    if(a < 5) {
      callbacks.finish(*allied.task, false);
      allied_survives = false;
    }
    if(b < 5) {
      callbacks.finish(*enemy.task, false);
      if(allied_survives) callbacks.finish(*allied.task, true);
    } else if(!allied_survives) callbacks.finish(*enemy.task, true);
  }
}
std::optional<size_t> random_task_survivor(std::span<size_t const> const order, std::span<actor_state const> const actors, int32_t const alive, std::function<int32_t()> const &random) {
  if(alive <= 0) return {};
  int32_t remaining{random() % alive + 1};
  for(auto id : order) {
    if(!actors[id].destroyed) --remaining;
    if(!remaining) return id;
  }
  return {};
}
void acquire_battle_target(actor_state &actor, actor_armament &armament, actor_type const &type, std::array<weapon_info, 55> const &weapons, math::trigonometry const &trig, uint32_t &global_seed, battle_acquisition_context const &context, battle_acquisition_callbacks const &callbacks) {
  if(armament.target || context.serial % 4 != context.frame % 4 || !(actor.conditions & 0x40) || !type.weapons[1] || !context.active) return;
  auto const target{callbacks.select ? callbacks.select() : nullptr};
  if(!target) return;
  auto position{callbacks.position(*target)};
  if(type.weapons[1] == 20) {
    int32_t const spread{weapons[20].movement_class == 3 ? std::bit_cast<int32_t>(static_cast<uint32_t>(weapons[20].tracking_or_dispersion) * 5u) : 0};
    if(spread <= 0) return;
    combat_random random{actor.random, global_seed};
    for(size_t i{0}; i != 2; ++i) {
      int32_t const first{math::negate(random.next()) % spread};
      int32_t const second{math::negate(random.next()) % spread};
      position[i] = math::add(position[i], math::subtract(first, second));
    }
    actor.random = random.value;
    global_seed = random.global_seed;
    position[2] = callbacks.height(position[0], position[1]);
    armament.target = callbacks.fixed(position);
  } else {
    armament.target = target;
  }
  actor.angles[0] = trig.arctangent(math::subtract(position[0], actor.object.world.position[0]), math::subtract(position[1], actor.object.world.position[1]));
  actor.object.world.rotation = trig.transformation(actor.angles[0], 0, 0);
}
void update_battle_formation(std::span<size_t const> const order, std::span<actor_state const> const actors, std::span<actor_armament const> const armaments, int32_t const now, battle_combat_callbacks const &callbacks) {
  for(auto id : order) {
    if(!actors[id].destroyed) {
      callbacks.acquire(id);
      if(armaments[id].target) {
        if(!callbacks.deadline(id)) callbacks.set_deadline(id, math::add(now, callbacks.random() % 2000));
        callbacks.fire(id);
      }
    }
  }
}
void release_battle_formation(std::span<size_t const> const order, std::span<actor_armament> const armaments) {
  for(auto id : order) armaments[id].target.reset();
}
} // namespace apache::simulation
