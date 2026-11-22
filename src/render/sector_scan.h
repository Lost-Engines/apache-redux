#pragma once
#include "math/geometry.h"
#include "projection.h"

namespace apache::render {
struct sector_bounds {
  int left{INT32_MAX};
  int bottom{INT32_MAX};
  int right{INT32_MIN};
  int top{INT32_MIN};
  bool empty() const {
    return left > right || bottom > top;
  }
};
sector_bounds scan_sectors(math::pose const &camera, projection const &view, uint32_t heading, unsigned int distance, bool super);
unsigned int sector_visibility(math::matrix3 const &rotation, math::vector3 const &origin, projection const &view, bool super);
} // namespace apache::render
