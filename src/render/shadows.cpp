#include "shadows.h"
#include <algorithm>
#include <stdexcept>

namespace apache::render {
namespace {
int32_t times_eight(int32_t const value) { return std::bit_cast<int32_t>(static_cast<uint32_t>(value) << 3); }
resources::model const &model_at(resources::model_archive const &models, uint32_t const id) {
  if(id >= models.size() || !models[id]) throw std::runtime_error{"Unavailable shadow model"};
  return *models[id];
}
} // namespace
shadow_projection::shadow_projection(resources::terrain const &heights, math::trigonometry const &trig, sky_lighting const &light) : terrain{heights} {
  auto const pitch{static_cast<uint32_t>(std::min(std::bit_cast<int32_t>(light.pitch), -0x71c71c7))};
  int32_t const sine{trig.sine(pitch)}, cosine{trig.cosine(pitch)};
  if(!sine) throw std::runtime_error{"Shadow light must have a vertical component"};
  cast[0] = math::negate(static_cast<int32_t>(((int64_t{trig.sine(light.heading)} * cosine) >> 3) / sine));
  cast[1] = math::negate(static_cast<int32_t>(((int64_t{trig.cosine(light.heading)} * cosine) >> 3) / sine));
}
shadow_plane shadow_projection::project(math::vector3 const &position, bool const check_receivers) const {
  shadow_plane result;
  result.height = std::max(0, math::subtract(position[2], cloud_mode == 0 ? terrain.elevation(position[0], position[1]) : cloud_top));
  auto const cast_position{[&]{
    for(size_t i{0}; i != 2; ++i) result.position[i] = math::add(position[i], math::multiply_q31(times_eight(result.height), cast[i]));
  }};
  cast_position();
  result.position[2] = std::max(0, std::min(math::subtract(position[2], 256), cloud_mode == 0 ? terrain.elevation(result.position[0], result.position[1]) : cloud_top));
  if(check_receivers && result.position[2] == 0 && receiver) {
    if(auto const height{receiver(result.position)}) {
      result.position[2] = *height;
      result.height = std::max(0, math::subtract(position[2], *height));
      cast_position();
    }
  }
  return result;
}
shadow_renderer::shadow_renderer(framebuffer &target, resources::model_archive const &objects, shadow_projection const &cast, scene_palette const &colours) : models{objects}, casting{cast}, palette{colours}, raster{target}, view{target.clip.left, target.clip.top, target.clip.right - target.clip.left + 1, target.clip.bottom - target.clip.top + 1, target.horizontal_fov} {}
void shadow_renderer::draw(model_instance &object, math::pose const &camera) {
  maintain_model(object);
  draw_instance(object, camera, 0);
}
void shadow_renderer::draw_instance(model_instance &object, math::pose const &camera, unsigned int const depth) {
  if(depth > 64) throw std::runtime_error{"Excessive shadow attachment depth"};
  auto const *definition{&model_at(models, object.id == 0x4ba || object.id == 0x4bb ? 0x40a : object.id)};
  size_t lods{0};
  while(definition->lod_distance != -1 && double(definition->lod_distance) * lod_distance_multiplier < object.distance) {
    if(++lods > models.size()) throw std::runtime_error{"Cyclic shadow detail chain"};
    definition = &model_at(models, static_cast<uint32_t>(definition->lod_next));
  }
  auto const &model{*definition};
  if(model.kind || !model.offsets[4]) return;
  auto const plane{casting.project(object.world.position, true)};
  math::vector3 const delta{math::subtract(object.world.position[0], camera.position[0]), math::subtract(object.world.position[1], camera.position[1]), math::subtract(plane.position[2], camera.position[2])};
  auto const origin{math::transform(camera.rotation, delta)};
  auto const rotation{math::transpose(object.world.rotation)};
  auto const convert{[&](std::array<uint8_t, 6> const &packed){
    resources::reader input{packed}; math::vector3 vertex;
    for(size_t i{0}; i != 3; ++i) vertex[i] = math::scale_q24(input.i16() * 16, object.scale[i]);
    vertex = math::transform(rotation, vertex);
    int32_t const height{times_eight(math::add(vertex[2], plane.height))};
    for(size_t i{0}; i != 2; ++i) vertex[i] = math::add(vertex[i], math::multiply_q31(height, casting.cast[i]));
    vertex[2] = 0;
    vertex = math::transform(camera.rotation, vertex);
    for(size_t i{0}; i != 3; ++i) vertex[i] = math::add(vertex[i], origin[i]);
    return view.prepare(vertex);
  }};
  unsigned int outside{255};
  for(auto const &point : model.bounding_vertices) outside &= convert(point).outcode;
  if(!model.bounding_vertices.empty() && outside) return;
  std::vector<vertex3> vertices;
  for(auto const &point : model.packed_vertices) vertices.push_back(convert(point));
  auto const polygon{[&](std::span<uint8_t const> const indices){
    std::vector<vertex3> result;
    for(auto const index : indices) result.push_back(vertices.at(index));
    return result;
  }};
  resources::drawing_callbacks callbacks{
    .face{[&](resources::primitive const &face, uint32_t, bool const secondary){
      if(face.type == 0 || face.type == 1 || face.type == 9 || face.type == 10) return 0;
      auto const raw{polygon(face.indices)};
      bool const clipped{std::ranges::any_of(raw, [](auto const &v){ return v.outcode != 0; })};
      auto const output{view.clip(raw)};
      if(output.empty()) return -1;
      int const order{winding(output, clipped)};
      if(face.type == 21 || (!secondary && order < 0)) return order;
      std::vector<screen_vertex> screen;
      for(auto const &point : output) screen.push_back(point.screen);
      material paint;
      paint.colours = palette.visual_remap[2] * 0x1010101u;
      paint.pattern = face.type == 6 ? fill_pattern::medium : fill_pattern::fine;
      raster.fill(screen, paint);
      return order;
    }},
    .winding{[&](std::span<uint8_t const> const indices){
      auto const raw{polygon(indices)};
      auto const output{view.clip(raw)};
      return output.empty() ? -1 : winding(output, std::ranges::any_of(raw, [](auto const &v){ return v.outcode != 0; }));
    }},
    .child{[&](unsigned int const slot){
      if(slot >= object.children.size()) throw std::runtime_error{"Shadow attachment slot is absent"};
      if(object.children[slot]) { object.children[slot]->distance = object.distance; draw_instance(*object.children[slot], camera, depth + 1); }
    }},
  };
  resources::execute_model(model, object.flags, 0, callbacks);
}
} // namespace apache::render
