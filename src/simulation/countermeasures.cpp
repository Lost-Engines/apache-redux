#include "countermeasures.h"
#include <algorithm>
#include <stdexcept>

namespace apache::simulation {
namespace {
size_t slot(unsigned int const weapon) {
  if(weapon != 7 && weapon != 8) throw std::runtime_error{"Invalid countermeasure weapon"};
  return weapon - 7;
}
int32_t &deadline(countermeasure_actor &actor, unsigned int weapon) {
  return weapon == 7 ? actor.chaff_at : actor.flare_at;
}
int32_t delay(countermeasure_actor &actor, weapon_info const &weapon, forces_info const &skill, uint32_t &seed) {
  combat_random random{actor.random, seed}; auto const result{countermeasure_delay(weapon, skill, random)};
  actor.random = random.value; seed = random.global_seed; return result;
}
bool emit(countermeasure_actor &actor, unsigned int weapon, int32_t now, forces_info const &skill, uint32_t &seed, countermeasure_callbacks const &callbacks) {
  if(!callbacks.launch || !callbacks.launch(weapon)) return false;
  static auto const definitions{weapon_definitions()};
  auto const &definition{definitions.at(weapon)};
  deadline(actor, weapon) = math::add(now, actor.player ? definition.fire_interval_ticks : delay(actor, definition, skill, seed));
  return true;
}
} // anonymous namespace
int32_t countermeasure_delay(weapon_info const &weapon, forces_info const &skill, combat_random &random) {
  auto const product{std::bit_cast<int32_t>(static_cast<uint32_t>(weapon.fire_interval_ticks) * static_cast<uint32_t>(skill.decoy_interval_percent))};
  // get_drone_random_ returns the negated old value; weapon and round RNGs do not.
  int32_t const interval{product / 100}, sample{math::negate(random.next())};
  return math::add(math::add(interval, 100), interval > 0 ? sample % interval : 0);
}
bool release_player_countermeasure(countermeasure_actor &actor, countermeasure_controls &controls, unsigned int const weapon, uint16_t const key, int32_t const now, forces_info const &skill, uint32_t &random_seed, countermeasure_callbacks const &callbacks) {
  auto const index{slot(weapon)};
  if(controls.mode == 0 || (controls.mode == 1 && callbacks.key_event && callbacks.key_event(key))) return false;
  if((controls.faults & 0x10000) || controls.deadline[index] > now || controls.ammunition[index] <= 0) return false;
  if(!emit(actor, weapon, now, skill, random_seed, callbacks)) return false;
  if(!controls.infinite) { controls.ammunition[index] = math::subtract(controls.ammunition[index], 1); controls.pylon_ammunition[index] = math::subtract(controls.pylon_ammunition[index], 1); }
  static auto const weapons{weapon_definitions()};
  controls.deadline[index] = math::add(now, weapons[weapon].fire_interval_ticks / (weapon == 8 ? 2 : 1));
  if(callbacks.released) callbacks.released(weapon);
  return true;
}
bool request_countermeasure(countermeasure_actor &actor, countermeasure_controls &controls, unsigned int const weapon, int32_t const now, forces_info const &skill, uint32_t &random_seed, countermeasure_callbacks const &callbacks) {
  slot(weapon);
  if(actor.classification != 1 && actor.classification != 2) return false;
  if(actor.player) return release_player_countermeasure(actor, controls, weapon, 0, now, skill, random_seed, callbacks);
  if(deadline(actor, weapon) <= now) return emit(actor, weapon, now, skill, random_seed, callbacks);
  static auto const weapons{weapon_definitions()};
  deadline(actor, weapon) = math::add(now, delay(actor, weapons[weapon], skill, random_seed));
  return false;
}
bool notify_incoming_missile(countermeasure_actor &actor, countermeasure_controls &controls, weapon_info const &missile, math::vector3 const &position, int32_t const speed, int32_t const now, forces_info const &skill, uint32_t &random_seed, countermeasure_callbacks const &callbacks) {
  // The original divides by zero for a stationary missile. It cannot supply an arrival estimate.
  if(speed == 0) return false;
  math::vector3 distance;
  for(size_t i{0}; i != 3; ++i) { auto const d{math::subtract(position[i], actor.position[i])}; distance[i] = d < 0 ? math::negate(d) : d; }
  std::ranges::sort(distance);
  auto const range{math::add(math::add(distance[2], distance[1] >> 2), distance[0] >> 2)};
  int32_t const scaled{std::bit_cast<int32_t>(static_cast<uint32_t>(range) * 200u)};
  int32_t const arrival{scaled == INT32_MIN && speed == -1 ? INT32_MIN : scaled / speed};
  if(actor.player) {
    if(callbacks.warning) callbacks.warning(arrival);
    if(!controls.automatic) return false;
  }
  bool released{false};
  if(arrival < 1201 && (missile.guidance == 2 || missile.guidance == 3)) released = request_countermeasure(actor, controls, missile.guidance == 2 ? 7 : 8, now, skill, random_seed, callbacks);
  if(released && !(actor.conditions & 2)) { actor.conditions |= 2; actor.retreat_until = math::add(now, 100); }
  return released;
}
} // namespace apache::simulation
