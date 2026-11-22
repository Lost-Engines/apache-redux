#pragma once
#include <array>
#include <functional>
#include "math/trigonometry.h"
#include "craters.h"

namespace apache::simulation {
struct fragment {
  bool active{false};
  uint32_t model{0};
  uint32_t flags{0};
  math::pose position;
  math::vector3 axis{0, 0, 0};
  int32_t spin{0};
  math::vector3 velocity{0, 0, 0};
};
class fragments {
  size_t next{0};
public:
  std::array<fragment, 64> pieces;
  void reset();
  void create(uint32_t model, uint32_t flags, math::pose const &position, uint32_t &random_seed);
  void advance(int32_t delta_time, math::trigonometry const &trig, resources::terrain const &terrain, std::function<uint8_t(int32_t, int32_t)> const &surface, craters &splashes, uint32_t &random_seed);
};
} // namespace apache::simulation
