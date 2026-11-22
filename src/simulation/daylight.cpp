#include "daylight.h"
#include <algorithm>

namespace apache::simulation {
daylight::daylight(resources::executable_data const &executable, int32_t const time, int32_t const now)
  : mission_time{time},
    start{now},
    next_update{math::add(now, 12000)} {
  resources::reader initial{executable.at(0x4d9a18, 12), "Initial sun and light"};
  light.heading = initial.u32();
  light.pitch = initial.u32();
  light.night = initial.i32();
  set(time);
}
void daylight::set(int32_t const time) {
  if(time <= 0x2bf200 || time >= 0xf1b300) {
    light.night = INT32_MAX;
  } else if(time >= 0x57e400 && time <= 0xc5c100) {
    light.night = 0;
  } else {
    light.night = static_cast<int32_t>(int64_t{time < 0x57e400 ? 0x57e400 - time : time - 0xc5c100} * 0x80000000ll / 0x2bf200);
  }
  if(time < 0x41eb00 || time > 0xdbba00) return;
  if(time < 0x43bfc0) {
    light.heading = 0xaaaaaaab;
    light.pitch = 0;
    return;
  }
  if(time > 0xd9e540) {
    light.heading = 0x55555555;
    light.pitch = 0;
    return;
  }
  bool const morning{time < 0x8ed280};
  int32_t const ratio{static_cast<int32_t>(int64_t{morning ? 0x8ed280 - time : time - 0x8ed280} * 0x80000000ll / 0x4ce780)};
  int32_t const heading{math::multiply_q31(ratio, -0x55555555)};
  light.heading = static_cast<uint32_t>(morning ? heading : math::negate(heading));
  light.pitch = static_cast<uint32_t>(-std::clamp(0x40000000 - math::multiply_q31(ratio, 0x40000000), 0, 0x2aaaaaab));
}
void daylight::update(int32_t const now) {
  if(now < next_update) return;
  set(std::clamp(math::add(mission_time, math::subtract(now, start)), 0, 0x107ac00));
  next_update = math::add(now, 12000);
}
} // namespace apache::simulation
