#include "terrain_renderer.h"
#include <algorithm>
#include <stdexcept>

namespace apache::render {
terrain_renderer::terrain_renderer(framebuffer &target, projection const &viewport, scene_palette const &colours, resources::terrain_textures const &images) : view{viewport}, raster{target}, palette{colours}, textures{images} {}
void terrain_renderer::fill(std::span<vertex3 const> const vertices, material const &paint) {
  auto const clipped{view.clip(vertices)};
  if(clipped.empty()) return;
  std::vector<screen_vertex> polygon;
  polygon.reserve(clipped.size());
  for(auto const &point : clipped) polygon.push_back(point.screen);
  raster.fill(polygon, paint);
}
void terrain_renderer::subdivide(std::array<vertex3, 3> const &triangle, unsigned int const kind, std::span<uint8_t const> const colours) {
  std::array<uint8_t, 7> constexpr major{90, 86, 106, 114, 123, 82, 134}, minor{89, 85, 105, 113, 122, 81, 133};
  if(kind >= major.size()) throw std::runtime_error{"Invalid terrain material"};
  std::array<vertex3, 81> grid;
  grid[0] = triangle[1]; grid[8] = triangle[0]; grid[72] = triangle[2];
  auto const interpolate{[&](size_t const begin, size_t const end, size_t const stride, int const divisions, bool const edge){
    auto point{grid[begin]};
    math::vector3 delta;
    for(size_t axis{0}; axis != 3; ++axis) {
      int32_t const difference{math::subtract(grid[end].eye[axis], point.eye[axis])};
      delta[axis] = edge ? difference >> 3 : difference / divisions;
    }
    int32_t const difference{math::subtract(grid[end].screen.intensity, point.screen.intensity)};
    int32_t const light_step{edge ? difference >> 3 : difference / divisions};
    for(int i{1}; i != divisions; ++i) {
      for(size_t axis{0}; axis != 3; ++axis) point.eye[axis] = math::add(point.eye[axis], delta[axis]);
      point.screen.intensity = math::add(point.screen.intensity, light_step);
      grid[begin + static_cast<size_t>(i) * stride] = view.prepare_scaled(point.eye, point.screen.intensity);
    }
  }};
  interpolate(0, 72, 9, 8, true);
  interpolate(0, 8, 1, 8, true);
  for(size_t i{2}; i != 9; ++i) interpolate(i, i * 9, 8, static_cast<int>(i), false);
  size_t colour_index{0};
  auto const emit{[&](std::array<vertex3, 3> const &vertices){
    uint64_t intensity{0};
    for(auto const &point : vertices) intensity += static_cast<uint32_t>(point.screen.intensity);
    size_t const level{static_cast<uint8_t>((intensity / 3) >> 23)};
    uint32_t pair{0};
    if(colours.empty()) pair = palette.intensity[static_cast<size_t>(major[kind]) * 256 + level] | (static_cast<uint32_t>(palette.intensity[static_cast<size_t>(minor[kind]) * 256 + level]) << 8);
    else pair = palette.intensity[static_cast<size_t>(colours[colour_index]) * 256 + level] * 0x101u;
    ++colour_index;
    material paint;
    paint.colours = pair * 0x00010001u;
    fill(vertices, paint);
  }};
  for(size_t row{0}; row != 8; ++row) for(size_t column{0}; column != 8 - row; ++column) {
    size_t const index{row + column * 9};
    emit({grid[index + 1], grid[index], grid[index + 9]});
    if(column != 7 - row) emit({grid[index + 9], grid[index + 10], grid[index + 1]});
  }
}
void terrain_renderer::draw(resources::terrain_post const &post, std::span<vertex3 const, 4> const corners, unsigned int const half, terrain_method const method, terrain_fog const &fog, bool const two_sided) {
  if(half > 1) throw std::runtime_error{"Invalid terrain triangle"};
  bool const forward{bool(post.flags & 64)};
  std::array<std::array<unsigned int, 3>, 4> constexpr indices{{{2, 0, 1}, {1, 3, 2}, {0, 1, 3}, {3, 2, 0}}};
  auto const &order{indices[(forward ? 2 : 0) + half]};
  std::array<vertex3, 3> triangle{corners[order[0]], corners[order[1]], corners[order[2]]};
  bool clipped{false};
  for(auto const &point : triangle) clipped |= point.outcode != 0;
  auto const visible{view.clip(triangle)};
  if(visible.empty() || (!two_sided && winding(visible, clipped) < 0)) return;
  unsigned int const kind{(post.flags >> (half * 3)) & 7u}, selector{post.textures[half] & 127u};
  if(method == terrain_method::subdivided || method == terrain_method::subdivided_texture) {
    if(method == terrain_method::subdivided_texture) subdivide(triangle, kind, textures.subdivisions(forward, half, selector));
    else subdivide(triangle, kind, {});
    return;
  }
  uint32_t const range{approximate_range(view.unmagnify(corners[0].eye))};
  material paint;
  if(method == terrain_method::constant) paint.colours = fog.constant_colour(range, selector == 127 ? 127 : post.colours[half], palette.visual_remap);
  else if(method == terrain_method::gouraud) {
    paint.colours = fog.shaded_colours(range, kind, palette.visual_remap);
    paint.light = shading::gouraud;
    paint.intensity_map = palette.intensity;
  } else {
    bool const detailed{method == terrain_method::texture64 || method == terrain_method::perspective};
    paint.image = &textures.get(forward, selector, detailed);
    paint.light = shading::gouraud;
    unsigned int const bank{fog.texture_bank(range, detailed ? 3 : 2)};
    paint.intensity_map = bank ? palette.fog_intensity[bank - 1] : palette.intensity;
    int32_t const maximum{detailed ? 0x3fffff : 0x1fffff};
    for(size_t i{0}; i != 3; ++i) {
      triangle[i].screen.u = order[i] >= 2 ? maximum : 0;
      triangle[i].screen.v = order[i] % 2 == 0 ? maximum : 0;
    }
  }
  if(method == terrain_method::perspective) {
    auto const clipped_vertices{view.clip(triangle)};
    std::vector<perspective_vertex> polygon;
    for(auto const &point : clipped_vertices) polygon.push_back({point.screen, point.eye[1]});
    raster.perspective(polygon, *paint.image, palette.intensity);
  } else fill(triangle, paint);
}
} // namespace apache::render
