#pragma once
#include "celestial.h"
#include "clouds.h"
#include "crater_renderer.h"
#include "draw_queue.h"
#include "effect_renderer.h"
#include "feature_renderer.h"
#include "fragment_renderer.h"
#include "horizon.h"
#include "model_renderer.h"
#include "map_renderer.h"
#include "mobile_scene.h"
#include "sector_scan.h"
#include "terrain_renderer.h"
#include "resources/features.h"

namespace apache::render {
struct landscape_options {
  unsigned int distance{7};
  unsigned int terrain_detail{3};
  bool features{true};
  bool scenery{true};
  bool celestial_bodies{true};
  bool shadows{true};
  unsigned int infrared{0};
  int32_t cockpit_glass{0};
  std::optional<uint32_t> hidden_mobile;
  sky_lighting lighting;
  int cloud_height{26000};
  int32_t cloud_bottom{6400000};
  int32_t cloud_top{6912000};
  int32_t near_object_depth{-262144};
  int32_t far_object_depth{4194304};
  model_style models;
  sky_conditions sky{.ground_colour{166}};
};
class landscape {
  resources::terrain terrain;
  celestial sky_objects;
  effect_renderer effect_drawer;
  resources::terrain_textures textures;
  resources::feature_layer features;
  resources::feature_layer super_features;
  math::trigonometry trig;
  resources::model_archive models;
  std::vector<std::optional<resources::texture>> object_textures;
  std::vector<model_instance> scenery;
  std::vector<resources::scenery_placement> scenery_placements;
  std::array<std::vector<size_t>, 4096> scenery_sectors;
  std::vector<resources::scenery_placement const *> live_scenery;
  std::vector<bool> extra_scenery;
  mobile_scene *live_mobiles{nullptr};
  double lod_distance_multiplier{1};
  simulation::effects const *live_effects{nullptr};
  simulation::craters const *live_craters{nullptr};
  simulation::fragments const *live_fragments{nullptr};
  void refresh_scenery(size_t index);
public:
  scene_palette palette;
  std::function<void()> before_mobile_scan;
  mobile_scene mobiles;
  int32_t sun_glare{0};
  simulation::effects effects;
  simulation::craters craters;
  simulation::fragments fragments;
  size_t missing_normals{0};
  size_t invalid_texture_samples{0};
  landscape(resources::data_store const &data, std::string_view zone);
  // Bound scene storage must outlive the renderer and retain its placement addresses.
  void bind_scene(std::span<resources::scenery_placement const> base, std::span<resources::scenery_placement const> extra, mobile_scene &objects, simulation::effects const &particles, simulation::craters const &marks, simulation::fragments const &pieces);
  void initialise_model_metadata(resources::executable_data const &executable) { resources::initialise_model_metadata(models, executable); }
  resources::terrain const &heights() const { return terrain; }
  uint8_t surface_type(int32_t x, int32_t y) const;
  void advance(int32_t delta_time, uint32_t &random_seed);
  void draw_map(framebuffer &target, rectangle area, window2 window, uint32_t heading, map_configuration const &configuration = {}, std::span<resources::scenery_placement const> extra = {}) const;
  std::string_view draw_flir(framebuffer &target, model_instance &object, math::vector3 const &aim, math::vector3 const &observer, sky_lighting const &lighting);
  void draw(framebuffer &target, math::pose camera, landscape_options const &options);
};
} // namespace apache::render
