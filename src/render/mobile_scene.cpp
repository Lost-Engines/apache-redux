#include "mobile_scene.h"
#include <algorithm>
#include <stdexcept>
#include "lod.h"
#include "terrain_shading.h"

namespace apache::render {
namespace {
std::optional<size_t> sector_at(math::vector3 const &position) {
  if(position[0] < 0 || position[0] >= 0x10000000 || position[1] < 0 || position[1] >= 0x10000000) return {};
  return static_cast<size_t>((position[0] >> 22) * 64 + (position[1] >> 22));
}
math::vector3 relative_to(math::vector3 const &position, math::vector3 const &origin) {
  return {math::subtract(position[0], origin[0]), math::subtract(position[1], origin[1]), math::subtract(position[2], origin[2])};
}
} // namespace
uint32_t mobile_scene::add(uint32_t const model, math::pose const &pose) {
  if(objects.size() >= UINT32_MAX) throw std::runtime_error{"Too many mobile scene objects"};
  auto const id{static_cast<uint32_t>(objects.size())};
  auto owned{std::make_unique<model_instance>(construct_model(models, model, pose))};
  auto *object{owned.get()};
  objects.emplace_back(entry{std::move(owned), object, {}});
  place(id);
  return id;
}
uint32_t mobile_scene::register_object(model_instance &object, bool const initially_place) {
  if(objects.size() >= UINT32_MAX) throw std::runtime_error{"Too many mobile scene objects"};
  auto const id{static_cast<uint32_t>(objects.size())};
  objects.emplace_back(entry{{}, &object, {}});
  if(initially_place) place(id);
  return id;
}
model_instance &mobile_scene::at(uint32_t const id) {
  if(id >= objects.size() || !objects[id]) throw std::runtime_error{"Unavailable mobile scene object"};
  return *objects[id]->object;
}
model_instance const &mobile_scene::at(uint32_t const id) const {
  if(id >= objects.size() || !objects[id]) throw std::runtime_error{"Unavailable mobile scene object"};
  return *objects[id]->object;
}
void mobile_scene::unplace(uint32_t const id) {
  at(id);
  auto &object{*objects.at(id)};
  auto const old{object.sector};
  if(old) std::erase(sectors[*old], id);
  object.sector.reset();
  if(old && on_remove) on_remove(id, *old);
}
void mobile_scene::place(uint32_t const id) {
  auto const sector{sector_at(at(id).world.position)};
  auto &object{*objects[id]};
  if(object.sector != sector) {
    unplace(id);
    if(sector) sectors[*sector].insert(sectors[*sector].begin(), id);
    object.sector = sector;
    if(sector && on_place) on_place(id, *sector);
  }
}
void mobile_scene::move(uint32_t const id, math::pose const &pose) {
  auto &model{at(id)};
  if(objects[id]->sector != sector_at(pose.position)) unplace(id);
  model.relative = pose;
  model.world = pose;
  place(id);
}
void mobile_scene::remove(uint32_t const id) {
  at(id); unplace(id); objects[id].reset();
}
void mobile_scene::clear() {
  for(auto &sector : sectors) sector.clear();
  objects.clear();
}
std::optional<int32_t> mobile_scene::shadow_receiver(math::vector3 const &position) const {
  auto const sector{sector_at(position)};
  if(!sector) return {};
  for(auto const id : sectors[*sector]) {
    auto const &object{*objects[id]->object};
    auto const &model{*models.at(object.id)};
    if(!(model.properties & 2)) continue;
    auto const local{math::transform(object.relative.rotation, relative_to(position, object.relative.position))};
    bool contains{true};
    for(size_t axis{0}; axis != 3; ++axis) contains &= local[axis] >= model.bounds[axis * 2] && local[axis] <= model.bounds[axis * 2 + 1];
    if(contains) return model.height;
  }
  return {};
}
void mobile_scene::submit(draw_queue &queue, math::pose const &camera, mobile_visibility const &visibility, shadow_projection const &casting) const {
  bool const clouds{(visibility.cloud_flags & 2) != 0};
  if(visibility.cloud_mode == 1 && (clouds || visibility.weather == 1)) return;
  bool const below{visibility.cloud_mode == 0 || (!clouds && visibility.weather == 0)};
  auto sort_casting{casting}; sort_casting.cloud_mode = below ? 0 : 2;
  int const sx{camera.position[0] >> 22}, sy{camera.position[1] >> 22};
  int const radius{lod_sector_radius(visibility.lod_distance_multiplier)};
  for(int x{-radius}; x <= radius; ++x) for(int y{-radius}; y <= radius; ++y) {
    if(sx + x < 0 || sx + x >= 64 || sy + y < 0 || sy + y >= 64) continue;
    for(auto const id : sectors[static_cast<size_t>((sx + x) * 64 + sy + y)]) {
      auto const &object{*objects[id]->object};
      auto const &position{object.relative.position};
      if(below ? (clouds && position[2] >= visibility.cloud_bottom) : position[2] <= visibility.cloud_top) continue;
      auto const delta{relative_to(position, camera.position)};
      int32_t const depth{math::transform(camera.rotation, delta)[1]};
      if(depth <= visibility.near_depth || depth >= visibility.far_depth) continue;
      if(queue.full()) return;
      auto const distance{static_cast<int32_t>(approximate_range(delta))};
      queued_draw const model{draw_kind::mobile, id, depth < 0 ? -distance : distance};
      std::optional<queued_draw> shadow;
      if(visibility.shadows && (models.at(object.id)->properties & 1)) {
        auto const point{sort_casting.project(position)};
        auto const delta_shadow{relative_to(point.position, camera.position)};
        int32_t const depth_shadow{math::transform(camera.rotation, delta_shadow)[1]};
        if(depth_shadow > visibility.near_depth && depth_shadow < lod_cutoff(0x80000, visibility.lod_distance_multiplier)) {
          auto const range{static_cast<int32_t>(approximate_range(delta_shadow))};
          shadow = {draw_kind::shadow, id, depth_shadow < 0 ? -range : range};
        }
      }
      if(below && shadow) {
        // The original reserves the model's slot before trying the shadow,
        // but submits the shadow first. A full buffer leaves that slot unused.
        if(queue.available() < 2) { queue.consume_unused_slot(); return; }
        queue.push(*shadow);
      }
      queue.push(model);
      if(!below && shadow && !queue.push(*shadow)) return;
    }
  }
}
} // namespace apache::render
