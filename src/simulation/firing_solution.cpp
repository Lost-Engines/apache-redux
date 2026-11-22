#include "firing_solution.h"
#include <algorithm>
#include <cmath>

namespace apache::simulation {
namespace {
int32_t magnitude(int32_t value) {
  return value < 0 ? math::negate(value) : value;
}
int32_t approximate2(int32_t x, int32_t y) {
  x = magnitude(x); y = magnitude(y);
  return std::bit_cast<int32_t>(static_cast<uint32_t>(std::max(x, y)) * 4u + static_cast<uint32_t>(std::min(x, y))) >> 2;
}
int32_t integer(double value) {
  return value < INT32_MIN || value >= 2147483648.0 ? INT32_MIN : static_cast<int32_t>(value);
}
math::vector3 difference(math::vector3 const &a, math::vector3 const &b) {
  math::vector3 result; for(size_t i{0}; i != 3; ++i) result[i] = math::subtract(a[i], b[i]); return result;
}
} // anonymous namespace
weapon_angles firing_angles(weapon_info const &weapon) {
  int32_t const spread{weapon.movement_class == 3 || weapon.movement_class == 4 ? 0 : weapon.tracking_or_dispersion};
  weapon_angles result;
  if(weapon.flags & 4) {
    auto const margin{static_cast<double>(spread) * 0.7};
    result = {integer(weapon.heading_limit + margin), integer(weapon.minimum_pitch - margin), integer(weapon.maximum_pitch + margin)};
  } else result = {math::add(weapon.heading_limit, spread), math::subtract(weapon.minimum_pitch, spread), math::add(weapon.maximum_pitch, spread)};
  if(result.heading < 0) result.heading = INT32_MAX;
  result.minimum_pitch = std::max(result.minimum_pitch, -0x40000000);
  result.maximum_pitch = std::min(result.maximum_pitch, 0x40000000);
  return result;
}
std::array<int32_t, 2> relative_aim_angles(math::vector3 const &aim, math::vector3 const &forward, math::trigonometry const &trig) {
  auto const angles{[&](math::vector3 vector){
    for(auto &value : vector) value >>= 16;
    return std::array{trig.arctangent(vector[0], vector[1]), trig.arctangent(vector[2], approximate2(vector[0], vector[1]))};
  }};
  auto const a{angles(aim)}, f{angles(forward)};
  return {std::bit_cast<int32_t>(a[0] - f[0]), std::bit_cast<int32_t>(a[1] - f[1])};
}
firing_solution aim_weapon(weapon_info const &weapon, firing_inputs const &input, math::trigonometry const &trig, std::function<bool(math::vector3 const &, math::vector3 const &)> const &visible) {
  if(weapon.movement_class != 1 && weapon.movement_class != 2) return {};
  if(weapon.weapon_role == 1 && input.target.position[2] <= 12800) return {};
  if(weapon.weapon_role == 2 && input.target.position[2] < 12800 && (input.night || input.target.position[2] < 7680)) return {};
  auto const offset{difference(input.target.position, input.source.position)};
  int32_t const distance{approximate2(offset[0], offset[1])};
  if(distance < weapon.minimum_range || distance > weapon.maximum_range) return {};
  firing_solution result;
  result.pose.position = input.source.position;
  result.pose.position[2] = integer(std::max(static_cast<double>(input.source_height) * 0.8, static_cast<double>(input.source.position[2])));
  auto aim{input.target.position};
  if(weapon.movement_class != 2) aim = intercept_point(result.pose.position, weapon.maximum_speed, input.target, static_cast<unsigned int>(std::clamp(result.pose.position[2] / 12800, 1, 3)));
  if(aim[0] < 0 || aim[1] < 0 || aim[0] > 0xfffffff || aim[1] > 0xfffffff) return {firing_status::lost_target, {}};
  if(visible && !visible(result.pose.position, aim)) return {firing_status::lost_target, {}};
  auto origin{result.pose.position};
  if((weapon.flags & 4) && weapon.movement_class == 2) for(size_t i{0}; i != 3; ++i) origin[i] = math::add(origin[i], math::multiply_q31(input.source.direction[i], weapon.guidance_start_distance));
  auto const direction{math::normalise_q31(difference(aim, origin))};
  result.pose.rotation = math::direction_matrix(direction);
  auto const angles{relative_aim_angles(direction, input.source.direction, trig)};
  auto const limits{firing_angles(weapon)};
  if(magnitude(angles[0]) > limits.heading || angles[1] < limits.minimum_pitch || angles[1] > limits.maximum_pitch) return {};
  if(weapon.flags & 4) for(size_t i{0}; i != 3; ++i) result.pose.rotation[3 + i] = input.source.direction[i];
  result.status = firing_status::ready;
  return result;
}
math::vector3 ballistic_aim(weapon_info const &weapon, math::vector3 const &source, movement_target const &target, bool const moving_mobile) {
  auto aim{target.position};
  if(!moving_mobile || !target.speed || weapon.maximum_speed <= 0) return aim;
  auto offset{difference(target.position, source)};
  for(auto &v : offset) v = magnitude(v);
  std::ranges::sort(offset);
  int32_t const range{math::add(math::add(offset[2], offset[1] >> 2), offset[0] >> 2)};
  int32_t const ticks{std::bit_cast<int32_t>(static_cast<uint32_t>(range) * 200u) / weapon.maximum_speed};
  int32_t const lead{std::bit_cast<int32_t>(static_cast<uint32_t>(ticks) * static_cast<uint32_t>(target.speed)) / 200};
  for(size_t i{0}; i != 3; ++i) aim[i] = math::add(aim[i], math::multiply_q31(target.direction[i], lead));
  return aim;
}
std::optional<math::pose> laser_bomb_aim(math::pose const &source, math::vector3 const &target, weapon_info const &weapon, math::trigonometry const &trig) {
  auto launch{source};
  launch.rotation = math::multiply(trig.transformation(0, 0xce38e38e, 0), source.rotation);
  auto const direction{math::normalise_q31(difference(target, source.position))};
  auto const angles{relative_aim_angles(direction, {launch.rotation[3], launch.rotation[4], launch.rotation[5]}, trig)};
  double const limit{weapon.movement_class == 3 || weapon.movement_class == 4 ? 0.0 : static_cast<double>(weapon.tracking_or_dispersion) * 0.4};
  if(static_cast<double>(magnitude(angles[0])) > limit || static_cast<double>(magnitude(angles[1])) > limit) return {};
  return launch;
}
} // namespace apache::simulation
