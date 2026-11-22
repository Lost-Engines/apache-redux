#include "effect_renderer.h"
#include <stdexcept>
#include <string>
#include "terrain_shading.h"

namespace apache::render {
namespace {
math::vector3 eye_position(math::pose const &camera, math::vector3 const &position) {
  math::vector3 relative;
  for(size_t i{0}; i != 3; ++i) relative[i] = math::subtract(position[i], camera.position[i]);
  return math::transform(camera.rotation, relative);
}
void billboard(math::vector3 const &centre, int32_t const sine, int32_t const cosine, resources::texture const &image, bool const ground, projection const &view, rasterizer &raster) {
  if(centre[1] < 1) return;
  int32_t const x{centre[0]};
  int32_t const y{centre[1]};
  int32_t const z{centre[2]};
  int32_t const left{math::subtract(x, cosine)};
  int32_t const right_edge{math::add(x, cosine)};
  int32_t const low{math::subtract(z, sine)};
  int32_t const high{math::add(z, sine)};
  std::array<math::vector3, 4> const corners{{
    {ground ? left : math::add(left, sine), y, ground ? low : math::subtract(low, cosine)},
    {math::subtract(left, sine), y, math::add(low, cosine)},
    {math::subtract(right_edge, sine), y, math::add(high, cosine)},
    {ground ? right_edge : math::add(right_edge, sine), y, ground ? high : math::subtract(high, cosine)}
  }};
  int32_t const right{static_cast<int32_t>(image.width * 65536u - 1)};
  int32_t const bottom{static_cast<int32_t>((image.pixels.size() / image.width) * 65536u - 1)};
  std::array<std::array<int32_t, 2>, 4> const uv{{{0, bottom}, {0, 0}, {right, 0}, {right, bottom}}};
  std::array<vertex3, 4> quad;
  for(size_t i{0}; i != 4; ++i) quad[i] = view.prepare(corners[i], uv[i][0], uv[i][1]);
  auto const clipped{view.clip(quad)};
  if(clipped.empty() || winding(clipped, true) < 0) return;
  std::vector<screen_vertex> polygon;
  for(auto const &point : clipped) polygon.push_back(point.screen);
  raster.fill(polygon, {
    .remapping{},
    .image{&image},
    .masked{true},
    .intensity_map{}
  });
}
} // namespace
void submit_effects(draw_queue &queue, simulation::effects const &effects, math::pose const &camera, effect_visibility const &visibility) {
  if(visibility.infra_red == 2 || visibility.cloud_mode == 1) return;
  auto const submit{[&](math::vector3 const &position, draw_kind const kind, uint32_t const index){
    if(visibility.cloud_mode == 0 ? position[2] >= visibility.cloud_bottom : position[2] <= visibility.cloud_top) return;
    math::vector3 relative;
    for(size_t i{0}; i != 3; ++i) relative[i] = math::subtract(position[i], camera.position[i]);
    int32_t const depth{math::transform(camera.rotation, relative)[1]};
    if(depth <= visibility.near_depth || depth >= visibility.far_depth) return;
    int32_t const range{static_cast<int32_t>(approximate_range(relative))};
    queue.push({kind, index, depth < 0 ? -range : range});
  }};
  for(uint32_t i{0}; i != effects.explosions.size(); ++i) {
    if(effects.explosions[i].kind) submit(effects.explosions[i].position, draw_kind::explosion, i);
  }
  for(uint32_t i{0}; i != effects.smoke_puffs.size(); ++i) {
    if(effects.smoke_puffs[i].active) submit(effects.smoke_puffs[i].position, draw_kind::smoke, i);
  }
}
effect_renderer::effect_renderer(resources::data_store const &data) {
  auto const load{[&](auto &frames, std::string const &prefix, unsigned int const width, unsigned int const height){
    for(size_t i{0}; i != frames.size(); ++i) {
      auto pixels{data.read("3DDATA/" + prefix + std::to_string(i + 1) + ".BMP")};
      if(pixels.size() != static_cast<size_t>(width) * height) throw std::runtime_error{"Invalid effect texture dimensions"};
      frames[i] = {width, std::move(pixels)};
    }
  }};
  load(ground_explosions, "GNDEXP", 64, 32);
  load(air_explosions, "AIREXP", 64, 64);
  load(smoke_images, "SMOKE", 32, 32);
}
void effect_renderer::draw_smoke(simulation::smoke const &puff, math::pose const &camera, uint32_t const roll, math::trigonometry const &trig, projection const &view, rasterizer &raster) const {
  if(!puff.active) return;
  if(puff.lifetime < 1 || puff.radius < 0 || puff.radius > 0x1000000) throw std::runtime_error{"Invalid smoke dimensions or lifetime"};
  if(puff.age < 0 || puff.age >= puff.lifetime) return;
  auto const fraction{static_cast<int32_t>((int64_t{puff.age} << 31) / puff.lifetime)};
  int32_t const radius{math::add(puff.radius, math::multiply_q31(puff.radius * 4, fraction))};
  uint32_t const angle{puff.angle + roll};
  billboard(eye_position(camera, puff.position), math::multiply_q31(trig.sine(angle), radius), math::multiply_q31(trig.cosine(angle), radius), smoke_images.at(static_cast<size_t>(fraction >> 28)), false, view, raster);
}
void effect_renderer::draw_explosion(simulation::explosion const &blast, math::pose const &camera, uint32_t const roll, math::trigonometry const &trig, projection const &view, rasterizer &raster) const {
  if(!blast.kind || blast.frame < 0) return;
  if(blast.kind > 2 || blast.radius < 0 || blast.radius > 0x1000000) throw std::runtime_error{"Invalid explosion kind or radius"};
  auto const &image{blast.kind == 1 ? ground_explosions.at(static_cast<size_t>(blast.frame)) : air_explosions.at(static_cast<size_t>(blast.frame))};
  billboard(eye_position(camera, blast.position), math::multiply_q31(trig.sine(roll), blast.radius), math::multiply_q31(trig.cosine(roll), blast.radius), image, blast.kind == 1, view, raster);
}
} // namespace apache::render
