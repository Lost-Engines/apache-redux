#include "celestial.h"
#include <bit>
#include <optional>
#include <stdexcept>
#include <string>

namespace apache::render {
std::vector<star> read_stars(std::span<uint8_t const> const data) {
  if(data.size() % 4) throw std::runtime_error{"Star map length is not a multiple of four"};
  std::vector<star> result;
  result.reserve(data.size() / 4);
  for(size_t i{0}; i < data.size(); i += 4) {
    star const point{data[i], {data[i + 1], data[i + 2], data[i + 3]}};
    if(point.colour >= 24 || point.grid[0] > 64 || point.grid[1] > 64 || point.grid[2] > 64) throw std::runtime_error{"Invalid star colour or grid coordinate"};
    result.push_back(point);
  }
  return result;
}
math::vector3 light_vector(math::trigonometry const &trig, sky_lighting const &lighting) {
  int32_t const cosine{trig.cosine(lighting.pitch)};
  return {math::negate(math::multiply_q31(trig.sine(lighting.heading), cosine)), math::negate(math::multiply_q31(trig.cosine(lighting.heading), cosine)), math::negate(trig.sine(lighting.pitch))};
}
celestial::celestial(resources::data_store const &data, std::string_view const zone) : stars{read_stars(data.read(std::string{zone} + ".WZ/STARMAP.BIN"))} {}
int32_t celestial::draw(framebuffer &target, math::matrix3 const &camera, math::trigonometry const &trig, scene_palette const &palette, sky_conditions const &conditions, sky_lighting const &lighting) const {
  if((conditions.cloud_flags & 2) && conditions.cloud_mode != 2) return 0;
  int const width{target.clip.right - target.clip.left + 1}, height{target.clip.bottom - target.clip.top + 1};
  projection const view{target};
  bool const daytime{lighting.night <= 644245094};
  if(!daytime) {
    std::array<std::array<int32_t, 65>, 9> grids;
    for(size_t axis{0}; axis != 9; ++axis) grids[axis] = math::subdivide_grid(camera[axis], 19);
    std::array<int32_t, 24> constexpr thresholds{0, 0, 0x20000000, 0x40000000, 0x60000000, 0, 0x20000000, 0x40000000};
    for(auto const &point : stars) {
      if(lighting.night < thresholds[point.colour]) continue;
      math::vector3 eye;
      for(size_t axis{0}; axis != 3; ++axis) eye[axis] = math::add(math::add(grids[axis * 3][point.grid[0]], grids[axis * 3 + 1][point.grid[1]]), grids[axis * 3 + 2][point.grid[2]]);
      auto const projected{view.prepare(eye)};
      if(!projected.outcode) target.point(projected.screen.x, projected.screen.y, palette.visual_remap[point.colour + 168]);
    }
  }
  if(std::bit_cast<int32_t>(lighting.pitch) > -0x71c71c7) return 0;
  auto const light{light_vector(trig, lighting)};
  int32_t glare{0};
  if(daytime) {
    auto const depth{math::transform(camera, light)[1]};
    if(depth > 0x78000000) glare = std::bit_cast<int32_t>(static_cast<uint32_t>(depth) * 16u - 0x80000000u);
  }
  math::vector3 direction;
  for(size_t i{0}; i != 3; ++i) direction[i] = light[i] >> 8;
  auto const eye{view.prepare(math::transform(camera, direction)).eye};
  if(eye[1] < 1) return glare;
  auto const horizontal{[&](int32_t const x) -> std::optional<int> {
    int64_t const position{((int64_t{x} + eye[1]) * width >> 1) / eye[1] + target.clip.left};
    if(position < INT32_MIN || position > INT32_MAX) return {};
    return static_cast<int>(position);
  }};
  auto const x{horizontal(eye[0])};
  int64_t const y{target.clip.bottom - (((int64_t{eye[2]} + eye[1]) * height >> 1) / eye[1])};
  // Keep the sun visible at every output resolution and field of view.
  if(!x || *x < target.clip.left - width || *x > target.clip.right + width || y < target.clip.top - height || y > target.clip.bottom + height) return glare;
  auto const right{horizontal(math::add(eye[0], static_cast<int32_t>(500000 * view.lateral_scale())))};
  if(!right) return glare;
  rasterizer raster{target};
  material paint;
  paint.colours = palette.visual_remap[191] * 0x01010101u;
  raster.circle(*x, static_cast<int>(y), *right - *x, paint);
  return glare;
}
} // namespace apache::render
