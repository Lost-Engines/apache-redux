#include "crater_renderer.h"
#include <stdexcept>
#include "terrain_shading.h"

namespace apache::render {
void submit_craters(draw_queue &queue, simulation::craters const &craters, math::pose const &camera, effect_visibility const &visibility) {
  if(visibility.cloud_mode != 0) return;
  for(uint32_t index{0}; index != 256; ++index) {
    auto const &item{index < 128 ? craters.short_lived[index] : craters.long_lived[index - 128]};
    if(item.model < 0) continue;
    math::vector3 delta;
    for(size_t i{0}; i != 3; ++i) delta[i] = math::subtract(item.position[i], camera.position[i]);
    int32_t const depth{math::transform(camera.rotation, delta)[1]};
    if(depth <= visibility.near_depth || depth >= visibility.far_depth) continue;
    auto const range{static_cast<int32_t>(approximate_range(delta))};
    if(!queue.push({draw_kind::crater, index, depth < 0 ? -range : range})) return;
  }
}
model_instance crater_model(simulation::crater const &crater, math::trigonometry const &trig, int32_t const distance) {
  if(crater.model < 0) throw std::runtime_error{"Inactive crater cannot be drawn"};
  model_instance result;
  result.id = static_cast<uint32_t>(crater.model);
  result.distance = distance;
  result.flags = crater.model == 0x180 && crater.age < 3 ? 1 : 0;
  result.scale = crater.scale;
  int32_t const sine{trig.sine(crater.heading)};
  int32_t const cosine{trig.cosine(crater.heading)};
  result.relative = {crater.position, {cosine, math::negate(sine), 0, sine, cosine, 0, 0, 0, INT32_MAX}};
  return result;
}
} // namespace apache::render
