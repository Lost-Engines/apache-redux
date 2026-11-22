#include "map_camera.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace apache::frontend {
namespace {
int32_t integer(long double const value) {
  return !std::isfinite(value) || value < INT32_MIN || value >= 2147483648.0L ? INT32_MIN : static_cast<int32_t>(value);
}
}
void map_camera::select(unsigned int const tool) {
  if(tool < 1 || tool > 3) throw std::runtime_error{"Invalid map camera mode"};
  mode = tool;
  pitch = tool == 1 ? 0xc0000000u : tool == 2 ? 0xd5555555u : 0u;
  camera.position[2] = tool == 1 ? 0x1f4000 : tool == 2 ? 0x3e800 : 0x6400;
}
void map_camera::step(double const seconds, math::trigonometry const &trig, std::function<int32_t(int32_t, int32_t)> const &height) {
  auto &p{camera.position};
  if(mode == 2) {
    angle = static_cast<uint32_t>(integer(static_cast<long double>(std::bit_cast<int32_t>(angle)) + static_cast<long double>(seconds) * 268435456.0L));
    p[0] = math::add(target_x, math::multiply_q31(trig.cosine(angle), 0x12c00));
    p[1] = math::add(target_y, math::multiply_q31(trig.sine(angle), 0x12c00));
    heading = 0xc0000000u - angle;
  } else {
    int32_t const dx{math::subtract(target_x, p[0])}, dy{math::subtract(target_y, p[1])};
    if(std::abs(int64_t{dx}) >= 0x10000 || std::abs(int64_t{dy}) >= 0x10000) {
      angle = static_cast<uint32_t>(integer(std::atan2(static_cast<long double>(dy), static_cast<long double>(dx)) * static_cast<long double>(0.3183098861837697) * 2147483647.0L));
      if(mode == 1) { heading = 0; p[0] = target_x; p[1] = target_y; } else {
        heading = 0x40000000u - angle;
        auto const range{math::integer_sqrt(static_cast<uint64_t>(int64_t{dx} * dx) + static_cast<uint64_t>(int64_t{dy} * dy))};
        int32_t const wanted{static_cast<int32_t>(std::min(range, 512000u))};
        speed = math::add(speed, static_cast<int32_t>(std::clamp(int64_t{wanted} - speed, int64_t{-512000}, int64_t{512000})));
        if(range <= static_cast<long double>(wanted) * seconds) { p[0] = target_x; p[1] = target_y; } else {
          p[0] = integer(p[0] + static_cast<long double>(math::multiply_q31(trig.cosine(angle), speed)) * seconds);
          p[1] = integer(p[1] + static_cast<long double>(math::multiply_q31(trig.sine(angle), speed)) * seconds);
        }
        p[2] = math::add(height(p[0], p[1]), 0x6400);
      }
    }
  }
  camera.rotation = trig.transformation(heading, pitch, roll);
}
} // namespace apache::frontend
