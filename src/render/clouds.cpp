#include "clouds.h"
#include <algorithm>
#include <bit>
#include <cstdlib>
#include <stdexcept>
#include "terrain_shading.h"

namespace apache::render {
namespace {
int32_t grid_term(int32_t const coefficient, int const index, unsigned int const shift) {
  auto const value{static_cast<int32_t>((int64_t{coefficient} * (index < 0 ? -index : index)) >> shift)};
  return index < 0 ? math::negate(value) : value;
}
struct grid_position {
  int coarse{0};
  int fine{0};
  explicit grid_position(int const steps) : coarse{(steps + (steps < 0 ? -128 : 128)) / 256}, fine{steps - coarse * 256} {}
  void advance() { fine += 4; if(fine > 128) { fine -= 256; ++coarse; } }
};
} // namespace
sector_bounds scan_cloud_sectors(math::pose const &camera, math::trigonometry const &trig, uint32_t const heading, uint32_t const pitch, unsigned int const cloud_mode, bool const expanded_frustum) {
  int const range{cloud_mode == 2 ? 18 : 14};
  sector_bounds result{-3, -3, 3, 3};
  if(std::bit_cast<int32_t>(pitch) <= 0x31c71c72) {
    if(cloud_mode == 2) result = {-range, -range, range, range};
    else {
      uint32_t const start{heading - 0x20000000u}, quadrant{start >> 30}, local{start & 0x3fffffffu};
      auto const tangent{[&](uint32_t const angle){
        int32_t const ratio{static_cast<int32_t>((int64_t{trig.sine(angle)} << 31) / trig.cosine(angle))};
        return static_cast<int>(static_cast<double>(ratio) * (1.0 / 2147483648.0) * range);
      }};
      int x{0}, y{0};
      if(local <= 0x20000000) { y = range; x = tangent(std::min(local, 0x1f49f49fu)); }
      else { x = range; y = tangent(0x40000000u - std::max(local, 0x20b60b61u)); }
      if(quadrant == 1) { int const old{x}; x = y; y = -old; }
      else if(quadrant == 2) { x = -x; y = -y; }
      else if(quadrant == 3) { int const old{x}; x = -y; y = old; }
      int end_x{x}, end_y{y};
      for(int steps{range * 2}; steps > 0;) {
        if(end_y == range) { --steps; ++end_x; }
        if(end_y == -range) { --steps; --end_x; }
        if(end_x == -range) { --steps; ++end_y; }
        if(end_x == range) { --steps; --end_y; }
      }
      result = {std::min({0, x, end_x}), std::min({0, y, end_y}), std::max({0, x, end_x}), std::max({0, y, end_y})};
    }
  }
  if(expanded_frustum) result = {-range, -range, range, range};
  auto const restrict_axis{[&](int &low, int &high, int const centre){
    if(centre + low < 0) low = 1 - centre;
    if(centre + low > 63) low = 63 - centre;
    low = std::max(low, -range);
    if(low > -1 && centre > 0) low = -1;
    if(centre + high < 0) high = centre + 1;
    if(centre + high > 62) high = 62 - centre;
    high = std::min(high, range);
    if(high < 1 && centre < 62) high = 1;
  }};
  restrict_axis(result.left, result.right, camera.position[0] >> 22);
  restrict_axis(result.bottom, result.top, camera.position[1] >> 22);
  return result;
}
unsigned int cloud_patch_type(int const x, int const y) {
  if(x < 0 || y < 0 || x >= 36 || y >= 36) return 0;
  if(x >= 8 && x < 28 && y >= 8 && y < 28) return ((x | y) & 1) ? 0 : 6;
  if((x | y) & 3) return 0;
  if(x == 4 && y >= 8 && y < 28) return 4;
  if(x == 28 && y >= 8 && y < 28) return 2;
  if(y == 4 && x >= 8 && x < 28) return 5;
  if(y == 28 && x >= 8 && x < 28) return 3;
  return 1;
}
size_t cloud_mesh::index(int const x, int const y) const {
  if(x < left || y < bottom || x - left >= static_cast<int>(columns) || y - bottom >= static_cast<int>(rows)) throw std::runtime_error{"Cloud patch leaves its coordinate grid"};
  return static_cast<size_t>(x - left) * rows + static_cast<unsigned int>(y - bottom);
}
cloud_mesh::cloud_mesh(resources::terrain const &terrain, math::pose const &camera, sector_bounds const bounds, int const height, unsigned int const cloud_mode, unsigned int const weather) : sectors{bounds}, mode{cloud_mode} {
  if(bounds.empty()) return;
  if(height < 0 || height > 65535 || cloud_mode > 2) throw std::runtime_error{"Invalid cloud height or mode"};
  int const sx{camera.position[0] >> 22}, sy{camera.position[1] >> 22};
  left = (sx + bounds.left) * 4; bottom = (sy + bounds.bottom) * 4;
  int const right{(sx + bounds.right + 1) * 4}, top{(sy + bounds.top + 1) * 4};
  if(left < 0 || bottom < 0 || right >= 256 || top >= 256) throw std::runtime_error{"Cloud coordinates leave the warzone"};
  columns = static_cast<unsigned int>(right - left + 1); rows = static_cast<unsigned int>(top - bottom + 1);
  vertices.reserve(static_cast<size_t>(columns) * rows);
  auto const &matrix{camera.rotation};
  auto const origin{math::transform(matrix, {-(camera.position[0] & 0x3fffff), -(camera.position[1] & 0x3fffff), math::negate(camera.position[2])})};
  grid_position x{bounds.left * 16};
  grid_position const first_y{bounds.bottom * 16};
  for(unsigned int column{0}; column != columns; ++column, x.advance()) {
    auto y{first_y};
    for(unsigned int row{0}; row != rows; ++row, y.advance()) {
      int const dx{std::abs(bounds.left * 4 + static_cast<int>(column))}, dy{std::abs(bounds.bottom * 4 + static_cast<int>(row))};
      int const range{std::max(dx, dy) + std::min(dx, dy) / 4};
      int const drop{mode != 2 && (dx > 8 || dy > 8) ? std::max(0, range - 8) * (weather == 1 ? 155 : 288) : 0};
      int const altitude{height - drop}, high{(altitude + 512) >> 10};
      int low{altitude & 1023};
      if(low >= 512) low -= 1024;
      low = (low >> 4) + ((low >> 3) & 1);
      cloud_vertex point;
      for(size_t axis{0}; axis != 3; ++axis) {
        size_t const offset{axis * 3};
        int32_t value{origin[axis]};
        value = math::add(value, grid_term(matrix[offset], x.coarse, 5));
        value = math::add(value, grid_term(matrix[offset], x.fine, 13));
        value = math::add(value, grid_term(matrix[offset + 1], y.coarse, 5));
        value = math::add(value, grid_term(matrix[offset + 1], y.fine, 13));
        value = math::add(value, grid_term(matrix[offset + 2], high, 13));
        value = math::add(value, grid_term(matrix[offset + 2], low, 19));
        point.eye[axis] = value;
      }
      point.intensity = terrain.post(static_cast<unsigned int>(left) + column, static_cast<unsigned int>(bottom) + row).light * (1 << 23);
      vertices.push_back(point);
    }
  }
}
void cloud_mesh::submit(draw_queue &queue, math::pose const &camera) {
  patches.clear();
  if(vertices.empty()) return;
  int const sx{camera.position[0] >> 22}, sy{camera.position[1] >> 22};
  int const range{mode == 2 ? 18 : 14}, step{0x400000}, altitude{camera.position[2] / 0xfa000};
  auto const point{[&](int const x, int const y) -> cloud_vertex& { return vertices[index(x, y)]; }};
  auto const range_to{[](cloud_vertex const &p){
    int32_t const distance{static_cast<int32_t>(approximate_range(p.eye))};
    return p.eye[1] < 0 ? -distance : distance;
  }};
  auto const add{[&](int const x, int const y, unsigned int const type, int32_t const depth){
    if(queue.push({draw_kind::cloud, static_cast<uint32_t>(patches.size()), depth})) patches.push_back({x, y, type});
  }};
  for(int x{sectors.left}; x <= sectors.right; ++x) for(int y{sectors.bottom}; y <= sectors.top; ++y) {
    if(std::abs(x) <= 4 && std::abs(y) <= 4) continue;
    int const gx{(sx + x) * 4}, gy{(sy + y) * 4};
    auto &first{point(gx, gy)};
    int32_t const distance{range_to(first)};
    if(queue.full() || distance > range * step) {
      // Allocation precedes range rejection in the original cloud pass.
      queue.consume_unused_slot();
      for(int const dx : {0, 4}) for(int const dy : {0, 4}) point(gx + dx, gy + dy).intensity = 0x73000000;
      continue;
    }
    int const start{(range - 2 - (mode == 0 ? altitude : 0)) * step};
    if(distance > start) {
      int intensity{(first.intensity >> 23) + ((distance - start) / (step / 12)) * 18};
      intensity = std::min(230, intensity);
      if(distance > (range - 1) * step) {
        point(gx + 4, gy).intensity = point(gx, gy + 4).intensity = point(gx + 4, gy + 4).intensity = 0x73000000;
        intensity = 230;
      }
      first.intensity = intensity * (1 << 23);
    }
    add(gx, gy, 1, distance);
  }
  for(int x{(sx + std::max(sectors.left, -4)) * 4}; x < (sx + std::min(sectors.right, 4) + 1) * 4; ++x) {
    for(int y{(sy + std::max(sectors.bottom, -4)) * 4}; y < (sy + std::min(sectors.top, 4) + 1) * 4; ++y) {
      unsigned int const type{cloud_patch_type(x - sx * 4 + 16, y - sy * 4 + 16)};
      if(!type || queue.full()) continue;
      auto &first{point(x, y)};
      int32_t const distance{range_to(first)};
      if(mode == 0 && distance > (range - 2 - altitude) * step) {
        int intensity{(first.intensity >> 23) + 54};
        if(distance > (range - 1 - altitude) * step) intensity = 230;
        first.intensity = std::min(230, intensity) * (1 << 23);
      }
      int const size{type <= 5 ? 4 : type <= 10 ? 2 : 1};
      int32_t const depth{std::max({first.eye[1], point(x, y + size).eye[1], point(x + size, y).eye[1], point(x + size, y + size).eye[1]})};
      add(x, y, type, depth);
    }
  }
}
void cloud_mesh::draw(rasterizer &raster, projection const &view, scene_palette const &palette, uint32_t const payload) const {
  auto const &item{patches.at(payload)};
  unsigned int const type{item.type <= 5 ? item.type : item.type <= 10 ? item.type - 5 : 1};
  int const size{item.type <= 5 ? 4 : item.type <= 10 ? 2 : 1}, half{size / 2};
  // Corners A, B, C, D, followed by the four optional edge midpoints.
  std::array<std::array<int, 2>, 8> const offsets{{{0, 0}, {0, size}, {size, 0}, {size, size}, {0, half}, {half, 0}, {size, half}, {half, size}}};
  std::array<std::array<std::array<unsigned int, 3>, 3>, 5> constexpr faces{{{{{2, 1, 0}, {2, 3, 1}, {0, 0, 0}}}, {{{0, 2, 4}, {4, 2, 3}, {4, 3, 1}}}, {{{5, 2, 3}, {5, 3, 1}, {5, 1, 0}}}, {{{6, 0, 2}, {6, 1, 0}, {6, 3, 1}}}, {{{0, 2, 7}, {0, 7, 1}, {2, 3, 7}}}}};
  for(unsigned int i{0}; i != (type == 1 ? 2u : 3u); ++i) {
    auto order{faces[type - 1][i]};
    if(mode != 0) std::swap(order[1], order[2]);
    std::array<vertex3, 3> triangle;
    for(size_t j{0}; j != 3; ++j) {
      auto const &offset{offsets[order[j]]};
      auto const &point{vertices[index(item.x + offset[0], item.y + offset[1])]};
      triangle[j] = view.prepare(point.eye, 0, 0, point.intensity);
    }
    auto const clipped{view.clip(triangle)};
    if(clipped.empty() || winding(clipped, true) < 0) continue;
    std::vector<screen_vertex> polygon;
    for(auto const &point : clipped) polygon.push_back(point.screen);
    raster.fill(polygon, {.colours{0xa6a7a6a7}, .remapping{}, .light{shading::gouraud}, .intensity_map{palette.intensity}});
  }
}
} // namespace apache::render
