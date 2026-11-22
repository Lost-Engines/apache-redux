#pragma once
#include "math/geometry.h"
#include "draw_queue.h"
#include "horizon.h"
#include "simulation/effects.h"

namespace apache::render {
struct effect_visibility {
  unsigned int cloud_mode{0};
  unsigned int infra_red{0};
  int32_t cloud_bottom{6400000};
  int32_t cloud_top{6912000};
  int32_t near_depth{-262144};
  int32_t far_depth{4194304};
};
void submit_effects(draw_queue &queue, simulation::effects const &effects, math::pose const &camera, effect_visibility const &visibility);
class effect_renderer {
  std::array<resources::texture, 33> ground_explosions;
  std::array<resources::texture, 15> air_explosions;
  std::array<resources::texture, 8> smoke_images;
public:
  explicit effect_renderer(resources::data_store const &data);
  void draw_smoke(simulation::smoke const &puff, math::pose const &camera, uint32_t roll, math::trigonometry const &trig, projection const &view, rasterizer &raster) const;
  void draw_explosion(simulation::explosion const &blast, math::pose const &camera, uint32_t roll, math::trigonometry const &trig, projection const &view, rasterizer &raster) const;
};
} // namespace apache::render
