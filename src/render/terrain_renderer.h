#pragma once
#include <array>
#include "projection.h"
#include "resources/terrain.h"
#include "resources/terrain_textures.h"
#include "scene_palette.h"
#include "terrain_shading.h"

namespace apache::render {
enum class terrain_method { constant, gouraud, texture32, texture64, subdivided, subdivided_texture, perspective };
class terrain_renderer {
  projection const &view;
  rasterizer raster;
  scene_palette const &palette;
  resources::terrain_textures const &textures;
  void fill(std::span<vertex3 const> vertices, material const &paint);
  void subdivide(std::array<vertex3, 3> const &triangle, unsigned int kind, std::span<uint8_t const> colours);
public:
  terrain_renderer(framebuffer &target, projection const &viewport, scene_palette const &colours, resources::terrain_textures const &images);
  void draw(resources::terrain_post const &post, std::span<vertex3 const, 4> corners, unsigned int half, terrain_method method, terrain_fog const &fog, bool two_sided = false);
  size_t invalid_texture_samples() const {
    return raster.invalid_texture_samples;
  }
};
} // namespace apache::render
