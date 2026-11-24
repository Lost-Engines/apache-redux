#pragma once
#include <array>
#include <optional>
#include <span>
#include <vector>
#include "rasterizer.h"

namespace apache::render {
struct vertex2 {
  int32_t x{0};
  int32_t y{0};
  int32_t intensity{0};
};
struct window2 {
  int32_t left{0};
  int32_t bottom{0};
  int32_t right{0};
  int32_t top{0};
};
class projection2 {
  rectangle viewport;
  window2 window;
  std::array<int32_t, 2> centre{0, 0};
  std::array<int32_t, 2> scale{0, 0};
  uint8_t outcode(vertex2 const &point) const;
  vertex2 transform(vertex2 const &point) const;
  screen_vertex screen(vertex2 const &point) const;
  bool clip_endpoint(vertex2 &point, vertex2 const &other) const;
public:
  std::array<int32_t, 2> origin{0, 0};
  int32_t cosine{INT32_MAX};
  int32_t sine{0};
  projection2(rectangle viewport, window2 window);
  int32_t radius(int32_t world_radius) const;
  vertex2 unproject(int32_t x, int32_t y) const;
  screen_vertex project_raw(vertex2 const &point) const;
  std::optional<std::array<screen_vertex, 2>> clip_line_raw(vertex2 const &a, vertex2 const &b) const;
  std::vector<screen_vertex> clip_raw(std::span<vertex2 const> vertices) const;
  screen_vertex project(vertex2 const &point) const;
  auto project_precise(vertex2 const &point) const->std::array<double, 2>;
  std::optional<std::array<screen_vertex, 2>> clip_line(vertex2 const &a, vertex2 const &b) const;
  std::vector<screen_vertex> clip(std::span<vertex2 const> vertices) const;
};
} // namespace apache::render
