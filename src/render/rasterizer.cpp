#include "rasterizer.h"
#include <algorithm>
#include <cstdlib>
#include <stdexcept>
#include "math/fixed.h"

namespace apache::render {
namespace {
scan_edge edge(screen_vertex const &v) { return {v.x, v.u, v.v, v.intensity}; }
void require_inside(screen_vertex const &v, framebuffer const &target) {
  if(v.x < 0 || v.y < 0 || v.x >= static_cast<int>(target.image.width) || v.y >= static_cast<int>(target.image.height)) throw std::runtime_error{"Raster input must be clipped to the screen"};
}
} // namespace
rasterizer::rasterizer(framebuffer &destination) : target{destination}, left(destination.image.height), right(destination.image.height), perspective_left(destination.image.height), perspective_right(destination.image.height) {}
void rasterizer::line(screen_vertex a, screen_vertex const b, uint8_t const colour) {
  require_inside(a, target);
  require_inside(b, target);
  int const dx{std::abs(b.x - a.x)}, dy{std::abs(b.y - a.y)};
  int const sx{b.x < a.x ? -1 : 1}, sy{b.y < a.y ? -1 : 1};
  bool const horizontal{dx > dy};
  int const major{horizontal ? dx : dy}, minor{horizontal ? dy : dx};
  int error{(major + 1) / 2};
  for(int i{0}; i <= major; ++i) {
    target.point(a.x, a.y, colour);
    if(horizontal) a.x += sx;
    else a.y += sy;
    error -= minor;
    if(error <= 0) {
      error += major;
      if(horizontal) a.y += sy;
      else a.x += sx;
    }
  }
}
void rasterizer::scan(std::span<screen_vertex const> const polygon) {
  first_row = static_cast<int>(target.image.height);
  last_row = -1;
  for(auto const &v : polygon) require_inside(v, target);
  if(polygon.empty()) return;
  first_row = last_row = polygon[0].y;
  left[first_row] = right[first_row] = edge(polygon[0]);
  if(polygon.size() == 1) return;
  for(size_t i{0}; i != polygon.size(); ++i) {
    auto const &a{polygon[i]};
    auto const &b{polygon[(i + 1) % polygon.size()]};
    if(b.y < first_row) { first_row = b.y; left[b.y] = right[b.y] = edge(b); }
    if(b.y > last_row) { last_row = b.y; left[b.y] = right[b.y] = edge(b); }
    if(a.y == b.y) {
      if(b.x > a.x && b.x > right[b.y].x) right[b.y] = edge(b);
      if(b.x < a.x && b.x < left[b.y].x) left[b.y] = edge(b);
      continue;
    }
    int const dx{std::abs(b.x - a.x)}, dy{std::abs(b.y - a.y)};
    int const sx{b.x < a.x ? -1 : 1}, sy{b.y < a.y ? -1 : 1};
    auto &side{sy > 0 ? right : left};
    auto value{edge(a)};
    int y{a.y};
    int const major{std::max(dx, dy)};
    scan_edge const increment{sx, (b.u - a.u) / major, (b.v - a.v) / major, (b.intensity - a.intensity) / major};
    auto const advance{[&]{ value.u = math::add(value.u, increment.u); value.v = math::add(value.v, increment.v); value.intensity = math::add(value.intensity, increment.intensity); }};
    if(dx <= dy) {
      int error{dy / 2};
      for(int step{0}; step <= dy; ++step) {
        side[y] = value;
        y += sy;
        advance();
        error -= dx;
        if(dx && error <= 0) { error += dy; value.x += sx; }
      }
    } else {
      // The shallow edges keep the outside pixel of each scan line. The
      // original uses different entry/exit handling for the two slopes.
      int error{dx / 2};
      bool const outward{sx == sy};
      if(!outward) { side[y] = value; y += sy; }
      for(int row{0}; row < dy; ++row) {
        if(row || !outward) { value.x += sx; advance(); }
        error -= dy;
        while(error > 0) { value.x += sx; advance(); error -= dy; }
        error += dx;
        side[y] = value;
        y += sy;
      }
      if(outward) side[y] = edge(b);
    }
  }
}
void rasterizer::polygon(std::span<screen_vertex const> const vertices, uint32_t const colours, fill_pattern const pattern, std::span<uint8_t const> const remapping) {
  fill(vertices, {.colours{colours}, .pattern{pattern}, .remapping{remapping}, .intensity_map{}});
}
void rasterizer::fill(std::span<screen_vertex const> const vertices, material const &paint) {
  scan(vertices);
  paint_scans(paint);
}
void rasterizer::circle_outline(int const cx, int const cy, int const radius, uint8_t const colour) {
  if(radius <= 0) return;
  if(radius > 1048576) throw std::runtime_error{"Circle radius exceeds the rasterizer limit"};
  auto const &area{target.clip};
  if(int64_t{cx} - radius > area.right || int64_t{cx} + radius < area.left || int64_t{cy} - radius > area.bottom || int64_t{cy} + radius < area.top) return;
  int y{radius}, error{3 - radius * 2};
  for(int x{0}; x <= y; ++x) {
    target.point(cx + x, cy + y, colour); target.point(cx - x, cy + y, colour);
    target.point(cx - x, cy - y, colour); target.point(cx + x, cy - y, colour);
    target.point(cx + y, cy + x, colour); target.point(cx - y, cy + x, colour);
    target.point(cx - y, cy - x, colour); target.point(cx + y, cy - x, colour);
    if(error < 0) error += x * 4 + 6;
    else { error += (x - y) * 4 + 10; --y; }
  }
}
void rasterizer::circle(int const centre_x, int const centre_y, int const radius, material const &paint) {
  if(paint.image || paint.light != shading::none) throw std::runtime_error{"Circles do not carry texture or lighting coordinates"};
  if(radius <= 0) return;
  if(radius > 1048576) throw std::runtime_error{"Circle radius exceeds the rasterizer limit"};
  int64_t const cx{centre_x}, cy{centre_y};
  auto const area{intersect(target.clip, {0, 0, static_cast<int>(target.image.width) - 1, static_cast<int>(target.image.height) - 1})};
  if(area.left > area.right || area.top > area.bottom || cx - radius > area.right || cx + radius < area.left || cy - radius > area.bottom || cy + radius < area.top) return;
  first_row = static_cast<int>(target.image.height);
  last_row = -1;
  auto const span{[&](int64_t const y, int64_t const extent){
    if(y < area.top || y > area.bottom) return;
    int64_t const lhs{std::max<int64_t>(cx - extent, area.left)}, rhs{std::min<int64_t>(cx + extent, area.right)};
    if(lhs > rhs) return;
    int const row{static_cast<int>(y)};
    left[row] = {static_cast<int>(lhs), 0, 0, 0};
    right[row] = {static_cast<int>(rhs), 0, 0, 0};
    first_row = std::min(first_row, row);
    last_row = std::max(last_row, row);
  }};
  int y{radius}, error{3 - radius * 2};
  for(int x{0}; x <= y; ++x) {
    span(cy + x, y); span(cy - x, y);
    if(error < 0) error += x * 4 + 6;
    else {
      span(cy + y, x); span(cy - y, x);
      error += (x - y) * 4 + 10;
      --y;
    }
  }
  paint_scans(paint);
}
void rasterizer::paint_scans(material const &paint) {
  if(paint.pattern == fill_pattern::remap && paint.remapping.size() != 256) throw std::runtime_error{"A translucent fill needs a 256-entry colour map"};
  if(paint.light != shading::none && paint.intensity_map.size() != 65536) throw std::runtime_error{"Shading needs the palette intensity table"};
  if(paint.image && (!paint.image->width || paint.image->pixels.empty() || paint.image->pixels.size() % paint.image->width)) throw std::runtime_error{"Texture dimensions disagree with its pixels"};
  bool const flat{!paint.image && paint.light != shading::gouraud};
  std::array<uint8_t, 2> colours{static_cast<uint8_t>(paint.colours), static_cast<uint8_t>(paint.colours >> 8)};
  if(flat && paint.light == shading::constant) for(auto &colour : colours)
    colour = paint.intensity_map[(size_t{colour} << 8) | (static_cast<uint32_t>(std::max(paint.intensity, 0)) >> 23)];
  uint8_t heat{std::array<uint8_t, 4>{204, 102, 51, 153}[paint.heat_phase & 3]};
  for(int y{std::max(first_row, target.clip.top)}; y <= std::min(last_row, target.clip.bottom); ++y) {
    int const lhs{std::min(left[y].x, right[y].x)}, rhs{std::max(left[y].x, right[y].x)};
    int const start{std::max(lhs, target.clip.left)}, end{std::min(rhs, target.clip.right)};
    if(paint.pattern == fill_pattern::heat_haze) {
      bool const shift_right{bool(heat & 1)};
      heat = static_cast<uint8_t>((heat >> 1) | (heat << 7));
      if(start <= end) {
        size_t const row{static_cast<size_t>(y) * target.image.width};
        uint8_t previous{target.image.pixels[row + start]};
        for(int x{start}; x <= end; ++x) {
          auto &pixel{target.image.pixels[row + x]}; target.mark(row + x);
          if(shift_right) { auto const old{pixel}; pixel = previous; previous = old; }
          else pixel = target.image.pixels[std::min(row + static_cast<size_t>(x) + 1, target.image.pixels.size() - 1)];
        }
      }
      if(shift_right) ++y;
      continue;
    }
    if(start > end) continue;
    size_t const row{static_cast<size_t>(y) * target.image.width};
    auto *out{target.image.pixels.data() + row};
    auto *coverage{target.coverage.empty() ? nullptr : target.coverage.data() + row};
    if(flat) {
      if(paint.pattern == fill_pattern::solid || paint.pattern == fill_pattern::remap) {
        if(paint.pattern == fill_pattern::remap) {
          for(int x{start}; x <= end; ++x) out[x] = paint.remapping[out[x]];
        } else if(colours[0] == colours[1]) {
          std::fill(out + start, out + end + 1, colours[0]);
        } else {
          for(int x{start}; x <= end; ++x) out[x] = colours[(x + y) & 1];
        }
        if(coverage) std::fill(coverage + start, coverage + end + 1, 1);
      } else {
        // mesh patterns only visit the pixels they actually write
        if(paint.pattern == fill_pattern::medium && (y & 1)) continue;
        int const step{paint.pattern == fill_pattern::coarse ? 4 : 2};
        int const offset{paint.pattern == fill_pattern::coarse ? (y & 1) * 2 : paint.pattern == fill_pattern::fine ? y & 1 : 0};
        int const first{start + ((offset - start) & (step - 1))};
        for(int x{first}; x <= end; x += step) out[x] = colours[(x + y) & 1];
        if(coverage) for(int x{first}; x <= end; x += step) coverage[x] = 1;
      }
      continue;
    }
    int const width{rhs - lhs};
    auto value{left[y]};
    scan_edge const increment{0, width ? (right[y].u - value.u) / width : 0, width ? (right[y].v - value.v) / width : 0, width ? (right[y].intensity - value.intensity) / width : 0};
    value.u += (start - lhs) * increment.u;
    value.v += (start - lhs) * increment.v;
    value.intensity += (start - lhs) * increment.intensity;
    if(paint.image && paint.pattern == fill_pattern::solid) {
      // choose the lighting routine once per span, outside the texture loop
      auto const textured{[&]<shading light>(){
        for(int x{start}; x <= end; ++x) {
          auto const tx{static_cast<uint32_t>(value.u) >> 16}, ty{static_cast<uint32_t>(value.v) >> 16};
          size_t const address{size_t{ty} * paint.image->width + tx};
          int32_t const intensity{light == shading::constant ? paint.intensity : value.intensity};
          value.u = math::add(value.u, increment.u);
          value.v = math::add(value.v, increment.v);
          if constexpr(light == shading::gouraud) value.intensity = math::add(value.intensity, increment.intensity);
          if(address >= paint.image->pixels.size()) { ++invalid_texture_samples; continue; }
          uint8_t colour{paint.image->pixels[address]};
          if(paint.masked && !colour) continue;
          if constexpr(light != shading::none)
            colour = paint.intensity_map[(size_t{colour} << 8) | (static_cast<uint32_t>(std::max(intensity, 0)) >> 23)];
          out[x] = colour;
          if(coverage) coverage[x] = 1;
        }
      }};
      switch(paint.light) {
        case shading::none: textured.template operator()<shading::none>(); break;
        case shading::constant: textured.template operator()<shading::constant>(); break;
        case shading::gouraud: textured.template operator()<shading::gouraud>(); break;
      }
      continue;
    }
    if(!paint.image && paint.pattern == fill_pattern::solid) {
      for(int x{start}; x <= end; ++x) {
        auto const light{static_cast<uint32_t>(std::max(value.intensity, 0)) >> 23};
        out[x] = paint.intensity_map[(size_t{colours[(x + y) & 1]} << 8) | light];
        value.intensity = math::add(value.intensity, increment.intensity);
      }
      if(coverage) std::fill(coverage + start, coverage + end + 1, 1);
      continue;
    }
    for(int x{start}; x <= end; ++x) {
      int32_t const u{value.u}, v{value.v}, intensity{paint.light == shading::constant ? paint.intensity : value.intensity};
      value.u = math::add(value.u, increment.u);
      value.v = math::add(value.v, increment.v);
      value.intensity = math::add(value.intensity, increment.intensity);
      if(paint.pattern == fill_pattern::fine && ((x ^ y) & 1)) continue;
      if(paint.pattern == fill_pattern::medium && ((x | y) & 1)) continue;
      if(paint.pattern == fill_pattern::coarse && ((x - ((y & 1) * 2)) & 3)) continue;
      uint8_t colour{static_cast<uint8_t>(paint.colours >> (8 * ((x + y) & 1)))};
      if(paint.image) {
        auto const tx{static_cast<uint32_t>(u) >> 16}, ty{static_cast<uint32_t>(v) >> 16};
        // Authored UVs include the right edge (u == width). The original
        // addresses the contiguous image, so that endpoint samples the next
        // row. Bound the final address rather than rejecting the coordinate.
        size_t const address{static_cast<size_t>(ty) * paint.image->width + tx};
        if(address >= paint.image->pixels.size()) { ++invalid_texture_samples; continue; }
        colour = paint.image->pixels[address];
        if(paint.masked && !colour) continue;
      }
      if(paint.light != shading::none) colour = paint.intensity_map[(static_cast<size_t>(colour) << 8) | (static_cast<uint32_t>(std::max(intensity, 0)) >> 23)];
      auto &pixel{target.image.pixels[static_cast<size_t>(y) * target.image.width + x]};
      pixel = paint.pattern == fill_pattern::remap ? paint.remapping[pixel] : colour;
      target.mark(static_cast<size_t>(y) * target.image.width + x);
    }
  }
}
} // namespace apache::render
