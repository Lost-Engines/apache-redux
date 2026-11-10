#include "projection.h"
#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <numbers>
#include <optional>
#include <stdexcept>

namespace apache::render {
namespace {
int64_t plane_distance(math::vector3 const &p, unsigned int const plane) {
  switch(plane) {
  case 128: return int64_t{p[1]} - 1;
  case 8: return int64_t{p[1]} - p[2];
  case 4: return int64_t{p[1]} + p[2];
  case 2: return int64_t{p[1]} - p[0];
  default: return int64_t{p[1]} + p[0];
  }
}
std::optional<vertex3> crossing(vertex3 const &a, vertex3 const &b, unsigned int const plane) {
  int64_t const numerator{plane_distance(a.eye, plane)}, denominator{numerator - plane_distance(b.eye, plane)};
  if(!denominator || (numerator < 0) != (denominator < 0) || std::abs(numerator) > std::abs(denominator)) return {};
  vertex3 result;
  if(numerator == denominator) result = b;
  else {
    auto const fraction{static_cast<int32_t>((std::abs(numerator) << 31) / std::abs(denominator))};
    auto const lerp{[&](int32_t const x, int32_t const y){ return math::add(x, math::multiply_q31(math::subtract(y, x), fraction)); }};
    for(size_t i{0}; i != 3; ++i) result.eye[i] = lerp(a.eye[i], b.eye[i]);
    result.screen.u = lerp(a.screen.u, b.screen.u);
    result.screen.v = lerp(a.screen.v, b.screen.v);
    result.screen.intensity = lerp(a.screen.intensity, b.screen.intensity);
  }
  switch(plane) {
  case 128: result.eye[1] = 1; break;
  case 8: result.eye[2] = result.eye[1]; break;
  case 4: result.eye[2] = math::negate(result.eye[1]); break;
  case 2: result.eye[0] = result.eye[1]; break;
  default: result.eye[0] = math::negate(result.eye[1]); break;
  }
  result.outcode = outcode(result.eye);
  return result;
}
int cross_sign(vertex3 const &a, vertex3 const &b, vertex3 const &c) {
  int64_t const cross{int64_t{a.screen.x - b.screen.x} * (b.screen.y - c.screen.y) - int64_t{b.screen.x - c.screen.x} * (a.screen.y - b.screen.y)};
  return static_cast<int>(cross >> 32);
}
} // namespace
uint8_t outcode(math::vector3 const &point) {
  std::array<uint8_t, 18> constexpr codes{0, 8, 4, 2, 10, 6, 1, 9, 5, 0x8f, 0x8b, 0x87, 0x8e, 0x8a, 0x86, 0x8d, 0x89, 0x85};
  unsigned int index{point[1] < 1 ? 9u : 0u};
  int64_t const distance{std::abs(int64_t{point[1]})};
  if(std::abs(int64_t{point[0]}) > distance) index += point[0] < 0 ? 6 : 3;
  if(std::abs(int64_t{point[2]}) > distance) index += point[2] < 0 ? 2 : 1;
  return codes[index];
}
int winding(std::span<vertex3 const> const vertices, bool const robust) {
  if(vertices.size() < 3) return 0;
  if(!robust) return cross_sign(vertices[0], vertices[1], vertices[2]);
  auto const different{[&](size_t const first, size_t const start){
    size_t next{start % vertices.size()};
    while(next != first && vertices[next].screen.x == vertices[first].screen.x && vertices[next].screen.y == vertices[first].screen.y) next = (next + 1) % vertices.size();
    return next;
  }};
  size_t const second{different(0, 1)};
  if(!second) return 0;
  size_t const third{different(second, second + 1)};
  int const initial{cross_sign(vertices[0], vertices[second], vertices[third])};
  size_t const fourth{different(third, third + 1)};
  if((cross_sign(vertices[second], vertices[third], vertices[fourth]) ^ initial) < 0) return cross_sign(vertices[third], vertices[fourth], vertices[different(fourth, fourth + 1)]);
  return initial;
}
projection::projection(int const x, int const y, int const w, int const h, double const horizontal_fov, std::optional<rectangle> const reference) : viewport{x, y, x + w - 1, y + h - 1}, width{w}, height{h}, reference_height{reference ? reference->bottom - reference->top + 1 : h} {
  if(w < 1 || h < 1 || w > 8192 || h > 8192) throw std::runtime_error{"Invalid projection dimensions"};
  if(!std::isfinite(horizontal_fov) || horizontal_fov <= 0 || horizontal_fov >= 180) throw std::runtime_error{"Invalid field of view"};
  if(reference_height < 1 || reference_height > 8192 || (reference && (reference->left != x || reference->right != x + w - 1))) throw std::runtime_error{"Invalid projection reference"};
  // A depth-dependent vertical shear keeps the optical axis in the windscreen
  // while the clipping planes cover the extra space below it.
  if(reference) vertical_offset = 2 * (viewport.bottom - reference->bottom) + reference_height - h;
  horizontal_factor = horizontal_fov == 90 ? 1 : 1 / std::tan(horizontal_fov * std::numbers::pi / 360);
  vertical_factor = horizontal_factor * w / reference_height;
  double const scale{static_cast<double>(w) * 2147483647.0 / (static_cast<double>(reference_height) * 16.0)};
  vertical_scale = static_cast<int32_t>(std::min(scale, double{INT32_MAX}));
  double const inverse{static_cast<double>(reference_height) * 2147483647.0 / (static_cast<double>(w) * 16.0)};
  inverse_vertical_scale = static_cast<int32_t>(std::min(inverse, double{INT32_MAX}));
  original_scale = horizontal_fov == 90 && scale <= INT32_MAX && inverse <= INT32_MAX;
}
projection::projection(framebuffer const &target) : projection{target.clip.left, target.clip.top,
  target.clip.right - target.clip.left + 1, target.clip.bottom - target.clip.top + 1,
  target.horizontal_fov, target.projection_reference} {}
void projection::project(vertex3 &point) const {
  auto const &p{point.eye};
  if(p[1] <= 0) throw std::runtime_error{"Cannot project a point behind the near plane"};
  point.screen.x = std::min(viewport.right, viewport.left + static_cast<int>(((int64_t{p[0]} + p[1]) * width >> 1) / p[1]));
  point.screen.y = std::max(viewport.top, viewport.bottom - static_cast<int>(((int64_t{p[2]} + p[1]) * height >> 1) / p[1]));
}
math::vector3 projection::scale(math::vector3 eye) const {
  if(original_scale) eye[2] = std::bit_cast<int32_t>(static_cast<uint32_t>((math::product_bits(eye[2], vertical_scale) + (1ull << 26)) >> 27));
  else {
    auto const bounded{[](double value){ return static_cast<int32_t>(std::clamp(std::round(value), double{INT32_MIN}, double{INT32_MAX})); }};
    eye[0] = bounded(eye[0] * horizontal_factor); eye[2] = bounded(eye[2] * vertical_factor);
  }
  // Retain the reference scale for fog and undo this shear before unprojecting.
  if(reference_height != height || vertical_offset)
    eye[2] = static_cast<int32_t>(std::clamp((int64_t{eye[2]} * reference_height + int64_t{eye[1]} * vertical_offset) / height, int64_t{INT32_MIN}, int64_t{INT32_MAX}));
  return eye;
}
math::vector3 projection::unframe(math::vector3 eye) const {
  if(reference_height != height || vertical_offset)
    eye[2] = static_cast<int32_t>(std::clamp((int64_t{eye[2]} * height - int64_t{eye[1]} * vertical_offset) / reference_height, int64_t{INT32_MIN}, int64_t{INT32_MAX}));
  return eye;
}
vertex3 projection::prepare(math::vector3 eye, int32_t const u, int32_t const v, int32_t const intensity) const {
  eye = scale(eye);
  vertex3 result{eye, {.u{u}, .v{v}, .intensity{intensity}}, outcode(eye)};
  if(!result.outcode) project(result);
  return result;
}
std::optional<screen_vertex> projection::project_unclipped(math::vector3 eye) const {
  if(eye[1] <= 0) return {};
  eye = scale(eye);
  auto const coordinate{[&](int32_t const side, int const extent) -> std::optional<int32_t> {
    int64_t const product{int64_t{math::add(side, eye[1])} * extent};
    if((static_cast<uint64_t>(std::abs(product)) >> 32) >= static_cast<uint32_t>(eye[1])) return {};
    int64_t const result{(product >> 1) / eye[1]};
    if(result <= INT32_MIN || result > INT32_MAX) return {};
    return static_cast<int32_t>(result);
  }};
  auto const x{coordinate(eye[0], width)}, y{coordinate(eye[2], height)};
  if(!x || !y) return {};
  return screen_vertex{math::add(viewport.left, *x), math::subtract(viewport.bottom, *y)};
}
math::vector3 projection::unmagnify(math::vector3 eye) const {
  eye = unframe(eye);
  if(horizontal_factor != 1) for(auto axis : {0, 2})
    eye[axis] = static_cast<int32_t>(std::clamp(std::round(eye[axis] / horizontal_factor), double{INT32_MIN}, double{INT32_MAX}));
  return eye;
}
math::vector3 projection::unscale(math::vector3 eye) const {
  eye = unframe(eye);
  if(original_scale) eye[2] = std::bit_cast<int32_t>(static_cast<uint32_t>((math::product_bits(eye[2], inverse_vertical_scale) + (1ull << 26)) >> 27));
  else {
    auto const bounded{[](double value){ return static_cast<int32_t>(std::clamp(std::round(value), double{INT32_MIN}, double{INT32_MAX})); }};
    eye[0] = bounded(eye[0] / horizontal_factor); eye[2] = bounded(eye[2] / vertical_factor);
  }
  return eye;
}
vertex3 projection::prepare_scaled(math::vector3 const &eye, int32_t const intensity) const {
  vertex3 result{eye, {.intensity{intensity}}, outcode(eye)};
  if(!result.outcode) project(result);
  return result;
}
std::optional<std::array<vertex3, 2>> projection::clip_line(vertex3 a, vertex3 b) const {
  if(a.outcode & b.outcode) return {};
  auto const end{[](vertex3 &point, vertex3 const &other){
    for(unsigned int const plane : {128u, 8u, 4u, 2u, 1u}) if(point.outcode & plane) {
      auto const intersection{crossing(point, other, plane)};
      if(!intersection) return false;
      point = *intersection;
      if((point.outcode & plane) || (point.outcode & other.outcode)) return false;
    }
    return point.outcode == 0;
  }};
  auto const original{a};
  if(!end(a, b) || !end(b, original)) return {};
  project(a); project(b);
  return std::array<vertex3, 2>{a, b};
}
std::vector<vertex3> projection::clip(std::span<vertex3 const> const vertices) const {
  std::vector<vertex3> result{vertices.begin(), vertices.end()};
  if(result.empty()) return result;
  unsigned int any{0}, all{255};
  for(auto const &v : result) { any |= v.outcode; all &= v.outcode; }
  if(all) return {};
  for(unsigned int const plane : {128u, 8u, 4u, 2u, 1u}) {
    if(!(any & plane)) continue;
    std::vector<vertex3> clipped;
    auto previous{result.back()};
    any = 0; all = 255;
    for(auto const &current : result) {
      if((current.outcode ^ previous.outcode) & plane) {
        auto const intersection{crossing(current, previous, plane)};
        if(!intersection) return {};
        clipped.push_back(*intersection);
      }
      if(!(current.outcode & plane)) clipped.push_back(current);
      previous = current;
    }
    for(auto const &v : clipped) { any |= v.outcode; all &= v.outcode; }
    if(clipped.empty() || (any & plane) || all) return {};
    result = std::move(clipped);
  }
  if(any) return {};
  for(auto &v : result) project(v);
  return result;
}
} // namespace apache::render
