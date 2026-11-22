#include "player_sight.h"
#include <algorithm>
#include <cmath>

namespace apache::simulation {
namespace {
math::vector3 difference(math::vector3 const &a, math::vector3 const &b) {
  math::vector3 r;
  for(size_t i{0}; i != 3; ++i) r[i] = math::subtract(a[i], b[i]);
  return r;
}
int32_t magnitude(int32_t v) {
  return v < 0 ? math::negate(v) : v;
}
int32_t range(math::vector3 const &v) {
  double const x{static_cast<double>(v[0])};
  double const y{static_cast<double>(v[1])};
  double const z{static_cast<double>(v[2])};
  return std::bit_cast<int32_t>(static_cast<uint32_t>(static_cast<uint64_t>(std::sqrt(x * x + y * y + z * z))));
}
int32_t product(int32_t a, int32_t b) {
  return std::bit_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b));
}
}
unsigned int player_weapon_sight::evaluate(weapon_sight_inputs const &input, weapon_sight_callbacks const &events) {
  auto const old_status{status};
  auto const old_target{previous};
  previous = input.target;
  auto const &weapon{definitions.at(input.weapon)};
  auto sound{[&](unsigned int id){
    if(events.cockpit_sound) events.cockpit_sound(id);
  }};
  if(!input.armed) return status = 7;
  if(input.target.identity.kind == target_kind::none) return status = 1;
  auto const distance{range(difference(input.body.position, input.target_position))};
  if(distance > weapon.maximum_range) {
    if(equivalent_targets(old_target, input.target) && (old_status == 3 || old_status == 6 || old_status == 8)) sound(62);
    return status = 4;
  }
  if(distance < weapon.minimum_range) return status = 5;
  if(weapon.guidance == 3) {
    if(input.weather == 2 || input.weather == 3) return status = 9;
    auto const z{input.body.position[2]};
    auto const target_z{input.target_position[2]};
    auto const bottom{input.cloud_bottom};
    auto const top{input.cloud_top};
    if(input.weather == 1 && ((bottom < z && z < top) || (z < bottom && bottom < target_z) || (bottom < z && target_z < bottom) || (z < top && top < target_z) || (top < z && target_z < top))) return status = 6;
    if(input.target_classification != 2) return status = 9;
  } else if(weapon.guidance == 1 && (input.weather == 2 || input.weather == 3)) return status = 6;
  if(events.visible && !events.visible()) return status = 6;
  if(input.weapon == 1) return status = events.position_gun ? events.position_gun() : 1;
  if(input.weapon == 4 || input.weapon == 5) return status = input.hydras_locked ? 3 : 8;
  if(input.weapon != 2 && input.weapon != 3 && input.weapon != 6) return status = 2;
  math::vector3 forward;
  math::vector3 origin{input.body.position};
  for(size_t i{0}; i != 3; ++i) {
    forward[i] = input.body.rotation[3 + i];
    origin[i] = math::add(origin[i], math::multiply_q31(forward[i], weapon.guidance_start_distance));
  }
  auto const angles{relative_aim_angles(math::normalise_q31(difference(input.target_position, origin)), forward, trig)};
  int32_t const limit{weapon.movement_class == 3 || weapon.movement_class == 4 ? 0 : weapon.tracking_or_dispersion};
  if(magnitude(angles[0]) > limit || magnitude(angles[1]) > limit) return status = 8;
  if(old_status != 3 && equivalent_targets(old_target, input.target)) sound(old_status == 4 ? 61 : 63);
  return status = 3;
}
void player_impact_times::launch(size_t const round, unsigned int const id, math::vector3 const &position, std::optional<math::vector3> const &target, int32_t const now) {
  if(entries.size() >= 8 || !target) return;
  auto delta{difference(*target, position)};
  for(auto &v : delta) v = magnitude(v);
  std::ranges::sort(delta);
  auto const distance{math::add(math::add(delta[2], delta[1] >> 2), delta[0] >> 2)};
  auto const &weapon{definitions.at(id)};
  int32_t const half{weapon.maximum_speed / 2};
  if(half <= 0) return;
  auto const ticks{distance < half ? product(weapon.motor_ticks, distance) / half : math::add(product(math::subtract(distance, half), 200) / weapon.maximum_speed, weapon.motor_ticks)};
  entries.push_back({round, math::add(now, ticks)});
}
void player_impact_times::remove(size_t const round, unsigned int const weapon) {
  if(weapon == 2 || weapon == 3 || weapon == 6) {
    auto i{std::ranges::find(entries, round, &missile_impact_time::round)};
    if(i != entries.end()) entries.erase(i);
  }
}
int32_t player_impact_times::current(int32_t const now) const {
  return entries.empty() ? -1 : std::max(0, math::subtract(entries.front().deadline, now));
}
} // namespace apache::simulation
