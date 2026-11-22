#include "effects.h"
#include <algorithm>
#include <stdexcept>

namespace apache::simulation {
namespace {
bool inside(math::vector3 const &position) {
  return position[0] >= 0 && position[0] < 0x10000000 && position[1] >= 0 && position[1] < 0x10000000;
}
void check_radius(int32_t const radius) {
  if(radius < 0 || radius > 0x1000000) throw std::runtime_error{"Invalid effect radius"};
}
} // anonymous namespace
void effects::reset() {
  for(auto &puff : smoke_puffs) puff.active = false;
  for(auto &blast : explosions) blast.kind = 0;
  next_smoke = next_explosion = 0;
}
bool effects::add_smoke(math::vector3 const &position, int32_t const radius, int32_t const lifetime, uint32_t &random_seed) {
  if(!inside(position)) return false;
  check_radius(radius);
  if(lifetime < 1) throw std::runtime_error{"Smoke needs a positive lifetime"};
  random_seed = random_seed * 0x41c64e6du + 0x3039u;
  smoke_puffs[next_smoke] = {true, position, radius, lifetime, 0, random_seed & 0x7fff0000u};
  next_smoke = (next_smoke + 1) % smoke_puffs.size();
  return true;
}
void effects::add_trail(math::vector3 const &start, math::vector3 const &end, int32_t const radius, int32_t const lifetime, unsigned int const density, uint32_t &random_seed) {
  if(density > 10 || lifetime < 1 || lifetime > INT32_MAX - 200) throw std::runtime_error{"Invalid smoke trail density or lifetime"};
  check_radius(radius);
  math::vector3 step, position{start};
  for(size_t i{0}; i != 3; ++i) step[i] = math::subtract(end[i], start[i]) >> density;
  for(unsigned int i{0}; i != (1u << density); ++i) {
    random_seed = random_seed * 0x41c64e6du + 0x3039u;
    auto const jitter{math::multiply_q31(static_cast<int32_t>(random_seed & 0x7fff0000), 200)};
    add_smoke(position, radius, lifetime + jitter, random_seed);
    for(size_t j{0}; j != 3; ++j) position[j] = math::add(position[j], step[j]);
  }
}
bool effects::add_ground_explosion(math::vector3 const &position, int32_t const radius, resources::terrain const &terrain) {
  if(!inside(position)) return false;
  check_radius(radius);
  auto ground{position}; ground[2] = terrain.elevation(position[0], position[1]);
  explosions[next_explosion] = {1, ground, {0, 0, 0}, radius, -1};
  next_explosion = (next_explosion + 1) % explosions.size();
  return true;
}
bool effects::add_air_explosion(math::vector3 const &position, int32_t const radius, math::vector3 const &velocity) {
  if(!inside(position)) return false;
  check_radius(radius);
  explosions[next_explosion] = {2, position, velocity, radius, -1};
  next_explosion = (next_explosion + 1) % explosions.size();
  return true;
}
void effects::advance(int32_t const delta_time, resources::terrain const &terrain) {
  if(delta_time < 0) throw std::runtime_error{"Visual time cannot run backwards"};
  int32_t const ticks{math::multiply_q31(delta_time, 200)};
  for(auto &blast : explosions) {
    if(!blast.kind) continue;
    if(blast.kind > 2) throw std::runtime_error{"Invalid explosion kind"};
    if(blast.frame < -1 || blast.frame >= (blast.kind == 1 ? 33 : 15)) throw std::runtime_error{"Invalid explosion frame"};
    ++blast.frame;
    if(blast.frame >= (blast.kind == 1 ? 33 : 15)) { blast.kind = 0; continue; }
    if(blast.kind == 2) {
      if(blast.frame > 0) for(size_t i{0}; i != 3; ++i) blast.position[i] = math::add(blast.position[i], math::multiply_q31(blast.velocity[i], delta_time));
      blast.position[2] = std::max(blast.position[2], math::add(terrain.elevation(blast.position[0], blast.position[1]), blast.radius));
    }
  }
  for(auto &puff : smoke_puffs) if(puff.active) {
    puff.age = math::add(puff.age, ticks);
    if(puff.age >= puff.lifetime) puff.active = false;
  }
}
} // namespace apache::simulation
