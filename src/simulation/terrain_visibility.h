#pragma once
#include <functional>
#include <optional>
#include "math/geometry.h"

namespace apache::simulation {
struct visibility_result {
  bool clear{true};
  std::optional<math::vector3> last_visible;
};
visibility_result terrain_visibility(math::vector3 const &start, math::vector3 const &end, unsigned int precision, bool weighted, int32_t maximum_height, std::function<int32_t(int32_t, int32_t)> const &height);
} // namespace apache::simulation
