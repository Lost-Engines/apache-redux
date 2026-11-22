#include "sight_geometry.h"
#include <algorithm>
#include <cmath>

namespace apache::simulation {
namespace {
int32_t multiply(int32_t const a, int32_t const b) {
  return std::bit_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b));
}
int32_t magnitude(int32_t const a) {
  return a < 0 ? math::negate(a) : a;
}
}
math::vector3 chain_gun_impact(math::pose const &barrel, int32_t const speed, std::optional<math::vector3> const &intercept, std::function<int32_t(int32_t, int32_t)> const &height) {
  if(intercept) return *intercept;
  auto const &p{barrel.position};
  if(p[2] <= height(p[0], p[1])) return {};
  int32_t const vertical{magnitude(math::multiply_q31(barrel.rotation[5], speed))};
  int32_t const climb_time{multiply(vertical, 200) / 8192};
  int32_t const altitude{magnitude(math::subtract(math::add(p[2], multiply(climb_time, vertical) / 200), height(p[0], p[1])))};
  double const fall_time{std::sqrt(static_cast<double>(std::max(0, altitude / 256))) * 200.0 * 0.25};
  int32_t const ticks{static_cast<int32_t>(std::clamp(fall_time + climb_time, static_cast<double>(INT32_MIN), static_cast<double>(INT32_MAX)))};
  // The original returns displacement coordinates here without adding the barrel
  // origin, and uses the absolute vertical velocity for both up and down fire.
  math::vector3 result{multiply(ticks, math::multiply_q31(barrel.rotation[3], speed)) / 200, multiply(ticks, math::multiply_q31(barrel.rotation[4], speed)) / 200, 0};
  result[2] = height(result[0], result[1]); return result;
}
} // namespace apache::simulation
