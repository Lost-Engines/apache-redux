#pragma once
#include <cstdint>
#include <vector>
#include "resources/image.h"

namespace apache::render {
struct rectangle {
  int left{0};
  int top{0};
  int right{-1};
  int bottom{-1};
  bool contains(int x, int y) const;
};
rectangle intersect(rectangle a, rectangle b);
struct rgba {
  uint8_t r{0};
  uint8_t g{0};
  uint8_t b{0};
  uint8_t a{255};
};
class framebuffer {
public:
  resources::indexed_image image;
  rectangle clip;
  double horizontal_fov{90};
  std::vector<uint8_t> coverage;
  void mark(size_t index) { if(!coverage.empty()) coverage[index] = 1; }
  framebuffer(unsigned int width, unsigned int height);
  void reset_clip();
  void clear(uint8_t colour = 0);
  void point(int x, int y, uint8_t colour);
  void fill(rectangle area, uint8_t colour);
  void surround(rectangle area, bool pressed = false);
  void blit(resources::indexed_image const &source, int x, int y, bool transparent = false);
  void colour_pixels(std::vector<rgba> &pixels) const;
  std::vector<rgba> colour_pixels() const;
};
} // namespace apache::render
