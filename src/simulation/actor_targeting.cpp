#include "actor_targeting.h"
#include <algorithm>
#include <bit>
#include "targets.h"

namespace apache::simulation {
namespace {
int32_t range2(math::vector3 const &a, math::vector3 const &b) {
  int32_t x{math::subtract(a[0], b[0])}, y{math::subtract(a[1], b[1])};
  if(x < 0) x = math::negate(x);
  if(y < 0) y = math::negate(y);
  return math::add(std::max(x, y), std::min(x, y) >> 2);
}
}
int32_t attention_range(int32_t const task, int32_t const side, forces_info const &skill) {
  int32_t range{0};
  switch(task) {
    case 3: case 19: range = 0x906000; break;
    case 6: case 15: range = 0xce4000; break;
    case 1: case 2: case 4: case 5: case 7: case 8: case 9: case 12: case 13: case 14: case 17: case 18: case 21: range = 0x528000; break;
    default: break;
  }
  if(side == 2) range = std::bit_cast<int32_t>(static_cast<uint32_t>(range) * static_cast<uint32_t>(skill.range_percent)) / 100;
  return range;
}
bool target_behind_actor(actor_state const &actor, math::vector3 const &position) {
  return (math::subtract(position[0], actor.object.world.position[0]) < 0) != (actor.object.world.rotation[3] < 0)
    && (math::subtract(position[1], actor.object.world.position[1]) < 0) != (actor.object.world.rotation[4] < 0);
}
int32_t distance_from_flight_path(flight_path const &path, math::vector3 const &position, math::trigonometry const &trig) {
  if(!path.current || !path.previous) return INT32_MAX;
  // This caller supplies the target as the line start, unlike the collision callers.
  int32_t distance{perpendicular_distance(position, *path.previous, *path.current, trig)};
  if(distance == INT32_MAX && path.next) distance = perpendicular_distance(position, *path.current, *path.next, trig);
  if(distance == INT32_MAX) distance = range2(position, *path.current);
  return distance;
}
actor_targeting::actor_targeting(actor_state &unit, actor_armament &arms, actor_attention &state, actor_type const &header, std::array<weapon_info, 55> const &definitions, math::trigonometry const &angles, forces_info const &difficulty, forces_info const &friendly, combat_random &rng, int32_t &message_time, actor_targeting_context const &inputs, actor_targeting_callbacks const &events)
  : actor{unit}, armament{arms}, attention{state}, type{header}, weapons{definitions}, trig{angles}, skill{difficulty}, friendly_skill{friendly}, random{rng}, bombing_message_time{message_time}, context{inputs}, callbacks{events} {}
movement_target actor_targeting::describe(shared_target const &target) const {
  if(!target) return {};
  return callbacks.describe ? callbacks.describe(*target) : movement_target{target->position, {}, 0, target->classification};
}
int32_t actor_targeting::enemy_side() const {
  return (actor.object.status & 6) == 2 ? 2 : 1;
}
int32_t actor_targeting::choose() {
  return choose_actor_weapon(actor, armament, type, weapons, describe(armament.target), context.task);
}
int32_t actor_targeting::next_fire_time() {
  return choose() && actor.weapon_slot >= 0 && actor.weapon_slot < 5 ? armament.deadlines[static_cast<size_t>(actor.weapon_slot)] : INT32_MAX;
}
void actor_targeting::set_fire_time(int32_t const value) {
  if(choose() && actor.weapon_slot >= 0 && actor.weapon_slot < 5) armament.deadlines[static_cast<size_t>(actor.weapon_slot)] = value;
}
void actor_targeting::release() {
  release_actor_target(actor, armament);
  if(callbacks.released) callbacks.released();
}
void actor_targeting::delay_fire() {
  int32_t const delay{acquisition_delay(((actor.object.status & 7) >> 1) == 1, skill, friendly_skill, random)};
  set_fire_time(std::max(next_fire_time(), math::add(context.now, delay)));
}
bool actor_targeting::behind(shared_target const &target) const {
  return target_behind_actor(actor, describe(target).position);
}
shared_target actor_targeting::search_air() const {
  return callbacks.search ? callbacks.search(actor.object.world.position, enemy_side(), armament.air_minimum, std::min(armament.air_maximum, attention.range), search_kind::air) : nullptr;
}
shared_target actor_targeting::search_ground() const {
  if(!context.path.current || !callbacks.ground_sector) return nullptr;
  return callbacks.ground_sector((*context.path.current)[0] >> 22, (*context.path.current)[1] >> 22, enemy_side());
}
void actor_targeting::basic_aircraft() {
  if(armament.target || !armament.air_maximum) return;
  auto const target{search_air()};
  if(!target || distance_from_flight_path(context.path, describe(target).position, trig) > attention.range || (callbacks.visible && !callbacks.visible(*target))) {
    actor.conditions &= ~8u; return;
  }
  if(actor.classification == 1 && callbacks.actor_class && callbacks.actor_class(*target) != 1) return;
  actor.retreat_until = math::add(math::add(context.now, 12000), random.next() % 2000);
  actor.conditions = (actor.conditions | 8) & ~2u; armament.target = target;
}
void actor_targeting::bomber_transit() {
  if(context.now > actor.retreat_until) release();
  armament.target = search_air();
  if(armament.target && !behind(armament.target)) { actor.conditions &= ~8u; actor.retreat_until = math::add(context.now, 1000); }
}
void actor_targeting::strategic_bomber() {
  if(actor.ammunition[1] <= 0) {
    if(armament.target && describe(armament.target).classification == 1) release();
    basic_aircraft(); return;
  }
  if(context.path.type != 4) { bomber_transit(); return; }
  if(armament.target) { if(describe(armament.target).classification != 1) release(); return; }
  if(context.path.current && callbacks.fixed) {
    auto position{*context.path.current}; if(callbacks.height) position[2] = callbacks.height(position[0], position[1]);
    armament.target = callbacks.fixed(position);
  }
}
void actor_targeting::tactical_bomber() {
  if(actor.ammunition[1] <= 0) {
    if(armament.target && describe(armament.target).classification == 1) release();
    if(!armament.target) basic_aircraft();
    return;
  }
  if(context.path.type != 4) { bomber_transit(); return; }
  if(armament.target) { if(behind(armament.target) || describe(armament.target).classification != 1) release(); return; }
  armament.target = search_ground();
  if(armament.target && !behind(armament.target)) actor.conditions &= ~8u;
}
void actor_targeting::laser_bomber() {
  if(actor.ammunition[1] <= 0) {
    if(armament.target && describe(armament.target).classification == 1) { armament.target.reset(); release(); }
    basic_aircraft(); return;
  }
  if(armament.target) {
    if(armament.target->identity.kind == target_kind::none || behind(armament.target)) { armament.target.reset(); release(); }
    return;
  }
  if(context.path.type != 4) { actor.conditions &= ~0x800u; return; }
  auto const &target{context.player_target};
  if(target && target->identity.kind != target_kind::none && describe(target).classification == 1 && !behind(target)) {
    // The original Y comparison compares the register with itself: only X is tested.
    if(context.path.current && (describe(target).position[0] >> 22) == ((*context.path.current)[0] >> 22)) armament.target = target;
  } else if((actor.conditions & 0x800) && context.now > bombing_message_time) {
    if(callbacks.sound) callbacks.sound(0x48);
    if(callbacks.message) callbacks.message("Designate!");
    bombing_message_time = math::add(context.now, 1000);
  }
  if(!(actor.conditions & 0x800)) {
    if(callbacks.message) callbacks.message("Bombing Start");
    if(callbacks.sound) callbacks.sound(0x47);
    actor.conditions |= 0x800; bombing_message_time = context.now;
  }
}
void actor_targeting::ground_strike() {
  if(armament.target) { if(context.now >= actor.retreat_until) release(); return; }
  if(context.now < actor.retreat_until || (actor.ammunition[1] <= 0 && actor.ammunition[0] <= 0)) return;
  shared_target target;
  if(context.task == 4 || context.task == 7) {
    // The original uses the air maximum even for this ground attack search.
    if(callbacks.search) target = callbacks.search(actor.object.world.position, enemy_side(), armament.ground_minimum, std::min(armament.air_maximum, attention.range), search_kind::air_defence);
  } else if(context.path.type == 4) target = search_ground();
  if(!target || behind(target) || distance_from_flight_path(context.path, describe(target).position, trig) > attention.range / 4 || (callbacks.visible && !callbacks.visible(*target))) return;
  actor.retreat_until = math::add(context.now, 6000); actor.conditions = (actor.conditions | 8) & ~2u; armament.target = target;
}
void actor_targeting::aircraft() {
  if(context.now < actor.retreat_until) return;
  auto const old{armament.target};
  switch(context.task) {
    case 1: case 2: case 3: case 6: basic_aircraft(); break;
    case 4: case 5: ground_strike(); break;
    case 7: basic_aircraft(); if(!armament.target) ground_strike(); break;
    case 13: strategic_bomber(); break;
    case 14: tactical_bomber(); break;
    case 15: if(callbacks.cap) callbacks.cap(); break;
    case 18: laser_bomber(); break;
    default: return;
  }
  if(!armament.target || armament.target == old) return;
  if(!choose()) { release(); return; }
  if(armament.target->identity.kind == target_kind::mobile && callbacks.targeted) callbacks.targeted(*armament.target);
  delay_fire();
}
void actor_targeting::air_defence() {
  if(!armament.air_maximum) return;
  if(armament.target) {
    if(within_target_sectors(describe(armament.target).position, actor.object.world.position, static_cast<int32_t>(armament.air_maximum * 1.5))) return;
    release();
  }
  if(context.task == 19 && context.has_formation && context.radar_model && actor.destroyed == 1 && type.main_weapon < type.weapons.size() && weapons.at(static_cast<size_t>(type.weapons[type.main_weapon])).guidance == 2) return;
  auto const old{armament.target}; armament.target = search_air();
  if(!armament.target) return;
  if(armament.target != old && armament.target->identity.kind == target_kind::mobile && callbacks.targeted) callbacks.targeted(*armament.target);
  delay_fire();
}
void actor_targeting::artillery() {
  if(context.task == 10 && !armament.target && context.path.current && callbacks.fixed) {
    armament.target = callbacks.fixed(*context.path.current); set_fire_time(context.start_time);
  } else if(context.task == 16) {
    armament.target = search_ground();
    // The original tests the function address instead of calling its deadline getter.
  }
}
void actor_targeting::check_air_lock() {
  if(!armament.target || armament.target->identity.kind == target_kind::none) {
    if((actor.conditions & 2) && context.now > actor.retreat_until) actor.conditions &= ~2u;
    if(actor.network == -1) armament.target.reset();
    return;
  }
  int32_t allowed{attention.range};
  switch(context.task) {
    case 4: allowed /= 8; [[fallthrough]];
    case 13: case 14: case 18:
      if(describe(armament.target).classification != 2) { actor.conditions &= ~2u; return; }
      allowed /= 2; break;
    case 5: case 7: allowed /= 8; break;
    case 1: case 2: case 3: case 6: case 15: break;
    default: return;
  }
  if(actor.conditions & 8) {
    if(actor.conditions & 2) {
      if(context.now > actor.retreat_until) { actor.conditions &= ~2u; actor.retreat_until = math::add(context.now, 2000); }
      return;
    }
    if(context.now > actor.retreat_until) {
      int32_t const classification{describe(armament.target).classification};
      int32_t const maximum{classification == 1 ? armament.ground_maximum : classification == 2 ? armament.air_maximum : 0};
      if(within_target_sectors(describe(armament.target).position, actor.object.world.position, math::add(maximum, maximum))) release();
      else actor.conditions = (actor.conditions & ~10u) | 2;
      actor.retreat_until = math::add(math::add(context.now, 400), random.next() % 600); return;
    }
    int32_t const distance{range2(actor.object.world.position, describe(armament.target).position)};
    int32_t const classification{describe(armament.target).classification};
    int32_t const minimum{classification == 1 ? armament.ground_minimum : classification == 2 ? armament.air_minimum : 0};
    if(distance < std::max(math::add(actor.speed, actor.speed), minimum)) {
      actor.retreat_until = math::add(math::add(context.now, 100), random.next() % 400); actor.conditions |= 2; return;
    }
    if(int32_t{attention.damage} - attention.processed_damage > 10) {
      release(); actor.retreat_until = math::add(context.now, 600); actor.conditions |= 2; attention.processed_damage = attention.damage; return;
    }
    if(distance_from_flight_path(context.path, describe(armament.target).position, trig) > allowed) { release(); actor.retreat_until = math::add(context.now, 3000); }
  }
  if(context.now > actor.retreat_until) { actor.conditions = (actor.conditions | 8) & ~2u; actor.retreat_until = math::add(context.now, 3000); }
}
} // namespace apache::simulation
