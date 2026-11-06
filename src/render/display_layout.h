#pragma once
#include "framebuffer.h"
namespace apache::render {
// The authored cockpit stays proportional; the world extends into widescreen space.
enum class horizontal_anchor { centre, left, right };
class display_layout {
  unsigned int source_width, source_height;
public:
  rectangle artwork, scene;
  double instrument_fov;
  display_layout(unsigned int width, unsigned int height, unsigned int logical_width,
                 unsigned int logical_height, rectangle viewport, double fov, horizontal_anchor anchor = horizontal_anchor::centre);
  void background(framebuffer const &world, framebuffer &overlay) const;
  void composite(framebuffer const &overlay, framebuffer &world) const;
};
} // namespace apache::render
