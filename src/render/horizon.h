#pragma once
#include "math/trigonometry.h"
#include "projection.h"
#include "projection2.h"
#include "scene_palette.h"

namespace apache::render {
struct sky_conditions {
  unsigned int cloud_mode{0};
  unsigned int cloud_flags{0};
  unsigned int weather{0};
  uint8_t ground_colour{71};
};
class horizon {
  framebuffer &target;
  rectangle area;
  projection view;
  projection2 plane;
  rasterizer raster;
  scene_palette const &palette;
  math::trigonometry const &trig;
  int32_t pitch{0};
  int32_t sine{0};
  int32_t cosine{INT32_MAX};
  int32_t half_width{0};
  int32_t half_height{0};
  vertex2 left;
  vertex2 right;
  std::array<int32_t, 2> extension{0, 0};
  void fill(std::array<vertex2, 4> const &points, uint8_t colour);
  horizon(framebuffer &destination, rectangle viewport, rectangle reference, scene_palette const &colours, math::trigonometry const &angles);
public:
  horizon(framebuffer &destination, rectangle viewport, scene_palette const &colours, math::trigonometry const &angles);
  void ground(uint32_t pitch_angle, uint32_t roll_angle, sky_conditions const &conditions);
  void sky(sky_conditions const &conditions);
};
} // namespace apache::render
