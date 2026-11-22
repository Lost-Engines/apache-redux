#include "terrain_shading.h"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <stdexcept>

namespace apache::render {
uint32_t approximate_range(math::vector3 const &point) {
  std::array<uint32_t, 3> magnitude;
  for(size_t i{0}; i != 3; ++i) magnitude[i] = static_cast<uint32_t>(std::abs(int64_t{point[i]}));
  std::ranges::sort(magnitude);
  return magnitude[2] + (magnitude[1] >> 2) + (magnitude[0] >> 2);
}
terrain_fog::terrain_fog(unsigned int const detail) {
  if(detail < 1 || detail > 14) throw std::runtime_error{"Terrain detail must be between 1 and 14"};
  if(detail < 3) {
    boundary = 0;
    width = detail;
  } else if(detail < 5) {
    boundary = 2;
    width = detail - 2;
  } else if(detail < 7) {
    boundary = detail - 2;
    width = 2;
  } else {
    boundary = 5;
    width = detail - 5;
  }
}
uint32_t terrain_fog::constant_colour(uint32_t const range, uint8_t colour, std::span<uint8_t const, 256> const remap) const {
  if(colour == 255) colour = 79;
  if(range < (boundary << 22)) return remap[colour] * 0x01010101u;
  if(colour < 71) colour = 115;
  uint32_t distance{static_cast<uint32_t>(static_cast<uint64_t>(static_cast<double>(range - (boundary << 22)) / (width * 0.5)))};
  unsigned int base{151};
  unsigned int level{0};
  if(distance <= 6291456) {
    level = distance >> 20;
    if(colour >= 83 && colour <= 98) {
      base = 139;
    } else if(colour >= 99 && colour <= 114) {
      base = 143;
    } else if(colour >= 115 && colour <= 126) base = 147;
  } else {
    distance = std::min(distance, width << 22);
    level = static_cast<unsigned int>(std::clamp(static_cast<int>((static_cast<double>(distance) - 6291456.0) * 2.8610229492187504e-6), 0, 6));
    base = 135;
  }
  base += level >> 1;
  if(!(level & 1)) return remap[base] * 0x01010101u;
  return (remap[base + 1] | (static_cast<uint32_t>(remap[base]) << 8)) * 0x00010001u;
}
uint32_t terrain_fog::shaded_colours(uint32_t const range, unsigned int const kind, std::span<uint8_t const, 256> const remap) const {
  std::array<uint32_t, 7> constexpr hatch{0x595a595a, 0x55565556, 0x696a696a, 0x71727172, 0x7a7b7a7b, 0x51525152, 0x85868586};
  std::array<uint8_t, 7> constexpr mapping{0xaa, 0xb4, 0xbe, 0xc8, 0xd2, 0xdc, 0xe6};
  if(kind >= hatch.size()) throw std::runtime_error{"Invalid terrain material"};
  if(range <= ((boundary - 1) << 22)) return hatch[kind];
  // The subtraction is unsigned in the original code, including beyond the
  // boundary. Its low byte selects a row in the normal intensity table.
  auto const colour{static_cast<uint8_t>(mapping[kind] + (((boundary << 22) - range) >> 19))};
  return remap[colour] * 0x01010101u;
}
unsigned int terrain_fog::texture_bank(uint32_t const range, unsigned int const terrain_mode) const {
  if(terrain_mode == 2 || range <= ((boundary - 2) << 22)) return 0;
  int const index{static_cast<int>(4.0 - static_cast<double>((boundary << 22) - range) / 2097152.0)};
  return index >= 0 && index <= 4 ? std::min(static_cast<unsigned int>(index) + 1, 4u) : 0;
}
} // namespace apache::render
