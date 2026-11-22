#include "fragments.h"
#include <stdexcept>

namespace apache::simulation {
void fragments::reset() {
  for(auto &piece : pieces) piece.active = false;
  next = 0;
}
void fragments::create(uint32_t const model, uint32_t const flags, math::pose const &position, uint32_t &random_seed) {
  auto const random{[&]{
    random_seed = random_seed * 0x41c64e6du + 0x3039u;
    return random_seed & 0x7fff0000u;
  }};
  auto &piece{pieces[next]};
  piece.active = true;
  piece.model = model;
  piece.flags = flags;
  piece.position = position;
  for(auto &v : piece.axis) v = std::bit_cast<int32_t>(random() + 0xc0000000u);
  piece.axis = math::normalise_q31(piece.axis);
  piece.spin = std::bit_cast<int32_t>(random() * 2u);
  piece.velocity[0] = std::bit_cast<int32_t>(random() + 0xc0000000u);
  piece.velocity[1] = std::bit_cast<int32_t>(random() + 0xc0000000u);
  piece.velocity[2] = static_cast<int32_t>(random() / 2);
  piece.velocity = math::normalise_q31(piece.velocity);
  auto const speed{static_cast<int32_t>((uint64_t{random()} * 0x9600u + 0x80000000ull) >> 32)};
  for(auto &v : piece.velocity) v = math::multiply_q31(v, speed);
  next = (next + 1) % pieces.size();
}
void fragments::advance(int32_t const delta_time, math::trigonometry const &trig, resources::terrain const &terrain, std::function<uint8_t(int32_t, int32_t)> const &surface, craters &splashes, uint32_t &random_seed) {
  if(delta_time < 0) throw std::runtime_error{"Fragment time cannot run backwards"};
  for(auto &piece : pieces) {
    if(!piece.active) continue;
    auto const angle{static_cast<uint32_t>(math::multiply_q31(piece.spin, delta_time))};
    piece.position.rotation = math::multiply(trig.rotation(piece.axis, angle), piece.position.rotation);
    piece.velocity[2] = math::subtract(piece.velocity[2], math::multiply_q31(delta_time, 8192));
    auto &position{piece.position.position};
    for(size_t i{0}; i != 3; ++i) position[i] = math::add(position[i], math::multiply_q31(piece.velocity[i], delta_time));
    auto const height{terrain.elevation(position[0], position[1])};
    if(position[2] <= height) {
      piece.spin = 0;
      piece.velocity.fill(0);
      position[2] = height;
      if(height == 0) {
        auto const type{surface(position[0], position[1])};
        if(type >= 1 && type <= 3) {
          piece.active = false;
          if(position[0] >= 0 && position[0] < 0x10000000 && position[1] >= 0 && position[1] < 0x10000000) splashes.add_random(position, 0x180, true, terrain, random_seed);
        }
      }
    }
  }
}
} // namespace apache::simulation
