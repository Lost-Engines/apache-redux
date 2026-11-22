#pragma once
#include <functional>
#include <optional>
#include "math/geometry.h"

namespace apache::simulation {
math::vector3 chain_gun_impact(math::pose const &barrel, int32_t speed, std::optional<math::vector3> const &intercept, std::function<int32_t(int32_t, int32_t)> const &height);
} // namespace apache::simulation
