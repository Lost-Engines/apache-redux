#include "feature_renderer.h"
#include <algorithm>
#include <stdexcept>

namespace apache::render {
namespace {
int32_t grid_term(int32_t const coefficient, int const index, unsigned int const shift) {
  int const magnitude{index < 0 ? -index : index};
  auto const value{static_cast<int32_t>((int64_t{coefficient} * magnitude) >> shift)};
  return index < 0 ? math::negate(value) : value;
}
} // namespace
math::vector3 feature_origin(math::pose const &camera, int const x_sector, int const y_sector, bool const super) {
  int32_t const offset{super ? 0x18000000 : 0}, mask{super ? 0x3ffffff : 0x3fffff};
  unsigned int const shift{super ? 26u : 22u};
  auto const x{math::add(camera.position[0], offset)}, y{math::add(camera.position[1], offset)};
  auto result{math::transform(camera.rotation, {-(x & mask), -(y & mask), math::negate(camera.position[2])})};
  std::array<int, 2> const relative{x_sector - (x >> shift), y_sector - (y >> shift)};
  for(size_t dimension{0}; dimension != 2; ++dimension) for(size_t axis{0}; axis != 3; ++axis) {
    int32_t const coefficient{camera.rotation[axis * 3 + dimension]};
    if(super) result[axis] = math::add(result[axis], grid_term(coefficient, relative[dimension], 5));
    else {
      int const steps{relative[dimension] * 16}, high{(steps + (steps < 0 ? -128 : 128)) / 256}, low{steps - high * 256};
      result[axis] = math::add(result[axis], grid_term(coefficient, high, 5));
      result[axis] = math::add(result[axis], grid_term(coefficient, low, 13));
    }
  }
  return result;
}
std::vector<vertex3> project_feature(resources::feature const &object, math::matrix3 const &rotation, math::vector3 const &origin, projection const &view) {
  std::vector<vertex3> result;
  result.reserve(object.vertices.size());
  std::array<unsigned int, 4> constexpr shifts{5, 13, 19, 25};
  for(auto const &vertex : object.vertices) {
    auto eye{origin};
    for(size_t level{0}; level != 4; ++level) for(size_t dimension{0}; dimension != 2; ++dimension) for(size_t axis{0}; axis != 3; ++axis) eye[axis] = math::add(eye[axis], grid_term(rotation[axis * 3 + dimension], vertex.components[level][dimension], shifts[level]));
    result.push_back(view.prepare(eye));
  }
  return result;
}
feature_renderer::feature_renderer(framebuffer &destination, projection const &viewport, scene_palette const &colours) : target{destination}, view{viewport}, palette{colours}, raster{destination} {}
void feature_renderer::draw(std::span<resources::feature_face const> const faces, std::span<vertex3 const> const points, terrain_fog const &fog) {
  for(auto const &face : faces) {
    if(face.indices.empty()) continue;
    for(auto const index : face.indices) if(index >= points.size()) throw std::runtime_error{"Terrain feature references a missing projected vertex"};
    uint8_t const colour{palette.visual_remap[face.colours[0]]};
    auto const fogged{[&](vertex3 const &point){ return fog.constant_colour(approximate_range(view.unmagnify(point.eye)), colour, palette.visual_remap); }};
    if(face.type == 0) {
      for(auto const index : face.indices) if(!points[index].outcode) target.point(points[index].screen.x, points[index].screen.y, static_cast<uint8_t>(fogged(points[index])));
    } else if(face.type == 1) {
      uint8_t const line_colour{static_cast<uint8_t>(fogged(points[face.indices[0]]))};
      for(size_t i{1}; i != face.indices.size(); ++i) {
        auto const line{view.clip_line(points[face.indices[i - 1]], points[face.indices[i]])};
        if(line) raster.line((*line)[0].screen, (*line)[1].screen, line_colour);
      }
    } else {
      std::vector<vertex3> polygon;
      for(auto const index : face.render_indices) polygon.push_back(points[index]);
      material paint;
      if(face.type == 2) {
        // The original checks each vertex before following its next link;
        // the final vertex does not participate in the minimum fog range.
        uint32_t range{approximate_range(view.unmagnify(points[face.indices[0]].eye))};
        for(size_t i{0}; i + 1 < face.indices.size(); ++i) range = std::min(range, approximate_range(view.unmagnify(points[face.indices[i]].eye)));
        paint.colours = fog.constant_colour(range, colour, palette.visual_remap);
      } else if(face.type == 17) {
        std::array<uint8_t, 256> levels{};
        for(size_t i{0}; i != face.indices.size(); ++i) levels[face.indices[i]] = face.intensities[i];
        for(size_t i{0}; i != polygon.size(); ++i) polygon[i].screen.intensity = levels[face.render_indices[i]];
        paint.colours = 0x85868586; paint.light = shading::gouraud; paint.intensity_map = palette.intensity;
      } else {
        paint.colours = (colour | (static_cast<uint32_t>(palette.visual_remap[face.colours[1]]) << 8)) * 0x00010001u;
        if(face.type == 4) paint.pattern = fill_pattern::fine;
        if(face.type == 5) paint.pattern = fill_pattern::coarse;
      }
      auto const clipped{view.clip(polygon)};
      std::vector<screen_vertex> screen;
      for(auto const &point : clipped) screen.push_back(point.screen);
      raster.fill(screen, paint);
    }
  }
}
} // namespace apache::render
