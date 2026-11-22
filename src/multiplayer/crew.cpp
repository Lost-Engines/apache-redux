#include "crew.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <stdexcept>

namespace apache::multiplayer {
namespace {
int32_t integer(double value) {
  if(!std::isfinite(value)) return 0;
  return static_cast<int32_t>(std::clamp(value, -2147483648.0, 2147483647.0));
}
bool valid_role(crew_role role) {
  return role == crew_role::pilot || role == crew_role::gunner;
}
}
bool crew_state::notify_key(uint16_t const key, unsigned int const game_mode) {
  if(game_mode != 3) return false;
  if(keys.size() >= 10 || std::ranges::find(keys, key) != keys.end()) return true;
  keys.push_back(key); return false;
}
std::array<uint8_t, 104> encode_crew(crew_role const role, crew_state &state) {
  if(!valid_role(role) || state.keys.size() > 10 || state.message > 10 || state.gunner.target_kind > 2) throw std::runtime_error{"Invalid shared-cockpit state"};
  std::array<uint8_t, 104> result{};
  auto const put{[&](size_t offset, uint32_t value, size_t size = 4){ for(size_t i{0}; i < size; ++i) result[offset + i] = static_cast<uint8_t>(value >> (8 * i)); }};
  if(role == crew_role::pilot) {
    auto &pilot{state.pilot};
    for(size_t i{0}; i < 3; ++i) { put(i * 4, static_cast<uint32_t>(pilot.angles[i])); put(52 + i * 4, static_cast<uint32_t>(pilot.position[i])); }
    put(12, static_cast<uint32_t>(integer(pilot.flight_time))); put(16, static_cast<uint32_t>(pilot.climb));
    put(24, static_cast<uint32_t>(pilot.fuel), 2); put(26, static_cast<uint32_t>(pilot.weight), 2);
    result[34] = static_cast<uint8_t>(pilot.torque);
    for(size_t i{0}; i < 8; ++i) result[35 + i] = static_cast<uint8_t>(integer(pilot.engines[i]));
    result[45] = static_cast<uint8_t>(pilot.sidespeed); result[46] = static_cast<uint8_t>(pilot.airspeed);
    result[48] = pilot.frame_ticks; result[49] = pilot.landing; pilot.landing = 0;
    if(pilot.landed) result[50] |= 4;
  } else {
    auto const &gunner{state.gunner}; put(20, static_cast<uint32_t>(gunner.hydra_elevation));
    if(gunner.target_kind == 2) { put(28, static_cast<uint16_t>(gunner.drone), 2); put(30, static_cast<uint16_t>(gunner.task), 2); }
    for(size_t i{0}; i < 3; ++i) { put(52 + i * 4, static_cast<uint32_t>(gunner.display_position[i])); put(64 + i * 4, static_cast<uint32_t>(gunner.position[i])); }
    put(76, gunner.target_kind); put(80, gunner.target_source);
  }
  result[43] = state.death; result[44] = static_cast<uint8_t>(state.keys.size());
  result[47] = state.message; state.message = 0;
  if(state.leaving) result[50] |= 2;
  for(size_t i{0}; i < state.keys.size(); ++i) put(84 + i * 2, state.keys[i], 2);
  state.keys.clear(); return result;
}
std::optional<crew_state> decode_crew(crew_role const role, std::span<uint8_t const> const bytes) {
  if(!valid_role(role) || bytes.size() != 104 || bytes[44] > 10 || bytes[47] > 10) return {};
  auto const get{[&](size_t offset, size_t size = 4){ uint32_t value{0}; for(size_t i{0}; i < size; ++i) value |= uint32_t{bytes[offset + i]} << (8 * i); return value; }};
  auto const signed_word{[&](size_t offset){ return std::bit_cast<int32_t>(get(offset)); }};
  auto const signed_short{[&](size_t offset){ return std::bit_cast<int16_t>(static_cast<uint16_t>(get(offset, 2))); }};
  crew_state result;
  if(role == crew_role::pilot) {
    auto &pilot{result.pilot};
    for(size_t i{0}; i < 3; ++i) { pilot.angles[i] = signed_word(i * 4); pilot.position[i] = signed_word(52 + i * 4); }
    pilot.flight_time = signed_word(12); pilot.climb = signed_word(16); pilot.fuel = signed_short(24); pilot.weight = signed_short(26);
    pilot.torque = bytes[34]; for(size_t i{0}; i < 8; ++i) pilot.engines[i] = bytes[35 + i];
    pilot.sidespeed = bytes[45]; pilot.airspeed = bytes[46]; pilot.frame_ticks = bytes[48]; pilot.landing = bytes[49]; pilot.landed = (bytes[50] & 4) != 0;
  } else {
    auto &gunner{result.gunner}; gunner.hydra_elevation = signed_word(20);
    gunner.drone = signed_short(28); gunner.task = signed_short(30);
    for(size_t i{0}; i < 3; ++i) { gunner.display_position[i] = signed_word(52 + i * 4); gunner.position[i] = signed_word(64 + i * 4); }
    gunner.target_kind = get(76); gunner.target_source = get(80); if(gunner.target_kind > 2) return {};
  }
  result.death = bytes[43]; result.message = bytes[47]; result.leaving = (bytes[50] & 2) != 0;
  for(size_t i{0}; i < bytes[44]; ++i) result.keys.push_back(static_cast<uint16_t>(get(84 + i * 2, 2)));
  return result;
}
} // namespace apache::multiplayer
