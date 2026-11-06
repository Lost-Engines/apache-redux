#include "rasterizer.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include "math/fixed.h"

namespace apache::render {
namespace {
perspective_edge endpoint(perspective_vertex const &point) {
  return {point.screen.x, 1.0 / point.depth, static_cast<double>(point.screen.u), static_cast<double>(point.screen.v), static_cast<double>(point.screen.intensity)};
}
int32_t nearest(double const value) {
  double const rounded{std::nearbyint(value)};
  if(rounded < INT32_MIN || rounded > INT32_MAX || !std::isfinite(rounded)) throw std::runtime_error{"Perspective interpolant exceeds the fixed-point range"};
  return static_cast<int32_t>(rounded);
}
} // namespace
void rasterizer::scan_perspective(std::span<perspective_vertex const> const vertices) {
  first_row = static_cast<int>(target.image.height); last_row = -1;
  for(auto const &point : vertices) if(point.depth < 1 || point.screen.x < 0 || point.screen.y < 0 || point.screen.x >= static_cast<int>(target.image.width) || point.screen.y >= static_cast<int>(target.image.height)) throw std::runtime_error{"Perspective input must be clipped to the screen"};
  if(vertices.empty()) return;
  first_row = last_row = vertices[0].screen.y;
  perspective_left[first_row] = perspective_right[first_row] = endpoint(vertices[0]);
  if(vertices.size() == 1) return;
  for(size_t i{0}; i != vertices.size(); ++i) {
    auto const &a{vertices[i]};
    auto const &b{vertices[(i + 1) % vertices.size()]};
    if(b.screen.y < first_row) { first_row = b.screen.y; perspective_left[first_row].x = perspective_right[first_row].x = b.screen.x; }
    if(b.screen.y > last_row) { last_row = b.screen.y; perspective_left[last_row].x = perspective_right[last_row].x = b.screen.x; }
    if(a.screen.y == b.screen.y) {
      if(b.screen.x > a.screen.x) perspective_right[b.screen.y].x = std::max(perspective_right[b.screen.y].x, b.screen.x);
      else if(b.screen.x < a.screen.x) perspective_left[b.screen.y].x = std::min(perspective_left[b.screen.y].x, b.screen.x);
      continue;
    }
    int const dx{std::abs(b.screen.x - a.screen.x)}, dy{std::abs(b.screen.y - a.screen.y)};
    int const sx{b.screen.x < a.screen.x ? -1 : 1}, sy{b.screen.y < a.screen.y ? -1 : 1};
    auto &side{sy > 0 ? perspective_right : perspective_left};
    auto value{endpoint(a)};
    double const reciprocal_end{1.0 / b.depth}, fraction{1.0 / std::max(dx, dy)};
    value.u *= value.reciprocal; value.v *= value.reciprocal; value.intensity *= value.reciprocal;
    perspective_edge const delta{sx, (reciprocal_end - value.reciprocal) * fraction, (b.screen.u * reciprocal_end - value.u) * fraction, (b.screen.v * reciprocal_end - value.v) * fraction, (b.screen.intensity * reciprocal_end - value.intensity) * fraction};
    int y{a.screen.y};
    auto const advance{[&]{ value.reciprocal += delta.reciprocal; value.u += delta.u; value.v += delta.v; value.intensity += delta.intensity; }};
    auto const store{[&]{ double const depth{1.0 / value.reciprocal}; side[y] = {value.x, value.reciprocal, value.u * depth, value.v * depth, value.intensity * depth}; }};
    if(dx <= dy) {
      int error{dy / 2};
      for(int step{0}; step <= dy; ++step) {
        store(); y += sy; advance(); error -= dx;
        if(dx && error <= 0) { error += dy; value.x += sx; }
      }
    } else {
      int error{dx / 2};
      bool const outward{sx == sy};
      if(!outward) { store(); y += sy; }
      for(int row{0}; row < dy; ++row) {
        if(row || !outward) { value.x += sx; advance(); }
        error -= dy;
        while(error > 0) { value.x += sx; advance(); error -= dy; }
        error += dx; store(); y += sy;
      }
      if(outward) side[y] = endpoint(b);
    }
  }
}
void rasterizer::perspective(std::span<perspective_vertex const> const vertices, resources::texture const &image, std::span<uint8_t const> const intensity_map) {
  if(image.width != 64 || image.pixels.size() != 4096 || intensity_map.size() != 65536) throw std::runtime_error{"Invalid perspective terrain material"};
  scan_perspective(vertices);
  for(int y{first_row}; y <= last_row; ++y) {
    auto lhs{perspective_left[y]}, rhs{perspective_right[y]};
    if(lhs.x > rhs.x) {
      // The original exchanges attributes but keeps the reciprocal-depth
      // arrays in place when it encounters a reversed span.
      std::swap(lhs.x, rhs.x); std::swap(lhs.u, rhs.u); std::swap(lhs.v, rhs.v); std::swap(lhs.intensity, rhs.intensity);
    }
    int x{lhs.x};
    auto *out{target.image.pixels.data() + size_t(y) * target.image.width};
    auto *coverage{target.coverage.empty() ? nullptr : target.coverage.data() + size_t(y) * target.image.width};
    bool const visible_row{y >= target.clip.top && y <= target.clip.bottom};
    bool const visible_span{visible_row && lhs.x >= target.clip.left && rhs.x <= target.clip.right};
    auto const pixel{[&](int32_t const u, int32_t const v, int32_t const light, bool const block){
      int64_t const address{block ? static_cast<int64_t>((static_cast<uint32_t>(v) >> 10) & 0xffc0) + (static_cast<uint32_t>(u) >> 16) : int64_t{v >> 16} * 64 + (u >> 16)};
      if(address < 0 || address >= 4096) ++invalid_texture_samples;
      else if(visible_span || (visible_row && x >= target.clip.left && x <= target.clip.right)) {
        // scan_perspective has already checked the framebuffer bounds; retain
        // the full spans so clipping cannot move the original 16-pixel blocks
        out[x] = intensity_map[size_t{image.pixels[static_cast<size_t>(address)]} * 256 + static_cast<uint8_t>(light >> 23)];
        if(coverage) coverage[x] = 1;
      }
      ++x;
    }};
    if(lhs.x == rhs.x) {
      pixel(static_cast<int32_t>(lhs.u), static_cast<int32_t>(lhs.v), static_cast<int32_t>(lhs.intensity), false);
      continue;
    }
    int remaining{rhs.x - lhs.x + 1};
    std::array<double, 3> const start{lhs.u, lhs.v, lhs.intensity}, finish{rhs.u, rhs.v, rhs.intensity};
    auto endpoint_value{start};
    if(remaining > 16) {
      int const blocks{remaining >> 4};
      double const fraction{16.0 / remaining}, depth{1.0 / lhs.reciprocal};
      double const denominator_step{(depth * rhs.reciprocal - 1.0) * fraction};
      std::array<double, 3> step;
      for(size_t i{0}; i != 3; ++i) step[i] = depth * finish[i] * rhs.reciprocal * fraction - start[i] * fraction;
      auto numerator{step};
      double denominator{denominator_step};
      std::array<int32_t, 3> value{nearest(start[0]), nearest(start[1]), nearest(start[2])};
      for(int block{0}; block != blocks; ++block) {
        std::array<int32_t, 3> delta, next;
        for(size_t i{0}; i != 3; ++i) {
          endpoint_value[i] = (start[i] + numerator[i]) / (denominator + 1.0);
          numerator[i] += step[i];
          next[i] = nearest(endpoint_value[i]);
          delta[i] = math::subtract(next[i], value[i]) >> 4;
        }
        denominator += denominator_step;
        for(int index{0}; index != 16; ++index) {
          pixel(value[0], value[1], value[2], true);
          value[0] = math::add(value[0], delta[0]);
          value[1] = math::add(value[1], delta[1]);
          value[2] = math::add(value[2], delta[2]);
        }
        value = next;
      }
      remaining %= 16;
    }
    if(remaining) {
      std::array<int32_t, 3> value, delta;
      for(size_t i{0}; i != 3; ++i) { value[i] = nearest(endpoint_value[i]); delta[i] = math::subtract(nearest(finish[i]), value[i]) / remaining; }
      for(int index{0}; index != remaining; ++index) {
        pixel(value[0], value[1], value[2], false);
        value[0] = math::add(value[0], delta[0]);
        value[1] = math::add(value[1], delta[1]);
        value[2] = math::add(value[2], delta[2]);
      }
    }
  }
}
} // namespace apache::render
