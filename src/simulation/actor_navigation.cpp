#include "actor_navigation.h"
#include <algorithm>
#include <cmath>

namespace apache::simulation {
namespace {
int32_t signed_angle(uint32_t const value) {
  return std::bit_cast<int32_t>(value);
}
int32_t magnitude(int32_t const value) {
  return value < 0 ? math::negate(value) : value;
}
int32_t product(int32_t const a, int32_t const b) {
  return std::bit_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b));
}
int32_t range2(int32_t const x, int32_t const y) {
  auto const a{magnitude(x)}, b{magnitude(y)};
  return math::add(product(std::max(a, b), 4), std::min(a, b)) >> 2;
}
int32_t range3(math::vector3 value) {
  for(auto &v : value) v = magnitude(v);
  std::ranges::sort(value);
  return math::add(math::add(value[2], value[1] >> 2), value[0] >> 2);
}
bool outside(math::vector3 const &p) {
  return p[0] < 0 || p[1] < 0 || p[0] > 0xfffffff || p[1] > 0xfffffff;
}
int32_t required_speed(actor_state const &unit, navigation_order const &order, navigation_characteristics const &performance, int32_t const height_error, int32_t const range) {
  if((unit.conditions & 8) && !order.player_task && (unit.classification == 1 || unit.classification == 2) && order.has_target && range < 2027520) {
    if(order.target_classification == 1) return performance.ground_attack_speed;
    if(order.target_classification == 2) return performance.air_attack_speed;
  }
  if(!order.has_waypoint) return 0;
  if(!order.has_target && unit.classification == 1) {
    auto const &position{unit.object.world.position}; int32_t const dz{math::subtract(order.target[2], position[2])};
    if(order.previous_height && !*order.previous_height) {
      if(height_error == magnitude(dz) && magnitude(dz) > 256) return order.carrier_speed.value_or(0);
    } else if(!order.waypoint_height && range2(math::subtract(order.target[0], position[0]), math::subtract(order.target[1], position[1])) < 0x12fc0) return math::add(std::min(unit.speed, 0x10e0), order.carrier_speed.value_or(0));
  }
  return order.waypoint_speed;
}
void bound_helicopter(actor_state &unit, navigation_order const &order, int32_t const height_error, int32_t const delta) {
  if(order.has_target) { if(unit.conditions & 2) unit.angles[1] = 0; return; }
  auto const &p{unit.object.world.position};
  math::vector3 difference;
  for(size_t i{0}; i != 3; ++i) difference[i] = math::subtract(order.target[i], p[i]);
  int32_t const dz{difference[2]};
  if(order.previous_height && !*order.previous_height) {
    if(height_error != magnitude(dz)) return;
    unit.velocity[2] = std::clamp(dz, math::multiply_q31(-0x500, delta), math::multiply_q31(0xf00, delta));
    if(magnitude(dz) >= 256) unit.angles[1] = 0;
    return;
  }
  if(order.waypoint_height) return;
  int32_t const range{range2(difference[0], difference[1])};
  if(range > 0x653f) {
    if(range > 0x12fc0 && height_error == dz) unit.velocity[2] = 0;
    return;
  }
  bool const close{magnitude(dz) <= 0x1e00};
  unit.velocity[2] = std::clamp(dz, math::multiply_q31(close ? -0x280 : -0x500, delta), math::multiply_q31(close ? 0x780 : 0xf00, delta));
  unit.velocity[0] = std::clamp(unit.velocity[0], math::negate(magnitude(difference[0])), magnitude(difference[0]));
  unit.velocity[1] = std::clamp(unit.velocity[1], math::negate(magnitude(difference[1])), magnitude(difference[1]));
}
} // anonymous namespace
navigation_tables::navigation_tables(resources::executable_data const &executable) {
  auto const read{[&](auto &rows, uint32_t address){
    resources::reader input{executable.at(address, rows.size() * 32), "Actor navigation characteristics"};
    for(auto &row : rows) row = {input.i32(), input.i32(), input.i32(), input.i32(), input.i32(), input.i32(), input.i32(), input.i32()};
  }};
  read(aircraft, 0x426f70); read(ships, 0x427010);
}
navigation_result navigate_actor(actor_state &unit, navigation_order const &order, navigation_characteristics const &performance, math::trigonometry const &trig, navigation_environment const &world, int32_t const delta) {
  if(!unit.speed && !order.waypoint_speed) return {true, false};
  auto &position{unit.object.world.position}; math::vector3 difference;
  for(size_t i{0}; i != 3; ++i) difference[i] = math::subtract(order.target[i], position[i]);
  int32_t const movement{math::multiply_q31(unit.speed, delta)};
  int32_t range{range2(math::negate(difference[0]), math::negate(difference[1]))};
  bool arrived{range <= order.arrival_radius && !(!order.waypoint_height && position[2] != order.target[2] && position[2] != 0)};
  uint32_t const heading{difference[0] || difference[1] ? trig.arctangent(difference[0], difference[1]) : unit.angles[0]};
  int32_t const ground{world.height(position[0], position[1])};
  // The original leaves these locals unset on aligned/no-pitch paths. Defined
  // zero corrections keep those paths from inheriting unrelated stack data.
  int32_t heading_correction{0}, pitch_correction{0}, height_error{difference[2]}, acceleration{0};
  if(heading == unit.angles[0]) unit.angles[2] = 0;
  else {
    int32_t const error{signed_angle(heading - unit.angles[0])};
    if(ground < position[2] || unit.classification == 7) {
      int32_t const limit{std::clamp(static_cast<int32_t>(std::abs(static_cast<double>(signed_angle(unit.angles[2]))) + 23860929.0), -performance.turn_rate, performance.turn_rate)};
      heading_correction = std::clamp(error, -limit, limit);
      int32_t const bank{math::add(signed_angle(unit.angles[2]), product(math::multiply_q31(math::subtract(heading_correction, signed_angle(unit.angles[2])), delta), 2))};
      unit.angles[2] = static_cast<uint32_t>(std::clamp(bank, -performance.maximum_bank, performance.maximum_bank));
      unit.angles[0] += static_cast<uint32_t>(math::multiply_q31(signed_angle(unit.angles[2]), delta));
    } else {
      unit.angles[2] = 0; heading_correction = std::clamp(error, -performance.turn_rate, performance.turn_rate);
      unit.angles[0] += static_cast<uint32_t>(math::multiply_q31(heading_correction, delta));
    }
  }
  if(!performance.pitch_rate) unit.angles[1] = 0;
  else {
    if(position[2] < world.tallest_object && !ground) {
      auto ahead{position};
      for(size_t i{0}; i != 2; ++i) ahead[i] = math::add(position[i], math::multiply_q31(unit.object.world.rotation[3 + i], product(unit.speed, 4)));
      if(outside(ahead)) return {false, true};
      if(world.obstacle) if(auto const obstacle{world.obstacle(position, ahead)}; obstacle && obstacle->height >= position[2]) {
        height_error = math::subtract(obstacle->height, position[2]); range = range2(math::subtract(obstacle->position[0], position[0]), math::subtract(obstacle->position[1], position[1]));
      }
    } else {
      int32_t const clearance{unit.speed > 0x10e0 ? std::max(unit.speed / 2, 0x2800) : 0};
      int32_t const ahead_height{std::max(ground, world.height(math::add(position[0], unit.velocity[0]), math::add(position[1], unit.velocity[1])))};
      if(ground && math::add(ahead_height, clearance) > position[2]) { height_error = math::subtract(math::add(ahead_height, clearance), position[2]); range = 0x1e00; }
    }
    if(!height_error) unit.angles[1] = 0;
    else {
      pitch_correction = std::clamp(signed_angle(trig.arctangent(height_error, range) - unit.angles[1]), -performance.pitch_rate, performance.pitch_rate);
      unit.angles[1] += static_cast<uint32_t>(math::multiply_q31(pitch_correction, delta));
    }
  }
  if(pitch_correction > performance.pitch_rate * 0.8 || heading_correction > performance.turn_rate * 0.8) unit.conditions |= 0x8000;
  else unit.conditions &= ~0x8000u;
  int32_t const wanted{required_speed(unit, order, performance, height_error, range)};
  if(wanted != unit.speed) {
    acceleration = math::subtract(wanted, unit.speed);
    if(magnitude(acceleration) < 4) unit.speed = wanted;
    else {
      int32_t const limit{unit.conditions & 10 ? performance.acceleration : static_cast<int32_t>(performance.acceleration * 0.6)};
      acceleration = std::clamp(acceleration, -limit, limit);
      int32_t const gentle{static_cast<int32_t>(limit * 0.1)};
      if(unit.classification == 1 && magnitude(acceleration) > gentle) {
        if(wanted != unit.speed_ramp_target) { unit.speed_ramp_target = wanted; unit.desired_speed = unit.speed; }
        if(unit.desired_speed == unit.speed) acceleration = std::clamp(acceleration, -gentle, gentle);
        else {
          int32_t const midpoint{math::add(unit.desired_speed, wanted) / 2}, half{midpoint / 2}, quarter{std::max(1, half / 2)};
          if(math::add(quarter, half) < magnitude(math::subtract(midpoint, unit.speed))) {
            int32_t const endpoint{magnitude(math::subtract(unit.desired_speed, unit.speed)) < magnitude(math::subtract(unit.speed_ramp_target, unit.speed)) ? unit.desired_speed : unit.speed_ramp_target};
            acceleration = product(acceleration, magnitude(math::subtract(endpoint, unit.speed))) / quarter;
            if(acceleration < 0) acceleration = std::min(acceleration, -gentle); else acceleration = std::max(acceleration, gentle);
          }
        }
      }
      unit.speed = math::add(unit.speed, math::multiply_q31(acceleration, delta));
      if(math::add(ground, 0x500) < position[2]) unit.speed = std::clamp(unit.speed, performance.minimum_speed, performance.maximum_speed);
      else unit.speed = std::clamp(unit.speed, 0, performance.maximum_speed / 2);
    }
  }
  if(unit.classification == 2 && position[2] <= math::add(ground, 0x500)) arrived = range3(difference) < std::max(movement, 0x500);
  unit.velocity = {math::multiply_q31(trig.sine(unit.angles[0]), movement), math::multiply_q31(trig.cosine(unit.angles[0]), movement), math::multiply_q31(trig.sine(unit.angles[1]), movement)};
  uint32_t const motion_pitch{unit.angles[1]};
  if(unit.classification == 1) {
    int32_t const divisor{order.arcade ? 4320 * 4 : 4320};
    unit.angles[1] = static_cast<uint32_t>(product(-0xe38e38e / divisor, acceleration));
    bound_helicopter(unit, order, height_error, delta);
  }
  unit.object.world.rotation = trig.transformation(unit.angles[0], unit.angles[1], unit.angles[2]); unit.angles[1] = motion_pitch;
  position[2] = std::clamp(position[2], 0, 12800000);
  auto next{position}; for(size_t i{0}; i != 3; ++i) next[i] = math::add(next[i], unit.velocity[i]);
  if(outside(next)) return {false, true};
  if(world.relocate) world.relocate(unit, next); else position = next;
  if(!unit.carrier_slot) unit.object.relative = unit.object.world;
  return {arrived, false};
}
} // namespace apache::simulation
