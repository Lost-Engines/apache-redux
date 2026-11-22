#pragma once
#include <functional>
#include <optional>
#include "math/geometry.h"
#include "math/trigonometry.h"

namespace apache::simulation {
struct camera_input { int32_t turn{0}, pitch{0}, zoom{0}, delta{0}; };
struct orbit_camera { uint32_t heading{0}; int32_t pitch{-0xaaaaaab}, zoom{0}; };
struct look_camera { int32_t heading{0}, pitch{0}; };
class camera_geometry {
  math::trigonometry const &trig;
  std::function<int32_t(int32_t, int32_t)> height;
public:
  camera_geometry(math::trigonometry const &trig, std::function<int32_t(int32_t, int32_t)> height) : trig{trig}, height{std::move(height)} {}
  math::pose head(math::pose body, uint32_t heading, uint32_t pitch = 0) const;
  math::pose tracking(orbit_camera &state, math::pose const &body, uint32_t model, int32_t radius, camera_input const &input) const;
  math::pose spectator(orbit_camera &state, math::vector3 const &position, camera_input const &input) const;
  math::pose remote(math::vector3 const &position, math::vector3 const &subject) const;
  math::pose missile_target(math::vector3 const &missile, math::vector3 const &target, int32_t target_height) const;
  math::pose target_lock(math::vector3 const &body, math::vector3 const &target) const;
  math::pose crash(orbit_camera &state, math::vector3 const &position, bool airborne, int32_t delta) const;
  math::pose helmet(look_camera &state, math::pose const &body, camera_input const &input, std::optional<math::vector3> const &locked) const;
  math::pose goggles(look_camera &state, math::pose const &body, camera_input const &input, std::optional<math::vector3> const &locked, int32_t rate, int32_t distance) const;
  void magnify(math::pose &view, int32_t distance) const;
};
} // namespace apache::simulation
