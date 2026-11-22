#include "weapon_rules.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <stdexcept>

namespace apache::simulation {
namespace {
int32_t integer(double const value) {
  if(!std::isfinite(value) || value < INT32_MIN || value > INT32_MAX) throw std::runtime_error{"Projectile speed exceeds integer range"};
  return static_cast<int32_t>(value);
}
}
combat_tables::combat_tables(resources::executable_data const &executable) {
  resources::reader density_data{executable.at(0x44c1d0, densities.size() * 8), "Projectile atmosphere"};
  for(auto &value : densities) {
    uint64_t const low{density_data.u32()};
    uint64_t const high{density_data.u32()};
    value = std::bit_cast<double>(low | (high << 32));
  }
  resources::reader forces_data{executable.at(0x402b70, forces.size() * 28), "Forces settings"};
  for(auto &value : forces) value = {forces_data.i32(), forces_data.i32(), forces_data.i32(), forces_data.i32(), forces_data.i32(), forces_data.i32(), forces_data.i32()};
}
double combat_tables::density(int32_t const altitude) const {
  return altitude < 1 ? 1.0 : densities[static_cast<size_t>(std::min(70, altitude / 256000))];
}
forces_info const &combat_tables::skill(int const level) const {
  if(level < -2 || level > 9) throw std::runtime_error{"Forces setting must be between -2 and 9"};
  return forces[static_cast<size_t>(level + 2)];
}
bool update_projectile_speed(projectile_motion &round, weapon_info const &weapon, double const frame_time, combat_tables const &tables) {
  bool remove_flame{false};
  if(round.flags & 2) {
    if(round.age > weapon.motor_ticks) {
      round.flags &= ~2u;
      remove_flame = true;
    }
    round.speed = std::min(integer(static_cast<double>(weapon.motor_acceleration) * frame_time + round.speed), weapon.maximum_speed);
  }
  if(weapon.drag_coefficient != 0) {
    double const speed{static_cast<double>(round.speed)};
    round.speed = integer(speed - (((tables.density(round.altitude) * weapon.drag_coefficient) * speed) * speed) * frame_time);
  }
  round.speed = std::max(0, round.speed);
  return remove_flame;
}
bool projectile_expired(projectile_motion const &round, weapon_info const &weapon) {
  return (round.speed < weapon.minimum_speed && !(round.flags & 2)) || static_cast<double>(round.distance) > weapon.maximum_range * 1.5;
}
int32_t projectile_damage(unsigned int const weapon_id, weapon_info const &weapon, damage_context const &context) {
  int32_t const radius{std::max(weapon.blast_radius, context.radius_override)};
  if(int64_t{radius} * 2 < context.range) return 0;
  int32_t penetration{(context.alternate_armour ? weapon.alternate_penetration : weapon.penetration) - context.armour};
  int32_t damage{std::max(static_cast<int32_t>(weapon.damage), context.damage_override)};
  if(weapon.weapon_role == 2) damage = std::max(0, context.role_random % (damage * 2 + 1) - damage);
  if(context.range > radius / 2) {
    damage /= 2;
    --penetration;
  }
  if(context.range > radius) {
    damage /= 2;
    --penetration;
  }
  if(penetration <= 0) damage >>= (0u - static_cast<uint32_t>(penetration)) & 31;
  if(context.mode == 1 || context.mode == 4 || context.mode == 5) return weapon_id == 1 || weapon_id == 19 ? context.round_random % 4 == 0 : context.round_random % 24 + 12;
  if(context.enemy_source) damage = static_cast<int32_t>(int64_t{damage} * context.difficulty_percent / 100);
  return damage;
}
void select_ground_weapon(weapon_selection &state, actor_type const &actor, std::array<weapon_info, 55> const &weapons) {
  state.weapon = 0;
  if(!(state.flags & 0x20000)) return;
  if(state.task == 13 || (state.task == 14 && !(state.flags & 0x20))) {
    state.slot = 1;
    state.weapon = actor.weapons[1];
    return;
  }
  if(!(state.flags & 0x80) && state.ammunition[1] > 0 && state.ammunition[0] > 0) {
    auto const &gun{weapons.at(static_cast<size_t>(actor.weapons[0]))};
    auto const &secondary{weapons.at(static_cast<size_t>(actor.weapons[1]))};
    state.slot = 0;
    if(gun.minimum_range < state.range && state.range < gun.maximum_range) {
      state.weapon = actor.weapons[0];
    } else if(secondary.minimum_range < state.range && state.range < secondary.maximum_range) {
      state.slot = 1;
      state.weapon = state.task == 18 ? 29 : actor.weapons[1];
    }
  } else {
    state.slot = state.ammunition[1] > 0 ? 1 : 0;
    state.weapon = actor.weapons[static_cast<size_t>(state.slot)];
  }
}
void select_air_weapon(weapon_selection &state, actor_type const &actor, std::array<weapon_info, 55> const &weapons) {
  state.slot = 0;
  state.weapon = 0;
  if(!(state.flags & 0x10000) || state.range < state.air_minimum || state.range > state.air_maximum) return;
  auto const in_range{[&](size_t slot){
    auto const &weapon{weapons.at(static_cast<size_t>(actor.weapons[slot]))};
    return weapon.minimum_range < state.range && state.range < weapon.maximum_range;
  }};
  for(size_t slot{5}; slot-- > 2;) {
    if(state.ammunition[slot] > 0 && in_range(slot)) state.slot = static_cast<int32_t>(slot);
  }
  if(state.ammunition[0] > 0 && in_range(0)) state.slot = 0;
  state.weapon = actor.weapons[static_cast<size_t>(state.slot)];
}
bool can_fire(fire_conditions const &c, weapon_info const &weapon) {
  return c.has_target && c.weapon_id && (c.actor_flags & 0x40) && c.deadline <= c.now && c.ammunition > 0 && c.target_kind && !(c.task == 15 && c.target_kind == 3) && !(weapon.weapon_role == 3 && (c.actor_flags & 0x8000)) && !(weapon.guidance == 3 && (c.weather == 2 || c.weather == 3));
}
int32_t combat_random::next() {
  auto const result{value};
  auto const first{value * 0x41c64e6du + 0x3039u};
  global_seed = first * 0x41c64e6du + 0x3039u;
  if(shared_seed) *shared_seed = global_seed;
  value = (first & 0x7fff0000u) | ((global_seed >> 16) & 0x7fff);
  return std::bit_cast<int32_t>(result);
}
int32_t firing_interval(weapon_info const &weapon, int32_t const ammunition, forces_info const &skill, combat_random &random) {
  int32_t interval{weapon.fire_interval_ticks};
  if(weapon.flags & 1) {
    int32_t const burst{std::max(1, weapon.burst_size * skill.burst_size_percent / 100)};
    if(ammunition % burst == 0) interval = weapon.burst_pause_ticks;
  }
  interval = static_cast<int32_t>(int64_t{interval} * skill.fire_interval_percent / 100);
  auto const sample{random.next()};
  // Invalid zero intervals otherwise divide by zero in the original.
  return interval < 2 ? std::max(0, interval) : interval + sample % (interval / 2);
}
int32_t acquisition_delay(bool const friendly, forces_info const &skill, forces_info const &friendly_skill, combat_random &random) {
  return (friendly ? friendly_skill : skill).acquire_ticks + random.next() % 1000;
}
} // namespace apache::simulation
