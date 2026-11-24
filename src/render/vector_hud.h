#pragma once
#include <array>
#include "display_layout.h"

namespace apache::render {
// Helmet geometry is kept separate from the authored bitmap lettering.
class vector_hud {
  framebuffer &target;
  hud_mapping mapping;
public:
  vector_hud(framebuffer &target, hud_mapping mapping, uint8_t colour);
  void line(std::array<double, 2> const &a, std::array<double, 2> const &b, rectangle clip, uint32_t pattern = UINT32_MAX);
};
} // namespace apache::render
