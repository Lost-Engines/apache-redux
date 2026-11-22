#include "actor_reactions.h"
#include <algorithm>

namespace apache::simulation {
namespace {
int32_t next_war(uint32_t &state, combat_random &weapon) {
  combat_random random{state, weapon.global_seed, weapon.shared_seed}; auto const value{random.next()}; state = random.value; weapon.global_seed = random.global_seed; return value;
}
int32_t distance2(math::vector3 const &a, math::vector3 const &b) {
  int32_t x{math::subtract(a[0], b[0])}, y{math::subtract(a[1], b[1])};
  if(x < 0) x = math::negate(x);
  if(y < 0) y = math::negate(y);
  return std::bit_cast<int32_t>(static_cast<uint32_t>(std::max(x, y)) * 4u + static_cast<uint32_t>(std::min(x, y))) >> 2;
}
} // anonymous namespace
bool acquire_attacker(actor_state &actor, actor_armament &armament, actor_type const &type, std::array<weapon_info, 55> const &weapons, uint32_t &war_random, combat_random &weapon_random, actor_reaction_context const &context, actor_reaction_callbacks const &callbacks) {
  if(!(actor.conditions & 0x40) || (actor.conditions & 0x10) || !context.reference) return false;
  if(static_cast<int32_t>((actor.object.status >> 1) & 3) == context.attacker_side) return false;
  switch(context.task) {
    case 10: case 11: case 16: case 20: return false;
    case 13: case 14: case 18: if(actor.ammunition[1] > 0) return false; break;
    default: break;
  }
  if(armament.target) {
    switch(context.task) {
      case 1: case 2: case 3: case 6: case 7: case 13: case 14: case 15: if(context.attacker.classification != 2) return false; break;
      case 4: case 5: case 8: case 9: case 12: case 17: if(context.attacker.classification != 1) return false; break;
      default: return false;
    }
  }
  bool const ground{context.attacker.classification == 1};
  auto const distance{distance2(context.attacker.position, actor.object.world.position)};
  if(distance < (ground ? armament.ground_minimum : armament.air_minimum) || distance > (ground ? armament.ground_maximum : armament.air_maximum)) return false;
  if(callbacks.visible && !callbacks.visible()) return false;
  release_actor_target(actor, armament); if(callbacks.target_released) callbacks.target_released();
  armament.target = context.reference;
  actor.retreat_until = math::add(math::add(context.now, 400), next_war(war_random, weapon_random) % 2000);
  if(actor.classification == 1 || actor.classification == 2) actor.conditions |= 8;
  actor.conditions = (actor.conditions | 0x10) & ~2u;
  if(choose_actor_weapon(actor, armament, type, weapons, context.attacker, context.task) && actor.weapon_slot >= 0 && actor.weapon_slot < 5) armament.deadlines[static_cast<size_t>(actor.weapon_slot)] = math::add(context.now, 400);
  return true;
}
void notify_actor_fired_at(actor_state &actor, actor_armament &armament, actor_type const &type, std::array<weapon_info, 55> const &weapons, combat_warning_times &warnings, uint32_t &war_random, combat_random &weapon_random, actor_reaction_context const &context, actor_reaction_callbacks const &callbacks) {
  if(context.player) {
    if(context.attacker.classification == 1 && weapon_random.next() % 50 < 2 && callbacks.sound) callbacks.sound(0x20, true);
    return;
  }
  if(actor.classification == 7) {
    if((actor.object.status & 6) == 2 && math::subtract(context.now, warnings.ships_attacked) > 60000 && weapon_random.next() % 10 < 2) {
      if(callbacks.sound) callbacks.sound(0x69, false);
      warnings.ships_attacked = context.now;
    }
  } else if(!(actor.conditions & 2) && next_war(war_random, weapon_random) % ((actor.conditions & 0x10) ? 15 : 5) == 0) {
    actor.conditions |= 2; actor.retreat_until = math::add(context.now, next_war(war_random, weapon_random) % 200);
  }
  acquire_attacker(actor, armament, type, weapons, war_random, weapon_random, context, callbacks);
}
void notify_actor_targeted(actor_state &actor, actor_armament &armament, actor_type const &type, std::array<weapon_info, 55> const &weapons, combat_warning_times &warnings, uint32_t &war_random, combat_random &weapon_random, math::trigonometry const &trig, actor_reaction_context const &context, actor_reaction_callbacks const &callbacks) {
  if(!context.player) {
    if(!armament.target) acquire_attacker(actor, armament, type, weapons, war_random, weapon_random, context, callbacks);
    if(weapon_random.next() % 10 != 0) return;
  }
  if((actor.object.status & 6) != 2) return;
  if(context.attacker.classification == 1) {
    if(!context.player || context.now < warnings.radar || weapon_random.next() % 10 > 2) return;
    warnings.radar = math::add(context.now, 6000);
    switch(context.attacker_model) {
      case 54: case 99: case 211: if(callbacks.sound) callbacks.sound(0x22, context.player); break;
      case 80: case 84: case 87: case 203: if(callbacks.sound) callbacks.sound(0x21, context.player); break;
      default: break;
    }
  } else if(context.attacker.classification == 2) {
    if(context.now < warnings.bandit || weapon_random.next() % 10 > 2) return;
    warnings.bandit = math::add(context.now, 2000);
    uint32_t const bearing{trig.arctangent(math::subtract(context.attacker.position[0], context.view_position[0]), math::subtract(context.attacker.position[1], context.view_position[1])) - context.view_heading};
    uint32_t const clock{bearing / 0x15555555u};
    if(clock < 1 || clock > 12) return;
    unsigned int const sound{0x25u + (clock - 1) * 2 + (context.attacker.position[2] <= context.view_position[2] ? 1u : 0u)};
    if(callbacks.sound) callbacks.sound(sound, context.player);
  }
}
bool kill_actor(actor_state &actor, actor_armament &armament, int32_t &task_alive, std::span<actor_state *> const followers, std::function<void()> const &unplace, std::function<void()> const &released) {
  if(actor.destroyed == 1) return false;
  if(!(actor.object.status & 1) && unplace) unplace();
  actor.destroyed = 1; actor.object.status |= 1; task_alive = math::subtract(task_alive, 1); actor.speed = 0;
  if(!(actor.conditions & 0x200)) {
    actor.conditions |= 0x40000;
    for(auto *follower : followers) { follower->speed = 0; follower->conditions |= 0x40000; }
  }
  release_actor_target(actor, armament); if(released) released();
  return true;
}
} // namespace apache::simulation
