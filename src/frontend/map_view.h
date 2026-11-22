#pragma once
#include <vector>
#include "math/trigonometry.h"
#include "render/projection2.h"

namespace apache::frontend {
struct map_position {
  int32_t x{0x7ffffff};
  int32_t y{0x7ffffff};
  int32_t zoom{0x33333};
  uint32_t rotation{0};
};
class map_view {
  std::vector<map_position> history;
  void restrict();
public:
  map_position position;
  void set(int32_t x, int32_t y, int32_t zoom, uint32_t rotation);
  void centre(int32_t x, int32_t y);
  void zoom(int32_t scale);
  void rotate(uint32_t angle);
  void push();
  void pop();
  void clear() {
    history.clear();
  }
  size_t history_size() const {
    return history.size();
  }
  void fit(std::span<render::vertex2 const> points);
  render::window2 window(int width = 640, int height = 480) const;
  render::projection2 projection(math::trigonometry const &trig, int width = 640, int height = 480) const;
};
} // namespace apache::frontend
