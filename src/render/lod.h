#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace apache::render {
inline int32_t lod_cutoff(int32_t const distance, double const multiplier) {
  return static_cast<int32_t>(std::clamp(distance * multiplier, double(INT32_MIN), double(INT32_MAX)));
}
inline int lod_sector_radius(double const multiplier) {
  return static_cast<int>(std::clamp(std::ceil(multiplier), 1.0, 63.0));
}
} // namespace apache::render
