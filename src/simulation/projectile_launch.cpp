#include "projectile_launch.h"
#include <algorithm>
#include <stdexcept>

namespace apache::simulation {
namespace {
uint32_t random_pair(uint32_t &seed) {
  seed = seed * 0x41c64e6du + 0x3039u; auto const high{seed & 0x7fff0000u};
  seed = seed * 0x41c64e6du + 0x3039u; return high | ((seed >> 16) & 0x7fff);
}
int32_t next(uint32_t &value, uint32_t &global) {
  combat_random random{value, global};
  auto const result{random.next()};
  value = random.value;
  global = random.global_seed;
  return result;
}
int32_t approximate_range(math::vector3 const &a, math::vector3 const &b) {
  math::vector3 v; for(size_t i{0}; i != 3; ++i) { v[i] = math::subtract(a[i], b[i]); if(v[i] < 0) v[i] = math::negate(v[i]); }
  std::ranges::sort(v); return math::add(math::add(v[2], v[1] >> 2), v[0] >> 2);
}
} // anonymous namespace
void construct_projectile(projectile_launch_state &round, unsigned int const id, weapon_info const &weapon, projectile_launch_inputs const &input, math::trigonometry const &trig, std::function<int32_t(int32_t, int32_t)> const &height, uint32_t &global_random, uint32_t &weapon_random) {
  if(!id || id >= 55 || weapon.movement_class < 1 || weapon.movement_class > 4) throw std::runtime_error{"Invalid projectile definition"};
  round.weapon = id; round.body.motion.distance = 0; round.random = random_pair(global_random);
  round.body.motion.flags &= ~7u;
  if(weapon.motor_acceleration) round.body.motion.flags |= 2;
  if(weapon.movement_class == 3) {
    int32_t const dispersion{weapon.tracking_or_dispersion};
    if(dispersion <= 0 || dispersion > INT32_MAX / 2 || weapon.maximum_speed <= 0) throw std::runtime_error{"Invalid ballistic projectile dispersion or speed"};
    int32_t dx{next(weapon_random, global_random) % (dispersion * 2) - dispersion};
    int32_t dy{next(weapon_random, global_random) % (dispersion * 2) - dispersion};
    if(id == 28) {
      auto const heading{trig.arctangent(input.source.direction[0] >> 16, input.source.direction[1] >> 16)};
      auto const cosine{trig.cosine(heading)};
      dx = math::negate(math::rounded_q31(0ull - math::product_bits(dx, cosine)));
      dy = math::multiply_q31(dy / 2, cosine);
    }
    round.impact = {math::add(input.pose.position[0], dx), math::add(input.pose.position[1], dy), 0};
    round.impact[2] = height(round.impact[0], round.impact[1]);
    round.body.pose.position[0] = std::clamp(round.impact[0], 0, 0xfffffff);
    round.body.pose.position[1] = std::clamp(round.impact[1], 0, 0xfffffff);
    auto const distance{approximate_range(input.pose.position, input.source.position)};
    auto const duration{std::bit_cast<int32_t>(static_cast<uint32_t>(distance) * 200u) / weapon.maximum_speed};
    // Construction precedes launch, so the original uses the slot's previous timestamp.
    round.impact_at = math::add(math::add(round.launched_at, duration), next(round.random, global_random) % 1000);
    return;
  }
  round.body.motion.speed = std::clamp(math::add(weapon.launch_speed, input.source.speed), 256, INT32_MAX - 1);
  round.body.pose = input.pose;
  for(size_t i{0}; i != 2; ++i) round.body.pose.position[i] = std::clamp(round.body.pose.position[i], 0, 0xfffffff);
  round.body.previous = round.body.pose.position;
  auto &rotation{round.body.pose.rotation};
  // The inverted, exactly-zero component path reads an unset angle in the original.
  if(rotation[2]) {
    auto const roll{trig.arcsine(rotation[2])};
    if(roll) rotation = math::multiply(trig.transformation(0, 0, roll), rotation);
  }
  round.body.angles = trig.angles(rotation); round.body.angles[2] = 0;
  if(weapon.movement_class == 2) {
    round.body.seeker.flags = (round.body.seeker.flags & ~1u) | (input.has_target ? 1u : 0u);
    round.body.seeker.retry_time = 0; round.body.motion.flags |= 4;
  } else if(weapon.movement_class == 4) {
    round.simple_value = input.simple_value; round.body.motion.flags |= 2;
  }
}
render::model_instance projectile_model(projectile_launch_state const &round, weapon_info const &weapon, resources::model_archive const &models) {
  auto const id{static_cast<uint32_t>(weapon.model)};
  int32_t const flame{round.weapon == 4 || round.weapon == 5 ? 500 : models.at(id)->metadata[5]};
  std::array<uint32_t, 1> const children{flame > 0 ? static_cast<uint32_t>(flame) : 0};
  auto object{render::construct_model(models, id, round.body.pose, children)}; object.status |= 1;
  render::maintain_model(object); return object;
}
void launch_projectile(projectile_launch_state &round, int32_t const now) {
  round.body.motion.flags |= 1;
  round.launched_at = now;
}
} // namespace apache::simulation
