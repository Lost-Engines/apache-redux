#include "combat_update.h"
#include <algorithm>
#include <stdexcept>

namespace apache::simulation {
namespace {
int32_t range3(math::vector3 const &a, math::vector3 const &b) {
  math::vector3 v;
  for(size_t i{0}; i != 3; ++i) {
    v[i] = math::subtract(a[i], b[i]);
    if(v[i] < 0) v[i] = math::negate(v[i]);
  }
  std::ranges::sort(v);
  return math::add(math::add(v[2], v[1] >> 2), v[0] >> 2);
}
int32_t range2(math::vector3 const &a, math::vector3 const &b) {
  int32_t x{math::subtract(a[0], b[0])};
  int32_t y{math::subtract(a[1], b[1])};
  if(x < 0) x = math::negate(x);
  if(y < 0) y = math::negate(y);
  return std::bit_cast<int32_t>(static_cast<uint32_t>(std::max(x, y)) * 4u + static_cast<uint32_t>(std::min(x, y))) >> 2;
}
}
void point_ground_defence(ground_defence &object, math::trigonometry const &trig, ground_defence_callbacks const &callbacks) {
  if(!object.self || !object.target) return;
  auto const source{callbacks.position(*object.self)};
  auto const target{callbacks.position(*object.target)};
  object.heading = trig.arctangent(math::subtract(target[0], source[0]), math::subtract(target[1], source[1]));
  callbacks.heading(*object.self, static_cast<uint16_t>(object.heading >> 16));
}
void acquire_ground_defence_target(ground_defence &object, weapon_info const &weapon, int32_t const now, uint32_t const frame, forces_info const &skill, forces_info const &friendly_skill, combat_random &random, math::trigonometry const &trig, ground_defence_callbacks const &callbacks) {
  if(object.target) {
    if(callbacks.status(*object.target) == 0 && range3(object.position, callbacks.position(*object.target)) <= weapon.maximum_range) return;
    object.target.reset();
  }
  // The original uses the address modulo four; all 36-byte records are four-byte aligned.
  if(frame % 4 || !object.self) return;
  int32_t const side{callbacks.side(*object.self)};
  if(side != 1 && side != 2) return;
  object.target = callbacks.search(object.position, 3 - side, weapon.minimum_range, std::min(weapon.maximum_range, 0x528000));
  if(object.target) {
    object.deadline = math::add(now, acquisition_delay(side == 1, skill, friendly_skill, random));
    point_ground_defence(object, trig, callbacks);
  }
}
void fire_ground_defence(ground_defence &object, unsigned int const weapon_id, weapon_info const &weapon, int32_t const now, forces_info const &skill, combat_random &random, math::trigonometry const &trig, ground_defence_callbacks const &callbacks) {
  if(!object.target || !object.self) return;
  point_ground_defence(object, trig, callbacks);
  if(now < object.deadline) return;
  if(weapon.weapon_role != 1 && weapon.weapon_role != 2) throw std::runtime_error{"Unsupported static air-defence weapon"};
  auto const result{callbacks.shoot(*object.self, *object.target, weapon_id)};
  if(result == firing_status::blocked) return;
  if(result == firing_status::lost_target) {
    object.target.reset();
    return;
  }
  object.shots = math::add(object.shots, 1);
  object.deadline = math::add(now, firing_interval(weapon, object.shots, skill, random));
}
void visit_ground_defence_sectors(math::vector3 const &view, std::function<int32_t(int32_t, int32_t)> const &owner, std::function<void(int32_t, int32_t)> const &visit) {
  int32_t const x{view[0] >> 22};
  int32_t const y{view[1] >> 22};
  for(int32_t i{std::max(0, x - 2)}; i < std::min(63, x + 2); ++i) {
    for(int32_t j{std::max(0, y - 2)}; j < std::min(63, y + 2); ++j) {
      auto const side{owner(i, j)};
      if(side == 1 || side == 2) visit(i, j);
    }
  }
}
int update_actor_combat(actor_state &actor, actor_armament &armament, int32_t const task, int32_t const serial, uint32_t const frame, std::array<weapon_info, 55> const &weapons, actor_combat_callbacks const &callbacks) {
  auto const side{actor.object.status & 6};
  if(!(actor.conditions & 0x40) || actor.destroyed || (side != 2 && side != 4)) return 0;
  if(armament.target && callbacks.status(*armament.target) == 1) {
    release_actor_target(actor, armament);
    if(callbacks.released) callbacks.released();
    return 0;
  }
  if(serial % 4 == static_cast<int32_t>(frame % 4)) {
    switch(actor.classification) {
    case 1:
    case 2:
      if(!actor.object.world.position[2]) return 0;
      callbacks.air_acquire();
      break;
    case 3:
    case 4:
    case 7:
    case 8:
      callbacks.ground_acquire();
      break;
    case 5:
      callbacks.artillery_acquire();
      break;
    case 6:
      return 0;
    default:
      throw std::runtime_error{"Invalid combat actor class"};
    }
    if(!armament.target) return 1;
  } else if(!armament.target) return 0;
  if(actor.classification == 1 || actor.classification == 2) {
    if((task == 13 || task == 14) && !(actor.conditions & 0x20)) {
      auto const &weapon{weapons.at(callbacks.weapon())};
      if(weapon.weapon_role != 6) return 2;
      if(!weapon.maximum_speed) return 2;
      int32_t const envelope{std::bit_cast<int32_t>(static_cast<uint32_t>(actor.object.world.position[2] / weapon.maximum_speed) * static_cast<uint32_t>(actor.speed))};
      if(range2(actor.object.world.position, callbacks.position(*armament.target)) <= envelope) {
        callbacks.fire();
        if(callbacks.behind()) actor.conditions |= 0x20;
        if(task == 14) {
          actor.retreat_until = 0;
          actor.conditions &= ~8u;
        }
        return 2;
      }
    }
    if((task != 18 || (actor.conditions & 0x20)) && !(actor.conditions & 8)) return 2;
  } else if(actor.classification == 6) return 0;
  callbacks.fire();
  return 2;
}
void update_actor_formations(std::span<actor_group const> const groups, std::span<actor_state const> const actors, std::span<resources::mission_task const> const tasks, std::optional<size_t> const player, std::function<int(size_t)> const &update) {
  for(auto const &group : groups) {
    if(tasks[group.task].type == 20 || group.units.empty()) continue;
    size_t const leader{group.units.front()};
    auto const &actor{actors[leader]};
    if(player == leader || actor.network != -1 || !(actor.conditions & 0x40000)) continue;
    if(update(leader) == 1) continue;
    for(size_t i{group.units.size()}; i-- > 1;) {
      if(update(group.units[i]) == 1) break;
    }
  }
}
} // namespace apache::simulation
