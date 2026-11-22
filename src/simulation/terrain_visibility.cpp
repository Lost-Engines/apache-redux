#include "terrain_visibility.h"
#include <stdexcept>

namespace apache::simulation {
visibility_result terrain_visibility(math::vector3 const &start, math::vector3 const &end, unsigned int const precision, bool const weighted, int32_t const maximum_height, std::function<int32_t(int32_t, int32_t)> const &height) {
  if(precision > 16) throw std::out_of_range{"Terrain visibility precision exceeds supported sampling budget"};
  if(start[2] > maximum_height && end[2] > maximum_height) return {};
  math::vector3 step;
  for(size_t i{0}; i != 3; ++i) step[i] = math::subtract(end[i], start[i]) >> precision;
  auto point{start};
  auto previous{start};
  auto const advance{[&](math::vector3 const &increment){
    for(size_t i{0}; i != 3; ++i) point[i] = math::add(point[i], increment[i]);
  }};
  if(!weighted) {
    // The uniform routine uses precision as its sample count, too.
    for(unsigned int i{0}; i != precision; ++i) {
      if(point[2] < height(point[0], point[1])) return {false, previous};
      previous = point;
      advance(step);
    }
  } else {
    auto fine{step};
    for(auto &v : fine) v >>= precision;
    auto const count{1u << precision};
    for(unsigned int i{0}; i != count; ++i) {
      if(point[2] < height(point[0], point[1])) return {false, previous};
      previous = point;
      advance(fine);
    }
    // The original keeps the last fine sample as its returned obstruction edge.
    for(unsigned int i{1}; i < count; ++i) {
      if(point[2] < height(point[0], point[1])) return {false, previous};
      advance(step);
    }
  }
  return {};
}
} // namespace apache::simulation
