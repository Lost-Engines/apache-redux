#include "projectile_dynamics.h"
#include <algorithm>
#include <cmath>
#include "collisions.h"

namespace apache::simulation {
namespace {
math::vector3 subtract(math::vector3 const &a, math::vector3 const &b) {
  math::vector3 v;
  for(size_t i{0}; i != 3; ++i) v[i] = math::subtract(a[i], b[i]);
  return v;
}
int32_t range3(math::vector3 const &v) {
  double const x{static_cast<double>(v[0])}, y{static_cast<double>(v[1])}, z{static_cast<double>(v[2])};
  return std::bit_cast<int32_t>(static_cast<uint32_t>(static_cast<uint64_t>(std::sqrt(x * x + y * y + z * z))));
}
int32_t range2(int32_t x, int32_t y) {
  if(x < 0) x = math::negate(x);
  if(y < 0) y = math::negate(y);
  return math::add(std::bit_cast<int32_t>(static_cast<uint32_t>(std::max(x, y)) * 4), std::min(x, y)) >> 2;
}
std::array<int32_t, 2> direction_change(projectile_state const &round, math::vector3 const &target, math::trigonometry const &trig) {
  auto const v{subtract(target, round.pose.position)};
  return {std::bit_cast<int32_t>(trig.arctangent(v[0], v[1]) - round.angles[0]), std::bit_cast<int32_t>(trig.arctangent(v[2], range2(v[0], v[1])) - round.angles[1])};
}
math::vector3 straight_move(projectile_state const &round, int32_t const delta) {
  math::vector3 move; int32_t const distance{math::multiply_q31(round.motion.speed, delta)};
  for(size_t i{0}; i != 3; ++i) move[i] = math::multiply_q31(distance, round.pose.rotation[3 + i]);
  return move;
}
void unguided(projectile_step &step, projectile_state &round, math::trigonometry const &trig, int32_t const delta) {
  if(round.motion.flags & 4) pitch_projectile_down(round, trig, delta);
  step.displacement = straight_move(round, delta);
}
} // anonymous namespace
closest_approach closest_point(math::vector3 const &start, math::vector3 const &end, math::vector3 const &target, math::trigonometry const &trig) {
  closest_approach result{perpendicular_distance(start, end, target, trig), end};
  if(result.distance == INT32_MAX) return result;
  int32_t const length{range3(subtract(start, end))}, to_target{range3(subtract(start, target))};
  auto const projected{std::bit_cast<int32_t>(math::integer_sqrt(math::product_bits(to_target, to_target) - math::product_bits(result.distance, result.distance)))};
  if(length && projected < length) {
    int64_t const quotient{(int64_t{projected} * 0x80000000ll) / length};
    if(quotient < INT32_MIN || quotient > INT32_MAX) return result;
    auto const fraction{static_cast<int32_t>(quotient)};
    // The original projects from the start in the start-minus-end direction.
    for(size_t i{0}; i != 3; ++i) result.position[i] = math::add(start[i], math::multiply_q31(math::subtract(start[i], end[i]), fraction));
  }
  return result;
}
void pitch_projectile_down(projectile_state &round, math::trigonometry const &trig, int32_t const delta) {
  int32_t const speed{round.motion.speed / 256};
  if(!speed) return;
  int32_t const gravity{math::multiply_q31(trig.cosine(round.angles[1]), math::multiply_q31(delta, 0x735))};
  int64_t const turn{int64_t{gravity} * 0xb60b61 / speed};
  round.angles[1] -= static_cast<uint32_t>(turn);
  round.pose.rotation = trig.transformation(round.angles[0], round.angles[1], round.angles[2]);
}
projectile_step calculate_projectile_move(projectile_state &round, weapon_info const &weapon, std::optional<movement_target> const &target, bool const target_is_actor, math::trigonometry const &trig, int32_t const delta) {
  projectile_step step;
  if(weapon.movement_class == 1) { unguided(step, round, trig, delta); return step; }
  if(weapon.movement_class == 4) {
    step.displacement = straight_move(round, delta);
    step.displacement[2] = math::subtract(step.displacement[2], math::multiply_q31(weapon.tracking_or_dispersion, delta));
    return step;
  }
  if(weapon.movement_class != 2) return step;
  if(!target || weapon.guidance_start_distance > round.motion.distance) {
    if(weapon.guidance_start_distance <= round.motion.distance) pitch_projectile_down(round, trig, delta);
    step.displacement = straight_move(round, delta); return step;
  }
  step.notify_target = target_is_actor;
  auto const direct{direction_change(round, target->position, trig)};
  std::array<int32_t, 2> const absolute{direct[0] < 0 ? math::negate(direct[0]) : direct[0], direct[1] < 0 ? math::negate(direct[1]) : direct[1]};
  int32_t const tracking{weapon.tracking_or_dispersion};
  if(absolute[0] <= tracking && absolute[1] <= tracking) {
    auto aim{intercept_point(round.pose.position, round.motion.speed, *target, 3)};
    if(round.pose.position[2] < 0x6400 && math::add(round.motion.speed, round.motion.speed) < range2(math::subtract(aim[0], round.pose.position[0]), math::subtract(aim[1], round.pose.position[1])) && aim[2] <= 0x6400) aim[2] = 0x6400;
    auto turn{direction_change(round, aim, trig)};
    auto const window{static_cast<int32_t>(static_cast<double>(tracking) * 0.7)};
    for(size_t i{0}; i != 2; ++i) {
      auto const low{math::subtract(direct[i], window)}, high{math::add(direct[i], window)};
      if(turn[i] < low) turn[i] = low;
      else if(turn[i] > high) turn[i] = high;
    }
    int64_t const ratio{round.motion.speed ? int64_t{delta / 8} * weapon.lateral_acceleration / round.motion.speed : 0};
    int32_t const bounded{static_cast<int32_t>(std::clamp<int64_t>(ratio, -INT32_MAX, INT32_MAX))};
    int32_t const limit{std::bit_cast<int32_t>(trig.arcsine(bounded) * 8)};
    for(size_t i{0}; i != 2; ++i) {
      // Written as ordered comparisons because the original signed angle may wrap.
      if(turn[i] < math::negate(limit)) turn[i] = math::negate(limit);
      else if(turn[i] > limit) turn[i] = limit;
      round.angles[i] += static_cast<uint32_t>(turn[i]);
    }
    round.pose.rotation = trig.transformation(round.angles[0], round.angles[1], round.angles[2]);
    step.displacement = straight_move(round, delta); return step;
  }
  if(absolute[0] < 0x55555556 && absolute[1] < 0x55555556) {
    round.seeker.flags &= ~2u; unguided(step, round, trig, delta); return step;
  }
  auto const closest{closest_point(round.previous, round.pose.position, target->position, trig)};
  if(math::add(math::add(weapon.blast_radius, weapon.blast_radius), weapon.blast_radius) < closest.distance) {
    round.seeker.retry_time = INT32_MAX; round.seeker.flags &= ~2u; round.seeker.adjusted = seeker_target::none;
    unguided(step, round, trig, delta); return step;
  }
  round.pose.position = closest.position; step.explosion = closest.position; round.motion.flags &= ~1u;
  return step;
}
terrain_intersection subdivide_terrain_intersection(math::vector3 const &start, math::vector3 const &end, std::function<int32_t(int32_t, int32_t)> const &height) {
  terrain_intersection result{false, start, end};
  if(math::subtract(end[2], height(end[0], end[1])) >= 0) return result;
  result.hit = true; result.below = start;
  auto increment{subtract(end, start)}; for(auto &v : increment) v /= 8;
  for(unsigned int i{0}; i != 8; ++i) {
    for(size_t j{0}; j != 3; ++j) result.below[j] = math::add(result.below[j], increment[j]);
    if(math::subtract(result.below[2], height(result.below[0], result.below[1])) <= 0) break;
  }
  result.above = subtract(result.below, increment); return result;
}
bounded_projectile_move bound_projectile_move(math::vector3 const &start, math::vector3 const &move, std::function<int32_t(int32_t, int32_t)> const &height) {
  bounded_projectile_move result; result.displacement = move;
  for(size_t i{0}; i != 3; ++i) result.end[i] = math::add(start[i], move[i]);
  auto slice{subdivide_terrain_intersection(start, result.end, height)};
  result.hit_terrain = slice.hit;
  if(slice.hit) {
    for(unsigned int i{0}; i != 2; ++i) slice = subdivide_terrain_intersection(slice.above, slice.below, height);
    result.impact = slice.below; result.displacement = subtract(result.impact, start); result.displacement[2] = 0;
  }
  return result;
}
} // namespace apache::simulation
