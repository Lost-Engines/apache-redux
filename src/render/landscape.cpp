#include "landscape.h"
#include <algorithm>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include "terrain_projection.h"

namespace apache::render {
landscape::landscape(resources::data_store const &data, std::string_view const zone) : terrain{data.read(std::string{zone} + ".WZ/GRIDPOST.BIN")}, sky_objects{data, zone}, effect_drawer{data}, textures{data, zone}, features{data, zone, false}, super_features{data, zone, true}, trig{data}, models{resources::read_models(data.read("3DDATA/3DOBJS.BIN"))}, object_textures{resources::read_object_textures(data.read("3DDATA/OBJTEXT.BIN"))}, palette{data, zone}, mobiles{models} {
  auto placements{resources::read_scenery(data.read(std::string{zone} + ".WZ/GNDOBJS.BIN"))};
  resources::initialise_scenery(placements);
  scenery.reserve(placements.size());
  for(auto const &placement : placements) {
    if(!placement.model) continue;
    uint32_t const angle{static_cast<uint32_t>(placement.heading) << 16};
    int32_t const sine{trig.sine(angle)}, cosine{trig.cosine(angle)};
    math::pose const position{{placement.x, placement.y, 0}, {cosine, math::negate(sine), 0, sine, cosine, 0, 0, 0, INT32_MAX}};
    auto object{construct_scenery_model(models, placement.model, position)};
    object.flags = placement.animation;
    object.alternate_colours = (placement.status & 0x2000) != 0;
    scenery_sectors[placement.sector].push_back(scenery.size());
    scenery.push_back(std::move(object));
    scenery_placements.push_back(placement);
  }
}
void landscape::bind_scene(std::span<resources::scenery_placement const> const base, std::span<resources::scenery_placement const> const extra, mobile_scene &objects, simulation::effects const &particles, simulation::craters const &marks, simulation::fragments const &pieces) {
  live_mobiles = &objects; live_effects = &particles; live_craters = &marks; live_fragments = &pieces;
  scenery.clear(); scenery_placements.clear(); live_scenery.clear(); extra_scenery.clear();
  for(auto &sector : scenery_sectors) sector.clear();
  for(unsigned int layer{0}; layer != 2; ++layer) for(auto const &placement : layer ? extra : base) {
    if(!placement.model) continue;
    scenery_sectors.at(placement.sector).push_back(scenery.size());
    scenery.emplace_back(); scenery_placements.emplace_back(); live_scenery.push_back(&placement); extra_scenery.push_back(layer != 0);
    refresh_scenery(scenery.size() - 1);
  }
}
void landscape::refresh_scenery(size_t const index) {
  if(live_scenery.empty()) return;
  auto const &source{*live_scenery[index]}; auto &cached{scenery_placements[index]}; auto &object{scenery[index]};
  if(source.model && (source.model != object.id || source.heading != cached.heading || source.x != cached.x || source.y != cached.y)) {
    uint32_t const angle{static_cast<uint32_t>(source.heading) << 16};
    int32_t const sine{trig.sine(angle)}, cosine{trig.cosine(angle)};
    object = construct_scenery_model(models, source.model, {{source.x, source.y, 0}, {cosine, math::negate(sine), 0, sine, cosine, 0, 0, 0, INT32_MAX}});
  }
  cached = source; object.flags = source.animation; object.alternate_colours = (source.status & 0x2000) != 0;
}
uint8_t landscape::surface_type(int32_t const x, int32_t const y) const { return resources::surface_type(features, super_features, x, y); }
void landscape::advance(int32_t const delta_time, uint32_t &random_seed) {
  if(delta_time < 0) throw std::runtime_error{"Visual time cannot run backwards"};
  craters.advance(); effects.advance(delta_time, terrain);
  fragments.advance(delta_time, trig, terrain, [&](int32_t const x, int32_t const y){ return surface_type(x, y); }, craters, random_seed);
}
std::string_view landscape::draw_flir(framebuffer &target, model_instance &object, math::vector3 const &aim, math::vector3 const &observer, sky_lighting const &lighting) {
  auto const &definition{models.at(object.id).value()};
  math::vector3 delta;
  for(size_t i{0}; i != 3; ++i) delta[i] = math::subtract(aim[i], observer[i]);
  auto const forward{math::normalise_q31(delta)};
  math::pose camera{aim, math::direction_matrix(forward)};
  int32_t const offset{math::negate(math::add(std::max(definition.radius, definition.height), std::max(definition.radius, definition.height)))};
  for(size_t i{0}; i != 3; ++i) camera.position[i] = math::add(camera.position[i], math::multiply_q31(forward[i], offset));
  auto const elevation{terrain.elevation(camera.position[0], camera.position[1])};
  if(camera.position[2] <= elevation) camera.position[2] = elevation + 256;
  for(size_t i{0}; i != 3; ++i) delta[i] = math::subtract(object.relative.position[i], camera.position[i]);
  object.distance = static_cast<int32_t>(approximate_range(delta));
  model_renderer drawer{target, models, object_textures, palette};
  drawer.style = {.textures{false}, .gouraud{false}, .glass{false}, .light{light_vector(trig, lighting)}};
  // The isolated-target path keeps the colour remap left by the preceding
  // scene render. The central gunner display uses this same path.
  drawer.draw(object, camera);
  missing_normals += drawer.missing_normals; invalid_texture_samples += drawer.invalid_texture_samples;
  return definition.name;
}
void landscape::draw(framebuffer &target, math::pose camera, landscape_options const &configuration) {
  auto options{configuration};
  auto &active_mobiles{live_mobiles ? *live_mobiles : mobiles};
  auto const &active_effects{live_effects ? *live_effects : effects};
  auto const &active_craters{live_craters ? *live_craters : craters};
  auto const &active_fragments{live_fragments ? *live_fragments : fragments};
  if(options.infrared == 2) { options.terrain_detail = 0; options.models.textures = false; options.models.gouraud = false; options.models.glass = false; options.sky.cloud_flags &= ~2u; }
  palette.prepare(options.infrared);
  if(options.terrain_detail > 3) throw std::runtime_error{"Terrain quality must be between zero and three"};
  missing_normals = invalid_texture_samples = 0;
  projection const view{target.clip.left, target.clip.top, target.clip.right - target.clip.left + 1, target.clip.bottom - target.clip.top + 1, target.horizontal_fov};
  horizon background{target, target.clip, palette, trig};
  terrain_fog const fog{options.distance};
  auto const elevation{terrain.elevation(camera.position[0], camera.position[1])};
  if(camera.position[2] <= elevation) camera.position[2] = elevation + 256;
  auto const angles{trig.angles(camera.rotation)};
  auto sky{options.sky};
  sky.cloud_mode = camera.position[2] < options.cloud_bottom ? 0 : camera.position[2] < options.cloud_top ? 1 : 2;
  background.ground(angles[1], angles[2], sky);
  feature_renderer feature_drawer{target, view, palette};
  terrain_renderer terrain_drawer{target, view, palette, textures};
  model_renderer object_drawer{target, models, object_textures, palette};
  object_drawer.style = options.models;
  object_drawer.style.light = light_vector(trig, options.lighting);
  draw_queue queue;
  struct tile_draw {
    unsigned int x{0};
    unsigned int y{0};
    unsigned int half{0};
    terrain_method method{terrain_method::constant};
  };
  std::vector<tile_draw> tiles;
  std::vector<vertex3> grid;
  int grid_left{0}, grid_bottom{0};
  size_t rows{0};
  auto const corners{[&](unsigned int const x, unsigned int const y){
    size_t const index{(x - static_cast<unsigned int>(grid_left)) * rows + y - static_cast<unsigned int>(grid_bottom)};
    return std::array<vertex3, 4>{grid[index], grid[index + 1], grid[index + rows], grid[index + rows + 1]};
  }};
  auto const draw_tile{[&](tile_draw const &tile, bool const two_sided){ terrain_drawer.draw(terrain.post(tile.x, tile.y), corners(tile.x, tile.y), tile.half, tile.method, fog, two_sided); }};
  auto const draw_feature{[&](resources::feature_layer const &layer, int const x, int const y, math::vector3 const &origin, bool const distant){
    if(!options.features) return;
    size_t const index{static_cast<size_t>(x * (layer.super ? 16 : 64) + y)};
    auto const id{layer.placements.at(index)};
    if(!id) return;
    auto const &object{*layer.records[id]};
    auto const points{project_feature(object, camera.rotation, origin, view)};
    feature_drawer.draw(distant && object.has_distant ? object.distant : object.faces, points, fog);
  }};
  if(sky.cloud_mode == 0 || (!(sky.cloud_flags & 2) && sky.weather == 0)) {
    // The reference engine projects the super-feature vertices but never
    // submits their face lists. They remain available in the resource tools.
    auto const sectors{scan_sectors(camera, view, angles[0], options.distance, false)};
    if(!sectors.empty()) {
      int const sx{camera.position[0] >> 22}, sy{camera.position[1] >> 22};
      grid_left = (sx + sectors.left) * 4; grid_bottom = (sy + sectors.bottom) * 4;
      rows = static_cast<size_t>((sectors.top - sectors.bottom + 1) * 4 + 1);
      grid = project_terrain(terrain, camera, view, grid_left, grid_bottom, static_cast<unsigned int>((sectors.right - sectors.left + 1) * 4 + 1), static_cast<unsigned int>(rows));
      unsigned int const detail{options.distance < 4 ? std::min(options.terrain_detail, 1u) : options.terrain_detail};
      for(int x{sectors.left}; x <= sectors.right; ++x) for(int y{sectors.bottom}; y <= sectors.top; ++y) {
        auto const origin{feature_origin(camera, sx + x, sy + y, false)};
        if(!sector_visibility(camera.rotation, origin, view, false)) continue;
        for(unsigned int gx{static_cast<unsigned int>((sx + x) * 4)}; gx != static_cast<unsigned int>((sx + x + 1) * 4); ++gx) for(unsigned int gy{static_cast<unsigned int>((sy + y) * 4)}; gy != static_cast<unsigned int>((sy + y + 1) * 4); ++gy) {
          auto const points{corners(gx, gy)};
          unsigned int const range{approximate_range(view.unmagnify(points[0].eye)) >> 22};
          if(range >= fog.boundary + fog.width) continue;
          auto const &post{terrain.post(gx, gy)};
          std::array<std::array<unsigned int, 3>, 4> constexpr indices{{{2, 0, 1}, {1, 3, 2}, {0, 1, 3}, {3, 2, 0}}};
          bool const near{camera.position[2] - elevation < 0x200000 && std::abs((camera.position[0] >> 20) - static_cast<int>(gx)) < 3 && std::abs((camera.position[1] >> 20) - static_cast<int>(gy)) < 3};
          unsigned int const level{range < fog.boundary ? detail : 0};
          for(unsigned int half{0}; half != 2; ++half) {
            auto const &order{indices[((post.flags & 64) ? 2 : 0) + half]};
            unsigned int common{255};
            int32_t depth{INT32_MIN}, sum{0};
            for(auto const index : order) { common &= points[index].outcode; depth = std::max(depth, points[index].eye[1]); sum = math::add(sum, points[index].eye[1]); }
            if(common) continue;
            terrain_method method{terrain_method::constant};
            if(detail == 1 && level) method = near ? terrain_method::subdivided : terrain_method::gouraud;
            else if(detail == 2) {
              if(level == 2) method = (post.textures[half] & 127) == 127 ? (near ? terrain_method::subdivided : terrain_method::gouraud) : (near ? terrain_method::subdivided_texture : terrain_method::texture32);
              else if(level == 1) method = terrain_method::gouraud;
            } else if(detail == 3) method = near ? terrain_method::perspective : terrain_method::texture64;
            tile_draw const tile{gx, gy, half, method};
            if(post.textures[half] & 128) draw_tile(tile, true);
            else {
              auto const payload{static_cast<uint32_t>(tiles.size())};
              if(queue.push({draw_kind::terrain, payload, depth, sum, false})) tiles.push_back(tile);
            }
          }
        }
        draw_feature(features, sx + x, sy + y, origin, std::abs(x) >= 4 || std::abs(y) >= 4);
        if(options.scenery && std::abs(x) < 2 && std::abs(y) < 2) {
          for(auto const index : scenery_sectors[static_cast<size_t>((sx + x) * 64 + sy + y)]) {
            refresh_scenery(index);
            if(!scenery_placements[index].model) continue;
            auto &object{scenery[index]};
            math::vector3 delta;
            for(size_t axis{0}; axis != 3; ++axis) delta[axis] = math::subtract(object.relative.position[axis], camera.position[axis]);
            int32_t const depth{math::transform(camera.rotation, delta)[1]};
            if(depth <= options.near_object_depth || depth >= options.far_object_depth) continue;
            auto const range{static_cast<int32_t>(approximate_range(delta))};
            object.distance = depth < 0 ? -range : range;
            if(!queue.push({draw_kind::ground_object, static_cast<uint32_t>(index), object.distance, 0, models[object.id]->bounds[5] == 0})) break;
          }
        }
      }
    }
  }
  background.sky(sky);
  if(sky.cloud_mode == 0 && (sky.weather == 2 || sky.weather == 3)) sky.cloud_flags &= ~2u;
  bool const sky_visible{std::bit_cast<int32_t>(angles[1]) >= -0x38e38e38 && !(sky.cloud_mode == 1 && ((sky.cloud_flags & 2) || sky.weather == 1))};
  std::optional<cloud_mesh> clouds;
  rasterizer cloud_raster{target};
  if((sky.cloud_flags & 2) && sky.cloud_mode != 1) {
    clouds.emplace(terrain, camera, scan_cloud_sectors(camera, trig, angles[0], angles[1], sky.cloud_mode, target.horizontal_fov != 90), options.cloud_height, sky.cloud_mode, sky.weather);
    clouds->submit(queue, camera);
  }
  sun_glare = options.celestial_bodies && sky_visible && (sky.cloud_mode == 2 || sky.weather == 0) ? sky_objects.draw(target, camera.rotation, trig, palette, sky, options.lighting) : 0;
  submit_craters(queue, active_craters, camera, {.cloud_mode{sky.cloud_mode}, .near_depth{options.near_object_depth}, .far_depth{options.far_object_depth}});
  submit_effects(queue, active_effects, camera, {.cloud_mode{sky.cloud_mode}, .infra_red{options.infrared}, .cloud_bottom{options.cloud_bottom}, .cloud_top{options.cloud_top}, .near_depth{options.near_object_depth}, .far_depth{options.far_object_depth}});
  submit_fragments(queue, active_fragments, camera, {.cloud_mode{sky.cloud_mode}, .cloud_bottom{options.cloud_bottom}, .cloud_top{options.cloud_top}, .near_depth{options.near_object_depth}, .far_depth{options.far_object_depth}});
  if(before_mobile_scan) before_mobile_scan();
  shadow_projection casting{terrain, trig, options.lighting};
  casting.cloud_mode = sky.cloud_mode; casting.cloud_top = options.cloud_top;
  casting.receiver = [&](math::vector3 const &position){ return active_mobiles.shadow_receiver(position); };
  bool const shadows{options.shadows && options.lighting.night == 0 && std::bit_cast<int32_t>(options.lighting.pitch) < -0x71c71c6 && ((sky.cloud_mode == 0 && sky.weather == 0) || sky.cloud_mode == 2)};
  active_mobiles.submit(queue, camera, {.cloud_mode{sky.cloud_mode}, .cloud_flags{sky.cloud_flags}, .weather{sky.weather}, .cloud_bottom{options.cloud_bottom}, .cloud_top{options.cloud_top}, .near_depth{options.near_object_depth}, .far_depth{options.far_object_depth}, .shadows{shadows}}, casting);
  shadow_renderer shadow_drawer{target, models, casting, palette};
  for(auto const &entry : queue.ordered()) {
    if(entry.kind == draw_kind::terrain) draw_tile(tiles[entry.payload], false);
    else if(entry.kind == draw_kind::smoke) effect_drawer.draw_smoke(active_effects.smoke_puffs.at(entry.payload), camera, angles[2], trig, view, cloud_raster);
    else if(entry.kind == draw_kind::explosion) effect_drawer.draw_explosion(active_effects.explosions.at(entry.payload), camera, angles[2], trig, view, cloud_raster);
    else if(entry.kind == draw_kind::cloud) clouds->draw(cloud_raster, view, palette, entry.payload);
    else if(entry.kind == draw_kind::shadow) {
      auto &object{active_mobiles.at(entry.payload)}; object.distance = entry.depth;
      shadow_drawer.draw(object, camera);
    } else {
      if(entry.kind == draw_kind::mobile && options.hidden_mobile == entry.payload) continue;
      std::optional<model_instance> temporary;
      if(entry.kind == draw_kind::crater) temporary.emplace(crater_model(entry.payload < 128 ? active_craters.short_lived.at(entry.payload) : active_craters.long_lived.at(entry.payload - 128), trig, entry.depth));
      if(entry.kind == draw_kind::fragment) temporary.emplace(fragment_model(active_fragments.pieces.at(entry.payload), models, entry.depth));
      auto &object{temporary ? *temporary : entry.kind == draw_kind::mobile ? active_mobiles.at(entry.payload) : scenery[entry.payload]};
      object.distance = entry.depth;
      object_drawer.draw(object, camera);
      missing_normals += object_drawer.missing_normals;
      invalid_texture_samples += object_drawer.invalid_texture_samples;
    }
  }
  invalid_texture_samples += terrain_drawer.invalid_texture_samples() + cloud_raster.invalid_texture_samples;
  palette.update({.cloud_mode{sky.cloud_mode}, .cloud_flags{sky.cloud_flags}, .weather{sky.weather}, .altitude{camera.position[2]}, .cloud_bottom{options.cloud_bottom}, .cloud_top{options.cloud_top}, .night{options.lighting.night}, .infrared{options.infrared}, .sun_glare{sun_glare}, .cockpit_glass{options.cockpit_glass}});
  for(size_t i{0}; i != target.image.colours.size(); ++i) target.image.colours[i] = {palette.modified[i * 3], palette.modified[i * 3 + 1], palette.modified[i * 3 + 2]};
}
} // namespace apache::render
