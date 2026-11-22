#pragma once
#include <stdexcept>
#include "math/trigonometry.h"
#include "craters.h"
#include "render/mobile_scene.h"

namespace apache::simulation {
int32_t perpendicular_distance(math::vector3 const &start, math::vector3 const &end, math::vector3 const &point, math::trigonometry const &trig);
bool rotated_box_contains(render::model_instance const &object, math::vector3 const &point, resources::model_archive const &models);
render::model_instance const *bounding_box_hit(render::model_instance const &object, math::vector3 const &point, resources::model_archive const &models);
struct scenery_hit {
  size_t layer{0};
  size_t index{0};
  math::vector3 position{};
};
class scenery_collisions {
  resources::model_archive const &models;
  math::trigonometry const &trig;
  std::array<std::span<resources::scenery_placement const>, 2> layers;
  std::array<std::array<std::vector<size_t>, 4096>, 2> sectors;
public:
  int32_t tallest_object{0};
  bool contains(size_t layer, size_t index) const {
    return layer < layers.size() && index < layers[layer].size();
  }
  resources::scenery_placement const &at(size_t layer, size_t index) const {
    if(!contains(layer, index)) throw std::out_of_range{"Unavailable scenery target"};
    return layers[layer][index];
  }
  scenery_collisions(resources::model_archive const &objects, math::trigonometry const &angles, std::span<resources::scenery_placement const> scenery, std::span<resources::scenery_placement const> extra = {});
  std::vector<scenery_hit> in_range(math::vector3 const &position, int32_t radius) const;
  std::optional<scenery_hit> point(math::vector3 const &position, std::optional<std::array<size_t, 2>> ignored = {}) const;
  std::optional<scenery_hit> line(math::vector3 const &start, math::vector3 const &end, std::optional<std::array<size_t, 2>> ignored = {}) const;
};
std::vector<uint32_t> mobiles_in_range(render::mobile_scene const &scene, math::vector3 const &position, int32_t radius);
std::optional<uint32_t> mobile_point_collision(render::mobile_scene const &scene, resources::model_archive const &models, math::vector3 const &position, std::optional<uint32_t> ignored = {}, std::optional<uint32_t> ignored_second = {});
std::optional<uint32_t> mobile_line_collision(render::mobile_scene const &scene, resources::model_archive const &models, math::trigonometry const &trig, math::vector3 const &start, math::vector3 const &end, std::optional<uint32_t> ignored = {}, std::optional<uint32_t> ignored_second = {});
bool crater_point_collision(craters const &marks, resources::model_archive const &models, resources::terrain const &terrain, math::vector3 const &position, int32_t tallest);
bool crater_line_collision(craters const &marks, resources::model_archive const &models, resources::terrain const &terrain, math::trigonometry const &trig, math::vector3 const &start, math::vector3 const &end, int32_t tallest);
enum class flight_collision_kind {
  none,
  mobile,
  scenery,
  crater
};
struct flight_collision {
  flight_collision_kind kind{flight_collision_kind::none};
  uint32_t model{0};
  std::optional<uint32_t> mobile;
  render::model_instance const *part{nullptr};
  std::optional<scenery_hit> ground;
  bool swept{false};
};
// Flight positions are at the pilot's eye; collision tests use a point 0x900 below it.
flight_collision check_flight_collision(math::vector3 const &previous, math::vector3 const &position, std::optional<uint32_t> player, render::mobile_scene const &mobiles, scenery_collisions const &scenery, craters const &marks, resources::terrain const &terrain, resources::model_archive const &models, math::trigonometry const &trig);
bool carrier_landing_contact(bool registered, int32_t deck, int32_t previous_height, int32_t height);
} // namespace apache::simulation
