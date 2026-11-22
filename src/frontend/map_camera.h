#pragma once
#include <functional>
#include "math/geometry.h"
#include "math/trigonometry.h"

namespace apache::frontend {
class map_camera {
public:
  math::pose camera{{0x8200000, 0x8200000, 256000}, {}};
  int32_t target_x{0};
  int32_t target_y{0};
  int32_t speed{0};
  uint32_t heading{0};
  uint32_t pitch{0};
  uint32_t roll{0};
  uint32_t angle{0};
  unsigned int mode{3};
  void select(unsigned int tool);
  void target(int32_t x, int32_t y) {
    target_x = x;
    target_y = y;
  }
  void position(int32_t x, int32_t y) {
    camera.position[0] = x;
    camera.position[1] = y;
  }
  void step(double seconds, math::trigonometry const &trig, std::function<int32_t(int32_t, int32_t)> const &height);
};
} // namespace apache::frontend
