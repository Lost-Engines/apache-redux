#include "target_visibility.h"
#include <cmath>

namespace apache::simulation {
namespace {
int32_t distance(math::vector3 const &start, math::vector3 const &end) {
  double sum{0};
  for(size_t i{0}; i != 3; ++i) {
    double const d{static_cast<double>(math::subtract(end[i], start[i]))};
    sum += d * d;
  }
  return std::bit_cast<int32_t>(static_cast<uint32_t>(static_cast<uint64_t>(std::sqrt(sum))));
}
}
bool line_reaches_target(math::vector3 const &start, math::vector3 const &end, target_identity const &source, target_identity const &target, resources::terrain const &terrain, scenery_collisions const &scenery, render::mobile_scene const &mobiles, resources::model_archive const &models, math::trigonometry const &trig) {
  if(source.kind == target_kind::none || target.kind == target_kind::none) return false;
  if(!terrain_visibility(start, end, 3, true, terrain.maximum_height(), [&](int32_t x, int32_t y){return terrain.elevation(x, y);}).clear) return false;
  auto const mobile{mobile_line_collision(mobiles, models, trig, start, end, source.kind == target_kind::mobile ? std::optional{source.index} : std::nullopt)};
  // The original excludes a mobile launch platform, but not a scenery launcher.
  auto const ground{scenery.line(start, end)};
  if(!mobile && !ground) return true;
  if(!mobile) {
    // A scenery hit alone does not reject a selected mobile target.
    return target.kind == target_kind::mobile || (target.kind == target_kind::scenery && ground->layer == target.layer && ground->index == target.index);
  }
  if(target.kind != target_kind::mobile || *mobile != target.index) return false;
  if(ground && distance(start, ground->position) < distance(start, mobiles.at(*mobile).world.position)) return false;
  return true;
}
} // namespace apache::simulation
