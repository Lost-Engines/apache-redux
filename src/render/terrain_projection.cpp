#include "terrain_projection.h"
#include <stdexcept>

namespace apache::render {
namespace {
int32_t grid_term(int32_t const coefficient, int const index, unsigned int const shift) {
  int const magnitude{index < 0 ? -index : index};
  int32_t const value{static_cast<int32_t>((int64_t{coefficient} * magnitude) >> shift)};
  return index < 0 ? math::negate(value) : value;
}
struct grid_position {
  int coarse{0};
  int fine{0};
  explicit grid_position(int const steps)
    : coarse{(steps + (steps < 0 ? -128 : 128)) / 256},
      fine{steps - coarse * 256} {}
  void advance() {
    fine += 4;
    if(fine > 128) {
      fine -= 256;
      ++coarse;
    }
  }
};
} // anonymous namespace
std::vector<vertex3> project_terrain(resources::terrain const &terrain, math::pose const &camera, projection const &view, int const left, int const bottom, unsigned int const columns, unsigned int const rows) {
  if(left < 0 || bottom < 0 || left > 256 || bottom > 256 || columns > 257 - static_cast<unsigned int>(left) || rows > 257 - static_cast<unsigned int>(bottom)) throw std::runtime_error{"Terrain projection leaves the grid"};
  auto const &matrix{camera.rotation};
  auto const origin{math::transform(matrix, {-(camera.position[0] & 0x3fffff), -(camera.position[1] & 0x3fffff), math::negate(camera.position[2])})};
  grid_position x{(left - (camera.position[0] >> 22) * 4) * 4};
  grid_position const first_y{(bottom - (camera.position[1] >> 22) * 4) * 4};
  std::vector<vertex3> result;
  result.reserve(static_cast<size_t>(columns) * rows);
  for(unsigned int column{0}; column != columns; ++column, x.advance()) {
    auto y{first_y};
    for(unsigned int row{0}; row != rows; ++row, y.advance()) {
      auto const &post{terrain.post(static_cast<unsigned int>(left) + column, static_cast<unsigned int>(bottom) + row)};
      int const high{(post.height + 512) >> 10};
      int low{post.height & 1023};
      if(low >= 512) low -= 1024;
      low = (low >> 4) + ((low >> 3) & 1);
      math::vector3 eye;
      for(size_t axis{0}; axis != 3; ++axis) {
        size_t const offset{axis * 3};
        int32_t value{origin[axis]};
        value = math::add(value, grid_term(matrix[offset], x.coarse, 5));
        value = math::add(value, grid_term(matrix[offset], x.fine, 13));
        value = math::add(value, grid_term(matrix[offset + 1], y.coarse, 5));
        value = math::add(value, grid_term(matrix[offset + 1], y.fine, 13));
        value = math::add(value, grid_term(matrix[offset + 2], high, 13));
        value = math::add(value, grid_term(matrix[offset + 2], low, 19));
        eye[axis] = value;
      }
      result.push_back(view.prepare(eye, 0, 0, post.light * (1 << 23)));
    }
  }
  return result;
}
} // namespace apache::render
