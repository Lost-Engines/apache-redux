#include "seeker.h"
#include <cmath>

namespace apache::simulation {
namespace {
uint32_t range(math::vector3 const &a, math::vector3 const &b) {
  double const x{static_cast<double>(math::subtract(a[0], b[0]))}, y{static_cast<double>(math::subtract(a[1], b[1]))}, z{static_cast<double>(math::subtract(a[2], b[2]))};
  return static_cast<uint32_t>(static_cast<uint64_t>(std::sqrt(x * x + y * y + z * z)));
}
bool crosses(int32_t const a, int32_t const b, int32_t const plane) {
  return (a < plane && plane < b) || (b < plane && plane < a);
}
}
bool decoy_distracts(weapon_info const &weapon, int32_t const mode, bool const enemy, int32_t const modifier, combat_random &random) {
  int32_t const value{random.next()};
  if(mode == 1 || mode == 4 || mode == 5) return value % 24 == 0;
  if(!weapon.decoy_denominator) return false;
  int32_t const remainder{value % weapon.decoy_denominator};
  return enemy ? modifier != 0 && remainder / modifier == 0 : remainder == 0;
}
bool update_seeker(seeker_state &state, weapon_info const &weapon, seeker_inputs const &input, seeker_queries const &queries) {
  if(!(state.flags & 2) && input.now < state.retry_time && ((state.adjusted != seeker_target::none && input.adjusted_valid) || state.retry_time == INT32_MAX)) return false;
  auto const lose{[&]{ state.adjusted = seeker_target::none; state.flags &= ~2u; }};
  auto const permanent{[&]{ lose(); state.retry_time = INT32_MAX; }};
  if(input.target_status == 1) { lose(); return true; }
  if(weapon.guidance == 1) {
    if(!input.has_designator) { lose(); return true; }
    if(input.weather == 2) { permanent(); return true; }
    if(input.ground_designated) {
      if(range(input.missile_position, input.target_position) > 0x465000u) { lose(); return true; }
    } else if(!input.designator_active || input.designator_destroyed || range(input.designator_position, input.target_position) > 0x465000u) { lose(); return true; }
  } else if(weapon.guidance == 2 || weapon.guidance == 3) {
    bool const infrared{weapon.guidance == 3};
    if(infrared && (queries.classification ? queries.classification() : input.target_classification) != 2) { state.adjusted = seeker_target::none; return true; }
    if(state.retry_time < input.now && (infrared ? input.flare : input.chaff)) {
      if(queries.distracted()) {
        state.flags &= ~2u; state.retry_time = math::add(input.now, 800);
        state.adjusted = infrared ? seeker_target::flare : seeker_target::chaff; return true;
      }
      state.retry_time = math::add(input.now, 100);
    }
    if(infrared) {
      if(input.weather == 2 || input.weather == 3) { permanent(); return true; }
      auto const z{input.missile_position[2]}, target_z{input.target_position[2]};
      if(input.weather == 1 && ((input.cloud_bottom < z && z < input.cloud_top) || crosses(z, target_z, input.cloud_bottom) || crosses(z, target_z, input.cloud_top))) { lose(); return true; }
    } else if(input.source_radar && !queries.visible(input.source_position, input.target_position)) { lose(); return true; }
  } else { lose(); return true; }
  if(!queries.visible(input.missile_position, input.target_position)) { lose(); return true; }
  if(input.target_position[2] < 0xc800 && input.source_side == 2 && state.retry_time < input.now) {
    if(queries.distracted()) { state.flags &= ~2u; state.retry_time = math::add(input.now, queries.random() % 200); return false; }
    state.retry_time = math::add(input.now, 200);
  }
  state.adjusted = seeker_target::selected; state.flags |= 2; return true;
}
} // namespace apache::simulation
