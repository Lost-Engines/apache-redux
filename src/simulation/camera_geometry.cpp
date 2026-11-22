#include "camera_geometry.h"
#include <algorithm>

namespace apache::simulation {
namespace {
math::vector3 difference(math::vector3 const &a, math::vector3 const &b) {
  math::vector3 v;
  for(size_t i{0}; i != 3; ++i) v[i] = math::subtract(a[i], b[i]);
  return v;
}
int32_t increment(int32_t value, int32_t rate, int32_t delta) {
  return math::multiply_q31(math::multiply_q31(value, rate), delta);
}
int32_t arc(math::trigonometry const &trig, int32_t value, uint32_t length) {
  if(!length) return 0;
  auto const ratio{std::clamp<int64_t>(int64_t{value} * 0x80000000ll / length, INT32_MIN, INT32_MAX)};
  return std::bit_cast<int32_t>(trig.arcsine(static_cast<int32_t>(ratio)));
}
uint64_t square(int32_t value) {
  return math::product_bits(value, value);
}
uint32_t magnitude(int32_t value) {
  return value < 0 ? 0u - static_cast<uint32_t>(value) : static_cast<uint32_t>(value);
}
} // anonymous namespace
math::pose camera_geometry::head(math::pose body, uint32_t const heading, uint32_t const pitch) const {
  body.rotation = math::multiply(trig.transformation(heading, pitch, 0), body.rotation); return body;
}
math::pose camera_geometry::tracking(orbit_camera &state, math::pose const &body, uint32_t const model, int32_t const radius, camera_input const &input) const {
  state.heading -= static_cast<uint32_t>(increment(input.turn, 0x40000000, input.delta));
  state.pitch = std::clamp(math::add(state.pitch, increment(input.pitch, 0x40000000, input.delta)), -0x40000000, 0x40000000);
  int32_t const zoom{increment(math::negate(input.zoom), 0x40000000, input.delta)};
  state.zoom = math::add(state.zoom, zoom); if(state.zoom < 0) state.zoom = zoom < 0 ? 0 : INT32_MAX;
  math::pose result{body.position, trig.transformation(trig.arctangent(body.rotation[3], body.rotation[4]) + state.heading, static_cast<uint32_t>(state.pitch), 0)};
  int32_t const distance{model == 103 || model == 607 ? math::add(0x3000, math::multiply_q31(0xa000, state.zoom)) : math::add(std::bit_cast<int32_t>(static_cast<uint32_t>(radius) * 4), math::multiply_q31(std::bit_cast<int32_t>(static_cast<uint32_t>(radius) << 5), state.zoom))};
  for(size_t i{0}; i != 3; ++i) result.position[i] = math::add(result.position[i], math::multiply_q31(result.rotation[3 + i], math::negate(distance)));
  return result;
}
math::pose camera_geometry::spectator(orbit_camera &state, math::vector3 const &position, camera_input const &input) const {
  state.heading += static_cast<uint32_t>(increment(input.turn, 0x40000000, input.delta));
  state.pitch = std::clamp(math::add(state.pitch, increment(input.pitch, 0x40000000, input.delta)), -0x40000000, 0x40000000);
  return {position, trig.transformation(state.heading, static_cast<uint32_t>(state.pitch), 0)};
}
math::pose camera_geometry::remote(math::vector3 const &position, math::vector3 const &subject) const {
  auto const unit{math::normalise_q31(difference(subject, position))};
  return {position, trig.transformation(trig.arctangent(unit[0], unit[1]), trig.arcsine(unit[2]), 0)};
}
math::pose camera_geometry::missile_target(math::vector3 const &missile, math::vector3 const &target, int32_t const target_height) const {
  auto const unit{math::normalise_q31(difference(missile, target))}; auto position{target};
  int32_t const distance{std::bit_cast<int32_t>(static_cast<uint32_t>(target_height) * 3)};
  for(size_t i{0}; i != 3; ++i) position[i] = math::subtract(position[i], math::multiply_q31(unit[i], distance));
  position[2] = std::max(position[2], math::add(height(position[0], position[1]), distance / 4));
  return remote(position, missile);
}
math::pose camera_geometry::target_lock(math::vector3 const &body, math::vector3 const &target) const {
  auto const unit{math::normalise_q31(difference(target, body))}; auto position{body};
  for(size_t i{0}; i != 3; ++i) position[i] = math::subtract(position[i], math::multiply_q31(unit[i], 0x5000));
  position[2] = std::max(math::add(position[2], 0x1400), math::add(height(position[0], position[1]), 0x400));
  return remote(position, target);
}
math::pose camera_geometry::crash(orbit_camera &state, math::vector3 const &position, bool const airborne, int32_t const delta) const {
  state.heading += static_cast<uint32_t>(math::multiply_q31(delta, 0x20000000));
  state.pitch = std::max(math::subtract(state.pitch, math::multiply_q31(delta, airborne ? 0x15555555 : 0x71c71c7)), airborne ? -0x40000000 : -0x15555555);
  math::pose result{position, trig.transformation(state.heading, static_cast<uint32_t>(state.pitch), 0)};
  for(size_t i{0}; i != 3; ++i) result.position[i] = math::subtract(result.position[i], math::multiply_q31(result.rotation[3 + i], 0x6400));
  return result;
}
math::pose camera_geometry::helmet(look_camera &state, math::pose const &body, camera_input const &input, std::optional<math::vector3> const &locked) const {
  if(locked) {
    auto const local{math::transform(body.rotation, difference(*locked, body.position))};
    auto const horizontal{math::integer_sqrt(square(local[0]) + square(local[1]))}, length{math::integer_sqrt(uint64_t{horizontal} * horizontal + square(local[2]))};
    state.heading = !horizontal ? 0 : magnitude(local[0]) >= horizontal ? local[0] < 0 ? -0x40000000 : 0x40000000 : arc(trig, local[0], horizontal);
    state.pitch = !length ? 0 : magnitude(local[2]) >= length ? local[2] < 0 ? -0x20000000 : 0xe38e38e : arc(trig, local[2], length);
  } else {
    state.heading = math::add(state.heading, increment(input.turn, 0x40000000, input.delta)); state.pitch = math::add(state.pitch, increment(input.pitch, 0x40000000, input.delta));
  }
  state.heading = std::clamp(state.heading, -0x40000000, 0x40000000); state.pitch = std::clamp(state.pitch, -0x20000000, 0x38e38e39);
  return head(body, static_cast<uint32_t>(state.heading), static_cast<uint32_t>(state.pitch));
}
math::pose camera_geometry::goggles(look_camera &state, math::pose const &body, camera_input const &input, std::optional<math::vector3> const &locked, int32_t const rate, int32_t const distance) const {
  if(locked) {
    auto const local{math::transform(body.rotation, difference(*locked, body.position))};
    auto const horizontal{math::integer_sqrt(square(local[0]) + square(local[1]))}, length{math::integer_sqrt(uint64_t{horizontal} * horizontal + square(local[2]))};
    state.heading = !horizontal ? 0 : magnitude(local[0]) >= horizontal ? local[0] < 0 ? -0x40000000 : 0x40000000 : local[1] < 0 ? local[0] < 0 ? math::subtract(-0x40000000, arc(trig, math::negate(local[1]), horizontal)) : math::add(0x40000000, arc(trig, math::negate(local[1]), horizontal)) : arc(trig, local[0], horizontal);
    state.pitch = !length ? 0 : magnitude(local[2]) > length ? local[2] < 0 ? -0x2aaaaaab : 0x15555555 : arc(trig, local[2], length);
  } else {
    state.heading = math::add(state.heading, increment(input.turn, rate, input.delta)); state.pitch = math::add(state.pitch, increment(input.pitch, rate, input.delta));
  }
  state.heading = std::clamp(state.heading, -0x60000000, 0x60000000); state.pitch = std::clamp(state.pitch, -0x2aaaaaab, 0x15555555);
  auto result{head(body, static_cast<uint32_t>(state.heading), static_cast<uint32_t>(state.pitch))}; magnify(result, distance); return result;
}
void camera_geometry::magnify(math::pose &view, int32_t const distance) const {
  for(size_t i{0}; i != 3; ++i) view.position[i] = math::add(view.position[i], math::multiply_q31(view.rotation[3 + i], distance));
  view.position[2] = std::max(view.position[2], height(view.position[0], view.position[1]));
}
} // namespace apache::simulation
