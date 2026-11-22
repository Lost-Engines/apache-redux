#include "craters.h"
#include <algorithm>
#include <stdexcept>

namespace apache::simulation {
namespace {
bool inside(math::vector3 const &position) {
  return position[0] >= 0 && position[0] < 0x10000000 && position[1] >= 0 && position[1] < 0x10000000;
}
uint32_t random_heading(uint32_t &seed) {
  seed = seed * 0x41c64e6du + 0x3039u;
  return seed & 0x7fff0000u;
}
} // anonymous namespace
void craters::reset() {
  for(auto &item : short_lived) item.model = -1;
  for(auto &item : long_lived) item.model = -1;
  next_short = next_long = 0;
}
bool craters::add(math::vector3 const &position, int32_t const model, uint32_t const heading, bool const temporary, resources::terrain const &terrain) {
  if(!inside(position)) return false;
  if(model < 0) throw std::runtime_error{"Invalid crater model"};
  auto &index{temporary ? next_short : next_long};
  auto &records{temporary ? short_lived : long_lived};
  auto &item{records[index]};
  item.position = {position[0], position[1], terrain.elevation(position[0], position[1])};
  item.model = model; item.heading = heading;
  if(temporary) item.age = 0;
  index = (index + 1) % records.size();
  return true;
}
bool craters::add_random(math::vector3 const &position, int32_t const model, bool const temporary, resources::terrain const &terrain, uint32_t &seed) {
  return add(position, model, random_heading(seed), temporary, terrain);
}
bool craters::add_scalable(math::vector3 const &position, int32_t const radius, bool const heave, resources::terrain const &terrain, uint32_t &seed) {
  if(!inside(position)) return false;
  if(radius < 0 || radius > 0x1000000) throw std::runtime_error{"Invalid crater radius"};
  int32_t const units{std::max(1, radius / 256)};
  int32_t scale{1 << 24};
  if(radius < (heave ? 0x2a00 : 0x4000)) {
    int32_t const divisor{32 / units};
    if(divisor > 1) scale = static_cast<int32_t>((0x80000000ll / divisor) >> 8);
  } else if(!heave) {
    // Original floating arithmetic is exact here: an integer divided by 32,
    // then its fractional part multiplied by 2^31 and shifted down eight.
    scale = (std::min(units / 32, 3) << 24) + ((units % 32) << 18);
  }
  long_lived[next_long].scale.fill(scale);
  return add(position, heave ? 0x181 : 11, random_heading(seed), false, terrain);
}
void craters::advance() {
  for(auto &item : short_lived) if(item.model == 0x180 && ++item.age == 5) item.model = -1;
}
} // namespace apache::simulation
