#pragma once
#include <functional>
#include <memory>
#include "math/geometry.h"
#include "projection.h"
#include "resources/models.h"
#include "scene_palette.h"

namespace apache::render {
struct model_instance {
  uint32_t id{0};
  uint32_t flags{0}; // Drawing-program animation bits.
  uint32_t status{0}; // Mobile-object allegiance and destroyed/exclusion bits.
  bool alternate_colours{false};
  int32_t distance{0};
  math::pose relative;
  math::pose world;
  math::vector3 scale{1 << 24, 1 << 24, 1 << 24};
  std::vector<std::unique_ptr<model_instance>> children;
  std::function<void(model_instance const &)> maintained;
};
model_instance construct_model(resources::model_archive const &models, uint32_t id, math::pose const &position, std::span<uint32_t const> variable_children = {});
model_instance construct_scenery_model(resources::model_archive const &models, uint32_t id, math::pose const &position);
void maintain_model(model_instance &object, math::pose const *parent = nullptr);
void synchronize_model(model_instance &destination, model_instance const &source);
struct model_style {
  double lod_distance_multiplier{1};
  bool textures{true};
  bool gouraud{true};
  bool glass{true};
  uint8_t heat_phase{0};
  math::vector3 light{0, 0, INT32_MAX};
};
class model_renderer {
  framebuffer &target;
  rasterizer raster;
  projection view;
  resources::model_archive const &models;
  std::vector<std::optional<resources::texture>> const &textures;
  scene_palette const &palette;
  void draw_instance(model_instance &object, math::pose const &camera, unsigned int depth, bool alternate);
  int draw_face(resources::primitive const &face, std::span<vertex3 const> vertices, std::span<int32_t const> intensities, bool secondary, bool alternate);
public:
  model_style style;
  size_t missing_normals{0};
  size_t invalid_texture_samples{0};
  std::function<void(uint32_t, uint32_t)> face_observer;
  model_renderer(framebuffer &destination, resources::model_archive const &objects, std::vector<std::optional<resources::texture>> const &images, scene_palette const &colours);
  void draw(model_instance &object, math::pose const &camera);
};
} // namespace apache::render
