#include "sector_scan.h"
#include <algorithm>
#include <stdexcept>
#include "feature_renderer.h"

namespace apache::render {
namespace {
int32_t grid_term(int32_t const coefficient, int const index, unsigned int const shift) {
  auto const value{static_cast<int32_t>((int64_t{coefficient} * (index < 0 ? -index : index)) >> shift)};
  return index < 0 ? math::negate(value) : value;
}
math::vector3 grid_offset(math::matrix3 const &rotation, math::vector3 result, int const x, int const y, unsigned int const shift, int const z = 0) {
  for(size_t axis{0}; axis != 3; ++axis) {
    result[axis] = math::add(result[axis], grid_term(rotation[axis * 3], x, shift));
    result[axis] = math::add(result[axis], grid_term(rotation[axis * 3 + 1], y, shift));
    result[axis] = math::add(result[axis], grid_term(rotation[axis * 3 + 2], z, 13));
  }
  return result;
}
bool restrict_bounds(sector_bounds &bounds, sector_bounds const &limit, int const camera_x, int const camera_y, int const size) {
  if(bounds.empty()) return false;
  bounds.left = std::clamp(bounds.left, limit.left, limit.right);
  bounds.right = std::clamp(bounds.right, limit.left, limit.right);
  bounds.bottom = std::clamp(bounds.bottom, limit.bottom, limit.top);
  bounds.top = std::clamp(bounds.top, limit.bottom, limit.top);
  if(camera_x + bounds.left >= size || camera_y + bounds.bottom >= size || camera_x + bounds.right < 0 || camera_y + bounds.top < 0) { bounds = {}; return false; }
  bounds.left = std::max(bounds.left, -camera_x); bounds.right = std::min(bounds.right, size - 1 - camera_x);
  bounds.bottom = std::max(bounds.bottom, -camera_y); bounds.top = std::min(bounds.top, size - 1 - camera_y);
  return true;
}
} // anonymous namespace
sector_bounds scan_sectors(math::pose const &camera, projection const &view, uint32_t const heading, unsigned int const distance, bool const super) {
  if(distance < 1 || distance > 14) throw std::runtime_error{"Sector distance must be between 1 and 14"};
  int const size{super ? 16 : 64}, shift{super ? 26 : 22}, mask{super ? 0x3ffffff : 0x3fffff}, offset{super ? 0x18000000 : 0};
  int const camera_x{math::add(camera.position[0], offset) >> shift}, camera_y{math::add(camera.position[1], offset) >> shift};
  int const super_x{math::add(camera.position[0], 0x18000000) >> 26}, super_y{math::add(camera.position[1], 0x18000000) >> 26};
  auto const origin{feature_origin(camera, super_x, super_y, true)};
  auto const inverse{math::transpose(camera.rotation)};
  sector_bounds bounds;
  int const radius{static_cast<int>(distance)};
  sector_bounds limit{-radius, -radius, radius, radius};
  if(super) {
    int const range{camera.position[2] < 0x7d000 ? 1 : camera.position[2] < 0xfa000 ? 2 : 3};
    int const x_offset{math::add(camera.position[0], offset) & mask}, y_offset{math::add(camera.position[1], offset) & mask};
    limit = {x_offset < 0x2000000 ? -1 - range : -range, y_offset < 0x2000000 ? -1 - range : -range, x_offset < 0x2000000 ? range : range + 1, y_offset < 0x2000000 ? range : range + 1};
  }
  auto const add_quad{[&](int const left, int const bottom, int const right, int const top, unsigned int const grid_shift, int const height){
    std::array<vertex3, 4> quad;
    std::array<std::array<int, 2>, 4> const coordinates{{{left, bottom}, {left, top}, {right, top}, {right, bottom}}};
    for(size_t i{0}; i != 4; ++i) quad[i] = view.prepare(grid_offset(camera.rotation, origin, coordinates[i][0], coordinates[i][1], grid_shift, height));
    auto const clipped{view.clip(quad)};
    if(clipped.empty()) return false;
    for(auto const &point : clipped) {
      auto const world{math::transform(inverse, view.unscale(point.eye))};
      int const x{math::add(world[0], camera.position[0] & mask) >> shift}, y{math::add(world[1], camera.position[1] & mask) >> shift};
      bounds.left = std::min(bounds.left, x); bounds.right = std::max(bounds.right, x);
      bounds.bottom = std::min(bounds.bottom, y); bounds.top = std::max(bounds.top, y);
    }
    return restrict_bounds(bounds, limit, camera_x, camera_y, size);
  }};
  if(super) {
    add_quad(limit.left, limit.bottom, limit.right + 1, limit.top + 1, 5, 0);
    return bounds;
  }
  bool found{add_quad(-2, -2, 3, 3, 5, 0)};
  // These are the large-grid indices used by the original upper-plane
  // scan, unlike the huge-grid coordinates used for its ground plane.
  found = add_quad(-122, -122, -117, -117, 13, 16) || found;
  bool const inside{camera.position[0] >= 0 && camera.position[0] < 0x10000000 && camera.position[1] >= 0 && camera.position[1] < 0x10000000};
  if(inside && camera.position[2] <= 0x400000) {
    uint32_t const quadrant{heading + 0x20000000u};
    if(!found) {
      bounds = quadrant <= 0x40000000 || (quadrant > 0x80000000 && quadrant <= 0xc0000000) ? sector_bounds{-1, 0, 1, 0} : sector_bounds{0, -1, 0, 1};
      restrict_bounds(bounds, limit, camera_x, camera_y, size);
    } else if(quadrant <= 0x40000000) { if(bounds.bottom == 1) bounds.bottom = 0; } else if(quadrant <= 0x80000000) { if(bounds.left == 1) bounds.left = 0; } else if(quadrant <= 0xc0000000) { if(bounds.top == -1) bounds.top = 0; } else if(bounds.right == -1) bounds.right = 0;
  }
  return bounds;
}
unsigned int sector_visibility(math::matrix3 const &rotation, math::vector3 const &origin, projection const &view, bool const super) {
  unsigned int all{255}, any{0};
  int const width{super ? 1 : 16};
  for(int height{0}; height != (super ? 1 : 2); ++height) for(auto const &xy : std::array<std::array<int, 2>, 4>{{{0, 0}, {0, width}, {width, width}, {width, 0}}}) {
    auto const point{view.prepare(grid_offset(rotation, origin, xy[0], xy[1], super ? 5u : 13u, height * 16))};
    all &= point.outcode; any |= point.outcode;
  }
  return all ? 0 : any ? 1 : 2;
}
} // namespace apache::render
