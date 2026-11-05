#pragma once
#include <optional>
#include <span>
#include <vector>
#include "math/fixed.h"
#include "rasterizer.h"

namespace apache::render {
struct vertex3 {
  math::vector3 eye{0, 0, 0};
  screen_vertex screen;
  uint8_t outcode{0};
};
uint8_t outcode(math::vector3 const &point);
int winding(std::span<vertex3 const> vertices, bool robust = false);
class projection {
  rectangle viewport;
  int width{0};
  int height{0};
  int32_t vertical_scale{0};
  int32_t inverse_vertical_scale{0};
  double horizontal_factor{1}, vertical_factor{1};
  bool original_scale{true};
  math::vector3 scale(math::vector3 eye) const;
  void project(vertex3 &point) const;
public:
  projection(int x, int y, int w, int h, double horizontal_fov = 90);
  double lateral_scale() const { return horizontal_factor; }
  vertex3 prepare(math::vector3 eye, int32_t u = 0, int32_t v = 0, int32_t intensity = 0) const;
  std::optional<screen_vertex> project_unclipped(math::vector3 eye) const;
  math::vector3 unscale(math::vector3 eye) const;
  math::vector3 unmagnify(math::vector3 eye) const;
  vertex3 prepare_scaled(math::vector3 const &eye, int32_t intensity = 0) const;
  std::optional<std::array<vertex3, 2>> clip_line(vertex3 a, vertex3 b) const;
  std::vector<vertex3> clip(std::span<vertex3 const> vertices) const;
};
} // namespace apache::render
