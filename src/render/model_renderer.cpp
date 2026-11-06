#include "model_renderer.h"
#include <algorithm>
#include <stdexcept>

namespace apache::render {
namespace {
resources::model const &model_at(resources::model_archive const &models, uint32_t const id) {
  if(id >= models.size() || !models[id]) throw std::runtime_error{"Unavailable model instance"};
  return *models[id];
}
model_instance construct(resources::model_archive const &models, uint32_t const id, math::pose const &position, unsigned int const depth, std::span<uint32_t const> &variable_children) {
  if(depth > 64) throw std::runtime_error{"Cyclic or excessive model attachment tree"};
  auto const &definition{model_at(models, id)};
  model_instance object;
  object.id = id;
  object.relative = object.world = position;
  for(auto const &attachment : definition.children) {
    uint32_t child{attachment.model};
    if(!child && !variable_children.empty()) { child = variable_children.front(); variable_children = variable_children.subspan(1); }
    if(!child) object.children.emplace_back();
    else object.children.push_back(std::make_unique<model_instance>(construct(models, child, {attachment.translation, attachment.rotation}, depth + 1, variable_children)));
  }
  return object;
}
} // namespace
model_instance construct_model(resources::model_archive const &models, uint32_t const id, math::pose const &position, std::span<uint32_t const> variable_children) {
  return construct(models, id, position, 0, variable_children);
}
model_instance construct_scenery_model(resources::model_archive const &models, uint32_t const id, math::pose const &position) {
  auto const &definition{model_at(models, id)};
  model_instance object;
  object.id = id; object.relative = object.world = position;
  // Ground placements use a temporary original instance without attached parts.
  // Keep empty slots for attachment instructions in the shared model program.
  object.children.resize(definition.children.size());
  return object;
}
void synchronize_model(model_instance &destination, model_instance const &source) {
  if(&destination == &source) return;
  destination.id = source.id; destination.flags = source.flags; destination.status = source.status; destination.alternate_colours = source.alternate_colours;
  destination.relative = source.relative; destination.world = source.world; destination.scale = source.scale; destination.distance = source.distance;
  destination.children.resize(source.children.size());
  for(size_t i{0}; i != source.children.size(); ++i) {
    if(!source.children[i]) destination.children[i].reset();
    else {
      if(!destination.children[i]) destination.children[i] = std::make_unique<model_instance>();
      synchronize_model(*destination.children[i], *source.children[i]);
    }
  }
}
void maintain_model(model_instance &object, math::pose const *const parent) {
  if(parent) {
    auto relative{object.relative};
    for(size_t i{0}; i != 3; ++i) relative.position[i] = math::scale_q24(relative.position[i], object.scale[i]);
    object.world = math::attach(*parent, relative);
  } else object.world = object.relative;
  for(auto const &child : object.children) if(child) maintain_model(*child, &object.world);
  if(object.maintained) object.maintained(object);
}
model_renderer::model_renderer(framebuffer &destination, resources::model_archive const &objects, std::vector<std::optional<resources::texture>> const &images, scene_palette const &colours) : target{destination}, raster{destination}, view{destination.clip.left, destination.clip.top, destination.clip.right - destination.clip.left + 1, destination.clip.bottom - destination.clip.top + 1, destination.horizontal_fov}, models{objects}, textures{images}, palette{colours} {}
void model_renderer::draw(model_instance &object, math::pose const &camera) {
  missing_normals = 0;
  raster.invalid_texture_samples = 0;
  maintain_model(object);
  draw_instance(object, camera, 0, false);
  invalid_texture_samples = raster.invalid_texture_samples;
}
void model_renderer::draw_instance(model_instance &object, math::pose const &camera, unsigned int const depth, bool alternate) {
  if(depth > 64) throw std::runtime_error{"Excessive model drawing depth"};
  alternate ^= object.alternate_colours;
  auto const *definition{&model_at(models, object.id)};
  size_t lods{0};
  while(definition->lod_distance != -1 && double(definition->lod_distance) * style.lod_distance_multiplier < object.distance) {
    if(++lods > models.size()) throw std::runtime_error{"Cyclic model detail chain"};
    definition = &model_at(models, static_cast<uint32_t>(definition->lod_next));
  }
  auto const &model{*definition};
  if(!model.offsets[4]) return;
  math::vector3 delta;
  for(size_t i{0}; i != 3; ++i) delta[i] = math::subtract(object.world.position[i], camera.position[i]);
  auto const origin{math::transform(camera.rotation, delta)};
  auto const transform{math::multiply(camera.rotation, math::transpose(object.world.rotation))};
  std::array<std::array<int32_t, 65>, 9> coarse, fine;
  if(model.kind) for(size_t i{0}; i != 9; ++i) {
    coarse[i] = math::subdivide_grid(camera.rotation[i], 19);
    fine[i] = math::subdivide_grid(camera.rotation[i], 25);
  }
  auto const convert{[&](std::array<uint8_t, 6> const &packed){
    math::vector3 result;
    if(model.kind) {
      for(auto const index : packed) if(index > 64) throw std::runtime_error{"Grid coordinate leaves its lookup table"};
      for(size_t axis{0}; axis != 3; ++axis) {
        result[axis] = origin[axis];
        for(size_t local{0}; local != 3; ++local) result[axis] = math::add(result[axis], math::add(coarse[axis * 3 + local][packed[local]], fine[axis * 3 + local][packed[local + 3]]));
      }
    } else {
      resources::reader input{packed};
      for(size_t i{0}; i != 3; ++i) result[i] = math::scale_q24(input.i16() * 16, object.scale[i]);
      result = math::transform(transform, result);
      for(size_t i{0}; i != 3; ++i) result[i] = math::add(result[i], origin[i]);
    }
    return view.prepare(result);
  }};
  unsigned int outside{255};
  for(auto const &p : model.bounding_vertices) outside &= convert(p).outcode;
  if(!model.bounding_vertices.empty() && outside) return;
  std::vector<vertex3> vertices;
  vertices.reserve(model.packed_vertices.size());
  for(auto const &p : model.packed_vertices) vertices.push_back(convert(p));
  auto const light{model.kind ? style.light : math::transform(object.world.rotation, style.light)};
  std::vector<int32_t> intensities;
  intensities.reserve(model.normals.size());
  for(auto const &normal : model.normals) {
    uint64_t sum{0};
    for(size_t i{0}; i != 3; ++i) sum += math::product_bits(normal[i] * (1 << 24), light[i]);
    intensities.push_back(std::max(0, math::rounded_q31(sum)));
  }
  resources::drawing_callbacks callbacks{
    .face{[&](resources::primitive const &face, uint32_t const pc, bool const secondary){
      int const result{draw_face(face, vertices, intensities, secondary, alternate)};
      if(face_observer) face_observer(object.id, pc);
      return result;
    }},
    .winding{[&](std::span<uint8_t const> const indices){
      std::vector<vertex3> plane;
      bool clipped{false};
      for(auto const index : indices) { plane.push_back(vertices[index]); clipped |= vertices[index].outcode != 0; }
      auto const output{view.clip(plane)};
      return output.empty() ? -1 : render::winding(output, clipped);
    }},
    .child{[&](unsigned int const slot){
      if(slot >= object.children.size()) throw std::runtime_error{"Model detail references an absent attachment slot"};
      if(object.children[slot]) {
        object.children[slot]->distance = object.distance;
        draw_instance(*object.children[slot], camera, depth + 1, alternate);
      }
    }},
  };
  resources::execute_model(model, object.flags, 0, callbacks);
}
int model_renderer::draw_face(resources::primitive const &face, std::span<vertex3 const> const vertices, std::span<int32_t const> const intensities, bool const secondary, bool const alternate) {
  auto const remap{[&](unsigned int colour){
    if(alternate && colour >= 17 && colour < 41) colour = colour < 29 ? colour + 12 : colour - 12;
    return palette.visual_remap[colour];
  }};
  auto const type{face.type};
  if(face.indices.empty()) return -1;
  auto const normal{[&](size_t const index){
    if(index >= intensities.size()) {
      // Some shipped surfaces name normals absent from their model. Keep their
      // authored colour without reading uninitialized or adjacent memory.
      ++missing_normals;
      return INT32_MAX;
    }
    return intensities[index];
  }};
  bool const textured{type == 7 || type == 8 || type == 15 || type == 16 || type == 19 || type == 20};
  bool const shaded{type >= 9 && type <= 20};
  bool const gouraud{type >= 17 && type <= 20};
  bool const two_colours{type == 3 || type == 12 || type == 18 || textured};
  material paint;
  paint.intensity_map = palette.intensity;
  paint.heat_phase = style.heat_phase;
  if(type == 6) { paint.pattern = fill_pattern::remap; paint.remapping = palette.glass; }
  else if(type == 21) paint.pattern = fill_pattern::heat_haze;
  else {
    unsigned int const a{remap(face.attributes[0])}, b{two_colours ? remap(face.attributes[1]) : a};
    paint.colours = (a | (b << 8)) * 0x10001;
    if(shaded) {
      paint.light = gouraud && style.gouraud ? shading::gouraud : shading::constant;

    }
    if(type == 4 || type == 13) paint.pattern = fill_pattern::fine;
    if(type == 5 || type == 14) paint.pattern = fill_pattern::coarse;
    if(textured && style.textures) {
      unsigned int index{face.attributes.back()};
      if(alternate) {
        for(auto const &[a, b] : std::array<std::array<unsigned int, 2>, 3>{{{14, 56}, {92, 206}, {98, 200}}}) {
          if(index == a) { index = b; break; }
          if(index == b) { index = a; break; }
        }
      }
      if(index >= textures.size() || !textures[index]) throw std::runtime_error{"Surface references an unavailable texture"};
      paint.image = &*textures[index];
      paint.masked = type == 8 || type == 16 || type == 20;
    }
  }
  if(type == 0 || type == 1 || type == 9 || type == 10) {
    uint8_t colour{static_cast<uint8_t>(paint.colours)};
    if(shaded) paint.intensity = normal(face.attributes[1]);
    if(shaded) colour = palette.intensity[(size_t{colour} << 8) | (static_cast<uint32_t>(paint.intensity) >> 23)];
    if(type == 0 || type == 9) {
      for(auto const index : face.indices) if(!vertices[index].outcode) target.point(vertices[index].screen.x, vertices[index].screen.y, colour);
    } else for(size_t i{1}; i != face.indices.size(); ++i) {
      auto const line{view.clip_line(vertices[face.indices[i - 1]], vertices[face.indices[i]])};
      if(line) raster.line((*line)[0].screen, (*line)[1].screen, colour);
    }
    return 0;
  }
  std::vector<vertex3> polygon;
  bool clipped{false};
  for(size_t i{0}; i != face.indices.size(); ++i) {
    auto vertex{vertices[face.indices[i]]};
    if(textured) { vertex.screen.u = face.uv[i][0] * 65536; vertex.screen.v = face.uv[i][1] * 65536; }
    if(gouraud && style.gouraud) vertex.screen.intensity = normal(face.normals[i]);
    clipped |= vertex.outcode != 0;
    polygon.push_back(vertex);
  }
  auto const output{view.clip(polygon)};
  if(output.empty()) return -1;
  int const order{render::winding(output, clipped)};
  if(!secondary && order < 0) return order;
  if(shaded && paint.light == shading::constant) paint.intensity = normal(face.attributes[two_colours ? 2 : 1]);
  if((type == 6 || type == 21) && !style.glass) return order;
  std::vector<screen_vertex> screen;
  screen.reserve(output.size());
  for(auto const &v : output) screen.push_back(v.screen);
  raster.fill(screen, paint);
  return order;
}
} // namespace apache::render
