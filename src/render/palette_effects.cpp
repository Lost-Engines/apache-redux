#include "palette_effects.h"
#include <algorithm>
#include <stdexcept>
#include <vector>
#include "math/fixed.h"

namespace apache::render {
uint8_t luminance(uint8_t const red, uint8_t const green, uint8_t const blue) {
  return static_cast<uint8_t>((uint64_t{red} * 0x26666666 + uint64_t{green} * 0x4b851eb7 + uint64_t{blue} * 0x0e147ae1 + 0x40000000) >> 31);
}
palette_rgb apply_faders(palette_rgb colours, std::span<palette_fader const> const faders) {
  std::vector<palette_fader> ordered{faders.begin(), faders.end()};
  std::stable_sort(ordered.begin(), ordered.end(), [](auto const &a, auto const &b){return a.priority < b.priority;});
  for(auto const &fader : ordered) {
    if(fader.first > 256 || fader.count > 256 - fader.first) throw std::runtime_error{"Palette fader range exceeds 256 colours"};
    if(fader.level <= 0) continue;
    for(size_t i{fader.first}; i != fader.first + fader.count; ++i) {
      if(fader.monochrome) {
        auto const value{luminance(colours[i * 3], colours[i * 3 + 1], colours[i * 3 + 2])};
        for(size_t channel{0}; channel != 3; ++channel) colours[i * 3 + channel] = fader.colour[channel] ? value : 0;
      } else {
        for(size_t channel{0}; channel != 3; ++channel) {
          int32_t const original{static_cast<int32_t>(colours[i * 3 + channel]) << 16};
          int32_t const target{static_cast<int32_t>(fader.colour[channel]) << 16};
          colours[i * 3 + channel] = static_cast<uint8_t>((original + math::multiply_q31(target - original, fader.level)) >> 16);
        }
      }
    }
  }
  return colours;
}
} // namespace apache::render
