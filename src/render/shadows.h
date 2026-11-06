#pragma once
#include <functional>
#include <optional>
#include "celestial.h"
#include "model_renderer.h"
#include "resources/terrain.h"

namespace apache::render {
struct shadow_plane {
  math::vector3 position;
  int32_t height{0};
};
class shadow_projection {
  resources::terrain const &terrain;
public:
  std::array<int32_t, 2> cast;
  unsigned int cloud_mode{0};
  int32_t cloud_top{6912000};
  std::function<std::optional<int32_t>(math::vector3 const&)> receiver;
  shadow_projection(resources::terrain const &heights, math::trigonometry const &trig, sky_lighting const &light);
  shadow_plane project(math::vector3 const &position, bool check_receivers = false) const;
};
class shadow_renderer {
  resources::model_archive const &models;
  shadow_projection const &casting;
  scene_palette const &palette;
  rasterizer raster;
  projection view;
  void draw_instance(model_instance &object, math::pose const &camera, unsigned int depth);
public:
  double lod_distance_multiplier{1};
  shadow_renderer(framebuffer &target, resources::model_archive const &objects, shadow_projection const &cast, scene_palette const &colours);
  void draw(model_instance &object, math::pose const &camera);
};
} // namespace apache::render
