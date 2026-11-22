#include <algorithm>
#include <stdexcept>
#include "landscape.h"

namespace apache::render {
namespace {
int32_t grid_term(int32_t const coefficient, int const index, unsigned int const shift) {
  auto const value{static_cast<int32_t>((int64_t{coefficient} * std::abs(index)) >> shift)};
  return index < 0 ? math::negate(value) : value;
}
uint8_t map_colour(uint8_t const colour) {
  return colour == 11 ? 202 : colour == 111 || colour == 127 ? 200 : 2;
}
uint8_t outcode(vertex2 const &point, window2 const &window) {
  return static_cast<uint8_t>((point.y > window.top ? 8 : point.y < window.bottom ? 4 : 0) | (point.x > window.right ? 2 : point.x < window.left ? 1 : 0));
}
} // anonymous namespace
void draw_map_contours(framebuffer &target, projection2 const &view, std::array<vertex2, 3> triangle, int32_t const interval) {
  if(interval < 10) throw std::runtime_error{"Map contour interval must be at least ten feet"};
  int32_t const quantum{interval / 10};
  int32_t const spacing{interval / quantum};
  for(auto &point : triangle) point.intensity /= quantum;
  auto &[a, b, c]{triangle};
  if(a.intensity > c.intensity) std::swap(a, c);
  if(b.intensity > c.intensity) std::swap(b, c);
  if(a.intensity > b.intensity) std::swap(a, b);
  if(a.intensity == c.intensity) return;
  if(b.intensity == c.intensity) std::swap(b, c);
  auto const step{[](vertex2 const &from, vertex2 const &to, int32_t const height){
    return vertex2{math::subtract(to.x, from.x) / height, math::subtract(to.y, from.y) / height};
  }};
  auto const advance{[](vertex2 &point, vertex2 const &delta){
    point.x = math::add(point.x, delta.x);
    point.y = math::add(point.y, delta.y);
  }};
  rasterizer raster{target};
  auto edge{a};
  auto other{a};
  auto const long_step{step(a, c, c.intensity - a.intensity)};
  auto const segment{[&](int32_t const first, int32_t const last, vertex2 const &short_step){
    int32_t countdown{spacing - 1 - (first - 1) % spacing};
    for(int32_t z{first}; z < last; ++z) {
      if(!countdown) {
        if(auto const line{view.clip_line_raw(edge, other)}) raster.line((*line)[0], (*line)[1], 199);
        countdown = spacing;
      }
      --countdown;
      advance(edge, long_step);
      advance(other, short_step);
    }
  }};
  if(a.intensity == b.intensity) {
    other = b;
    segment(a.intensity, c.intensity, step(b, c, c.intensity - b.intensity));
  } else {
    segment(a.intensity, b.intensity, step(a, b, b.intensity - a.intensity));
    if(b.intensity != c.intensity) segment(b.intensity, c.intensity, step(other, c, c.intensity - b.intensity));
  }
}
void landscape::draw_map(framebuffer &target, rectangle const area, window2 const window, uint32_t const heading, map_configuration const &configuration, std::span<resources::scenery_placement const> const extra) const {
  target.clip = area;
  target.fill(area, 192);
  projection2 view{area, window};
  view.cosine = trig.cosine(heading);
  view.sine = trig.sine(heading);
  int32_t const cx{static_cast<int32_t>((int64_t{window.left} + window.right) / 2)};
  int32_t const cy{static_cast<int32_t>((int64_t{window.bottom} + window.top) / 2)};
  rasterizer raster{target};
  auto const origin{[&](int32_t const x, int32_t const y){
    auto const dx{math::subtract(x, cx)};
    auto const dy{math::subtract(y, cy)};
    return vertex2{math::add(math::rounded_q31(math::product_bits(dx, view.cosine) - math::product_bits(dy, view.sine)), cx), math::add(math::rounded_q31(math::product_bits(dy, view.cosine) + math::product_bits(dx, view.sine)), cy)};
  }};
  auto const grid{[&](vertex2 const &base, int const x, int const y, unsigned int const shift){
    return vertex2{math::add(base.x, math::add(grid_term(view.cosine, x, shift), grid_term(math::negate(view.sine), y, shift))), math::add(base.y, math::add(grid_term(view.sine, x, shift), grid_term(view.cosine, y, shift)))};
  }};
  auto const visible{[&](vertex2 const &base, bool const super){
    uint8_t common{255};
    for(auto const &[x, y] : std::array<std::array<int, 2>, 4>{{{0, 0}, {1, 0}, {1, 1}, {0, 1}}}) common &= outcode(grid(base, x * (super ? 1 : 16), y * (super ? 1 : 16), super ? 5 : 13), window);
    return !common;
  }};
  auto const feature{[&](resources::feature_layer const &layer, int const x, int const y, vertex2 const &base){
    auto const id{layer.placements.at(static_cast<size_t>(x * (layer.super ? 16 : 64) + y))};
    if(!id) return;
    auto const &record{layer.records.at(id).value()};
    std::vector<vertex2> points;
    points.reserve(record.vertices.size());
    std::array<unsigned int, 4> constexpr shifts{5, 13, 19, 25};
    for(auto const &vertex : record.vertices) {
      auto point{base};
      for(size_t level{0}; level != 4; ++level) point = grid(point, vertex.components[level][0], vertex.components[level][1], shifts[level]);
      points.push_back(point);
    }
    for(auto const &face : record.faces) {
      uint8_t const colour{map_colour(face.colours[0])};
      if(face.type == 0) {
        for(auto const index : face.indices) {
          auto const &point{points.at(index)};
          if(!outcode(point, window)) {
            auto const pixel{view.project_raw(point)};
            target.point(pixel.x, pixel.y, colour);
          }
        }
      } else if(face.type == 1) {
        for(size_t i{1}; i < face.indices.size(); ++i) {
          if(auto const line{view.clip_line_raw(points.at(face.indices[i - 1]), points.at(face.indices[i]))}) raster.line((*line)[0], (*line)[1], colour);
        }
      } else {
        if(face.type > 5) throw std::runtime_error{"Unsupported map feature drawing mode"};
        std::vector<vertex2> polygon;
        for(auto const index : face.render_indices) polygon.push_back(points.at(index));
        material paint;
        paint.colours = colour * 0x01010101u;
        if(face.type == 3) paint.colours = (colour | (static_cast<uint32_t>(map_colour(face.colours[1])) << 8)) * 0x00010001u;
        if(face.type == 4) paint.pattern = fill_pattern::fine;
        if(face.type == 5) paint.pattern = fill_pattern::coarse;
        raster.fill(view.clip_raw(polygon), paint);
      }
    }
  }};
  auto const object{[&](resources::scenery_placement const &placement){
    if(!placement.model || (placement.status & 1) || !(placement.status & 6)) return;
    auto const &model{models.at(placement.model).value()};
    if(!(model.metadata[7] & 1)) return;
    uint8_t const colour{static_cast<uint8_t>(204 + ((placement.status & 7) >> 1))};
    if(!(configuration.flags & 8)) {
      auto const point{view.project({placement.x, placement.y})};
      target.point(point.x, point.y, colour);
      return;
    }
    if(model.shadow.empty()) return;
    resources::reader shape{model.shadow, "map object footprint"};
    auto const count{shape.u16()};
    uint32_t const angle{0u - (static_cast<uint32_t>(placement.heading) << 16)};
    int32_t const sine{trig.sine(angle)};
    int32_t const cosine{trig.cosine(angle)};
    for(unsigned int i{0}; i != count; ++i) {
      auto const vertices{shape.u16()};
      std::vector<vertex2> polygon;
      for(unsigned int j{0}; j != vertices; ++j) {
        int32_t const x{shape.i16() * 16};
        int32_t const y{shape.i16() * 16};
        if(model.kind == 0) {
          polygon.push_back({math::add(placement.x, math::rounded_q31(math::product_bits(x, cosine) - math::product_bits(y, sine))), math::add(placement.y, math::rounded_q31(math::product_bits(y, cosine) + math::product_bits(x, sine)))});
        } else {
          polygon.push_back({math::add(placement.x, x), math::add(placement.y, y)});
        }
      }
      raster.polygon(view.clip(polygon), colour * 0x01010101u);
    }
  }};
  auto const scan{[&](bool const super){
    int min_x{INT32_MAX};
    int min_y{INT32_MAX};
    int max_x{INT32_MIN};
    int max_y{INT32_MIN};
    for(auto const &[x, y] : std::array<std::array<int, 2>, 4>{{{area.left, area.top}, {area.right, area.top}, {area.right, area.bottom}, {area.left, area.bottom}}}) {
      auto const point{view.unproject(x, y)};
      int const sx{math::add(point.x, super ? 0x18000000 : 0) >> (super ? 26 : 22)};
      int const sy{math::add(point.y, super ? 0x18000000 : 0) >> (super ? 26 : 22)};
      min_x = std::min(min_x, sx);
      min_y = std::min(min_y, sy);
      max_x = std::max(max_x, sx);
      max_y = std::max(max_y, sy);
    }
    return std::array<int, 4>{std::max(0, min_x), std::max(0, min_y), std::min(super ? 15 : 63, max_x), std::min(super ? 15 : 63, max_y)};
  }};
  auto const supers{scan(true)};
  for(int x{supers[0]}; x <= supers[2]; ++x) {
    for(int y{supers[1]}; y <= supers[3]; ++y) {
      auto const base{origin(x * 0x4000000 - 0x18000000, y * 0x4000000 - 0x18000000)};
      if(visible(base, true)) feature(super_features, x, y, base);
    }
  }
  std::array<std::array<unsigned int, 3>, 4> constexpr orders{{{2, 0, 1}, {1, 3, 2}, {0, 1, 3}, {3, 2, 0}}};
  std::array<uint8_t, 7> constexpr lhs{193, 195, 195, 193, 197, 197, 200};
  std::array<uint8_t, 7> constexpr rhs{194, 196, 196, 194, 198, 198, 200};
  auto const triangles{[&](int const sx, int const sy, vertex2 const &base, bool const contours){
    for(int x{0}; x != 4; ++x) {
      for(int y{0}; y != 4; ++y) {
        auto const &post{terrain.post(static_cast<unsigned int>(sx * 4 + x), static_cast<unsigned int>(sy * 4 + y))};
        std::array<vertex2, 4> vertices;
        for(unsigned int i{0}; i != 4; ++i) {
          int const gx{x + static_cast<int>(i / 2)};
          int const gy{y + static_cast<int>(i % 2)};
          vertices[i] = grid(base, gx * 4, gy * 4, 13);
          vertices[i].intensity = terrain.post(static_cast<unsigned int>(sx * 4 + gx), static_cast<unsigned int>(sy * 4 + gy)).height;
        }
        for(unsigned int half{0}; half != 2; ++half) {
          if(post.textures[half] & 128) continue;
          auto const &order{orders[((post.flags & 64) ? 2 : 0) + half]};
          std::array const triangle{vertices[order[0]], vertices[order[1]], vertices[order[2]]};
          if(contours) {
            draw_map_contours(target, view, triangle, configuration.contour_interval);
          } else {
            unsigned int const surface{(post.flags >> (half * 3)) & 7u};
            if(surface >= lhs.size()) throw std::runtime_error{"Invalid terrain material for map"};
            raster.polygon(view.clip_raw(triangle), (half ? rhs[surface] : lhs[surface]) * 0x01010101u);
          }
        }
      }
    }
  }};
  auto const sectors{scan(false)};
  for(int x{sectors[0]}; x <= sectors[2]; ++x) {
    for(int y{sectors[1]}; y <= sectors[3]; ++y) {
      auto const base{origin(x * 0x400000, y * 0x400000)};
      if(!visible(base, false)) continue;
      if(configuration.flags & 2) triangles(x, y, base, false);
      feature(features, x, y, base);
      for(auto const index : scenery_sectors[static_cast<size_t>(x * 64 + y)]) {
        if(!extra_scenery.empty() && extra_scenery[index] && !(configuration.flags & 16)) continue;
        object(live_scenery.empty() ? scenery_placements[index] : *live_scenery[index]);
      }
      if(configuration.flags & 16) {
        for(auto const &placement : extra) {
          if(placement.sector == x * 64 + y) object(placement);
        }
      }
    }
  }
  if(configuration.flags & 4) {
    for(int x{sectors[0]}; x <= sectors[2]; ++x) {
      for(int y{sectors[1]}; y <= sectors[3]; ++y) {
        auto const base{origin(x * 0x400000, y * 0x400000)};
        if(visible(base, false)) triangles(x, y, base, true);
      }
    }
  }
}
} // namespace apache::render
