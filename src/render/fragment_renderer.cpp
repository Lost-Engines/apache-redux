#include "fragment_renderer.h"
#include <stdexcept>
#include "terrain_shading.h"

namespace apache::render {
namespace {
void fracture_parts(model_instance const &object, simulation::fragments &fragments, uint32_t &seed) {
  fragments.create(object.id, object.flags, object.world, seed);
  for(auto const &child : object.children) if(child) fracture_parts(*child, fragments, seed);
}
} // anonymous namespace
void fracture_model(model_instance &object, simulation::fragments &fragments, uint32_t &random_seed) {
  // Children retain the transform established by their most recent maintenance.
  // Advancing the attachments here changes the original crash/debris poses.
  fracture_parts(object, fragments, random_seed);
}
void submit_fragments(draw_queue &queue, simulation::fragments const &fragments, math::pose const &camera, effect_visibility const &visibility) {
  if(visibility.cloud_mode == 1) return;
  for(uint32_t i{0}; i != fragments.pieces.size(); ++i) {
    auto const &piece{fragments.pieces[i]}; auto const &position{piece.position.position};
    if(!piece.active || (visibility.cloud_mode == 0 ? position[2] >= visibility.cloud_bottom : position[2] <= visibility.cloud_top)) continue;
    math::vector3 delta;
    for(size_t axis{0}; axis != 3; ++axis) delta[axis] = math::subtract(position[axis], camera.position[axis]);
    int32_t const depth{math::transform(camera.rotation, delta)[1]};
    if(depth <= visibility.near_depth || depth >= visibility.far_depth) continue;
    auto const range{static_cast<int32_t>(approximate_range(delta))};
    if(!queue.push({draw_kind::fragment, i, depth < 0 ? -range : range})) return;
  }
}
model_instance fragment_model(simulation::fragment const &fragment, resources::model_archive const &models, int32_t const distance) {
  if(!fragment.active || fragment.model >= models.size() || !models[fragment.model]) throw std::runtime_error{"Unavailable fragment model"};
  model_instance result;
  result.id = fragment.model; result.flags = fragment.flags; result.relative = fragment.position; result.distance = distance;
  // Attached parts have become separate fragments. Preserve empty attachment
  // slots so their calls in the parent's drawing program remain harmless.
  result.children.resize(models[fragment.model]->children.size());
  return result;
}
} // namespace apache::render
