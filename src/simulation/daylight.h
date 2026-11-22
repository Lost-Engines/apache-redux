#pragma once
#include "render/celestial.h"
#include "resources/executable.h"

namespace apache::simulation {
class daylight {
  int32_t mission_time{0}, start{0}, next_update{0};
public:
  render::sky_lighting light;
  daylight(resources::executable_data const &executable, int32_t mission_time, int32_t now = 0);
  void set(int32_t time);
  void update(int32_t now);
  bool night_vision() const {
    return static_cast<double>(light.night) > 1503238552.9;
  }
};
} // namespace apache::simulation
