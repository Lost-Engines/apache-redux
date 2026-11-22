#include "collisions.h"
#include <algorithm>

namespace apache::simulation {
namespace {
bool outside(math::vector3 const &p) {
  return p[0] < 0 || p[0] >= 0x10000000 || p[1] < 0 || p[1] >= 0x10000000;
}
size_t sector(math::vector3 const &p) {
  return static_cast<size_t>((p[0] >> 22) * 64 + (p[1] >> 22));
}
math::vector3 difference(math::vector3 const &a, math::vector3 const &b) {
  return {math::subtract(a[0], b[0]), math::subtract(a[1], b[1]), math::subtract(a[2], b[2])};
}
int32_t approximate2(int32_t const x, int32_t const y) {
  auto const a{x < 0 ? math::negate(x) : x};
  auto const b{y < 0 ? math::negate(y) : y};
  return math::add(std::bit_cast<int32_t>(static_cast<uint32_t>(std::max(a, b)) * 4), std::min(a, b)) >> 2;
}
int32_t approximate3(math::vector3 v) {
  for(auto &x : v) {
    if(x < 0) x = math::negate(x);
  }
  std::ranges::sort(v);
  return math::add(math::add(v[2], v[1] >> 2), v[0] >> 2);
}
std::array<int32_t, 2> search_bounds(int32_t coordinate, int32_t radius) {
  return {std::max(0, std::min(coordinate >> 22, math::subtract(coordinate, radius) >> 22)), std::min(63, std::max(coordinate >> 22, math::add(coordinate, radius) >> 22))};
}
bool interval(int32_t value, int32_t a, int32_t b, int32_t radius) {
  return math::subtract(std::min(a, b), radius) <= value && value <= math::add(std::max(a, b), radius);
}
bool box_contains(render::model_instance const &object, math::vector3 const &point, resources::model_archive const &models, bool const inclusive) {
  auto const local{math::transform(object.world.rotation, difference(point, object.world.position))};
  auto const &bounds{models.at(object.id)->bounds};
  for(size_t i{0}; i != 3; ++i) {
    if(inclusive ? local[i] < bounds[i * 2] || local[i] > bounds[i * 2 + 1] : local[i] <= bounds[i * 2] || local[i] >= bounds[i * 2 + 1]) return false;
  }
  return true;
}
} // anonymous namespace
int32_t perpendicular_distance(math::vector3 const &start, math::vector3 const &end, math::vector3 const &point, math::trigonometry const &trig) {
  auto direction{difference(end, start)};
  auto offset{difference(point, start)};
  int32_t const length{math::normalise_q31_in_place(direction)};
  int32_t const distance{math::normalise_q31_in_place(offset)};
  auto const dot{math::rounded_q31(math::product_bits(direction[0], offset[0]) + math::product_bits(direction[1], offset[1]) + math::product_bits(direction[2], offset[2]))};
  if(dot < 0 || math::multiply_q31(distance, dot) > length) return INT32_MAX;
  return math::multiply_q31(distance, trig.sine(trig.arccosine(dot)));
}
bool rotated_box_contains(render::model_instance const &object, math::vector3 const &point, resources::model_archive const &models) {
  return box_contains(object, point, models, true);
}
render::model_instance const *bounding_box_hit(render::model_instance const &object, math::vector3 const &point, resources::model_archive const &models) {
  if(box_contains(object, point, models, false)) return &object;
  for(auto const &child : object.children) {
    if(child) {
      if(auto const hit{bounding_box_hit(*child, point, models)}) return hit;
    }
  }
  return nullptr;
}
scenery_collisions::scenery_collisions(resources::model_archive const &objects, math::trigonometry const &angles, std::span<resources::scenery_placement const> const scenery, std::span<resources::scenery_placement const> const extra)
  : models{objects},
    trig{angles},
    layers{scenery, extra} {
  for(auto const &model : models) {
    if(model) tallest_object = std::max(tallest_object, model->height);
  }
  for(size_t layer{0}; layer != layers.size(); ++layer) {
    for(size_t i{0}; i != layers[layer].size(); ++i) sectors[layer].at(layers[layer][i].sector).push_back(i);
  }
}
std::vector<scenery_hit> scenery_collisions::in_range(math::vector3 const &position, int32_t const radius) const {
  std::vector<scenery_hit> result;
  if(outside(position) || radius < position[2]) return result;
  auto const xs{search_bounds(position[0], radius)};
  auto const ys{search_bounds(position[1], radius)};
  for(int32_t x{xs[0]}; x <= xs[1]; ++x) {
    for(int32_t y{ys[0]}; y <= ys[1]; ++y) {
      for(size_t layer{0}; layer != layers.size(); ++layer) {
        for(auto const index : sectors[layer][static_cast<size_t>(x * 64 + y)]) {
          auto const &object{layers[layer][index]};
          if(object.model && approximate3({math::subtract(object.x, position[0]), math::subtract(object.y, position[1]), position[2]}) < radius) result.push_back({layer, index, {object.x, object.y, 0}});
        }
      }
    }
  }
  return result;
}
std::vector<uint32_t> mobiles_in_range(render::mobile_scene const &scene, math::vector3 const &position, int32_t const radius) {
  std::vector<uint32_t> result;
  if(outside(position)) return result;
  auto const xs{search_bounds(position[0], radius)};
  auto const ys{search_bounds(position[1], radius)};
  for(int32_t x{xs[0]}; x <= xs[1]; ++x) {
    for(int32_t y{ys[0]}; y <= ys[1]; ++y) {
      for(auto const id : scene.sector(static_cast<size_t>(x * 64 + y))) {
        if(approximate3(difference(scene.at(id).world.position, position)) < radius) result.push_back(id);
      }
    }
  }
  return result;
}
std::optional<scenery_hit> scenery_collisions::point(math::vector3 const &position, std::optional<std::array<size_t, 2>> const ignored) const {
  if(outside(position) || position[2] > tallest_object) return {};
  for(size_t layer{0}; layer != layers.size(); ++layer) {
    for(auto index : sectors[layer][sector(position)]) {
      auto const &item{layers[layer][index]};
      if(!item.model || ignored == std::array{layer, index}) continue;
      auto const &model{*models.at(item.model)};
      if((model.properties & 4) && position[2] <= model.height && approximate2(math::subtract(item.x, position[0]), math::subtract(item.y, position[1])) < model.radius) return scenery_hit{layer, index, {item.x, item.y, 0}};
    }
  }
  return {};
}
std::optional<scenery_hit> scenery_collisions::line(math::vector3 const &start, math::vector3 const &end, std::optional<std::array<size_t, 2>> const ignored) const {
  if(outside(start) || outside(end) || (start[2] > tallest_object && end[2] > tallest_object)) return {};
  std::optional<scenery_hit> result;
  int32_t closest{INT32_MAX};
  for(size_t layer{0}; layer != layers.size(); ++layer) {
    for(int32_t x{std::min(start[0], end[0]) >> 22}; x <= (std::max(start[0], end[0]) >> 22); ++x) {
      for(int32_t y{std::min(start[1], end[1]) >> 22}; y <= (std::max(start[1], end[1]) >> 22); ++y) {
        for(auto index : sectors[layer][static_cast<size_t>(x * 64 + y)]) {
          auto const &item{layers[layer][index]};
          if(!item.model || ignored == std::array{layer, index}) continue;
          auto const &model{*models.at(item.model)};
          if(!(model.properties & 4) || (start[2] > model.height && end[2] > model.height) || !interval(item.x, start[0], end[0], model.radius) || !interval(item.y, start[1], end[1], model.radius)) continue;
          math::vector3 const position{item.x, item.y, 0};
          int32_t const distance{perpendicular_distance(start, end, position, trig)};
          if(distance < model.radius && distance < closest) {
            closest = distance;
            result = scenery_hit{layer, index, position};
          }
        }
      }
    }
  }
  return result;
}
std::optional<uint32_t> mobile_point_collision(render::mobile_scene const &scene, resources::model_archive const &models, math::vector3 const &position, std::optional<uint32_t> const ignored, std::optional<uint32_t> const ignored_second) {
  if(outside(position)) return {};
  for(auto id : scene.sector(sector(position))) {
    if(ignored == id || ignored_second == id) continue;
    auto const &object{scene.at(id)};
    auto const &model{*models.at(object.id)};
    if((model.properties & 4) && approximate3(difference(object.world.position, position)) < model.radius) return id;
  }
  return {};
}
std::optional<uint32_t> mobile_line_collision(render::mobile_scene const &scene, resources::model_archive const &models, math::trigonometry const &trig, math::vector3 const &start, math::vector3 const &end, std::optional<uint32_t> const ignored, std::optional<uint32_t> const ignored_second) {
  if(outside(start) || outside(end)) return {};
  std::optional<uint32_t> result;
  int32_t closest{INT32_MAX};
  for(int32_t x{std::min(start[0], end[0]) >> 22}; x <= (std::max(start[0], end[0]) >> 22); ++x) {
    for(int32_t y{std::min(start[1], end[1]) >> 22}; y <= (std::max(start[1], end[1]) >> 22); ++y) {
      for(auto id : scene.sector(static_cast<size_t>(x * 64 + y))) {
        if(ignored == id || ignored_second == id) continue;
        auto const &object{scene.at(id)};
        auto const &model{*models.at(object.id)};
        if(!(model.properties & 4)) continue;
        bool bounded{true};
        for(size_t i{0}; i != 3; ++i) bounded &= interval(object.world.position[i], start[i], end[i], model.radius);
        if(!bounded) continue;
        int32_t const distance{perpendicular_distance(start, end, object.world.position, trig)};
        if(distance < model.radius && distance < closest) {
          closest = distance;
          result = id;
        }
      }
    }
  }
  return result;
}
bool crater_point_collision(craters const &marks, resources::model_archive const &models, resources::terrain const &terrain, math::vector3 const &position, int32_t const tallest) {
  if(outside(position) || position[2] > math::add(terrain.elevation(position[0], position[1]), tallest)) return false;
  for(auto const &mark : marks.long_lived) {
    if(mark.model >= 0 && sector(mark.position) == sector(position)) {
      auto const &model{*models.at(static_cast<size_t>(mark.model))};
      if(mark.position[2] <= position[2] && position[2] <= math::add(mark.position[2], model.height) && approximate2(math::subtract(mark.position[0], position[0]), math::subtract(mark.position[1], position[1])) < model.radius) return true;
    }
  }
  return false;
}
bool crater_line_collision(craters const &marks, resources::model_archive const &models, resources::terrain const &terrain, math::trigonometry const &trig, math::vector3 const &start, math::vector3 const &end, int32_t const tallest) {
  if(outside(start) || outside(end) || (start[2] > math::add(terrain.elevation(start[0], start[1]), tallest) && end[2] > math::add(terrain.elevation(end[0], end[1]), tallest))) return false;
  for(auto const &mark : marks.long_lived) {
    if(mark.model >= 0 && (std::min(start[0], end[0]) >> 22) <= (mark.position[0] >> 22) && (mark.position[0] >> 22) <= (std::max(start[0], end[0]) >> 22)) {
      if((mark.position[1] >> 22) < (std::min(start[1], end[1]) >> 22) || (mark.position[1] >> 22) > (std::max(start[1], end[1]) >> 22)) continue;
      if(mark.position[2] < math::subtract(std::min(start[2], end[2]), tallest) || mark.position[2] > std::max(start[2], end[2])) continue;
      if(perpendicular_distance(start, end, mark.position, trig) < models.at(static_cast<size_t>(mark.model))->radius) return true;
    }
  }
  return false;
}
flight_collision check_flight_collision(math::vector3 const &previous, math::vector3 const &position, std::optional<uint32_t> const player, render::mobile_scene const &mobiles, scenery_collisions const &scenery, craters const &marks, resources::terrain const &terrain, resources::model_archive const &models, math::trigonometry const &trig) {
  auto start{previous};
  auto end{position};
  start[2] = math::subtract(start[2], 0x900);
  end[2] = math::subtract(end[2], 0x900);
  flight_collision result;
  result.swept = approximate3(difference(start, end)) >= 0x1e00;
  auto const mobile{result.swept ? mobile_line_collision(mobiles, models, trig, start, end, player) : mobile_point_collision(mobiles, models, end, player)};
  if(mobile) {
    if(auto const part{bounding_box_hit(mobiles.at(*mobile), end, models)}) {
      result.kind = flight_collision_kind::mobile;
      result.model = part->id;
      result.mobile = mobile;
      result.part = part;
      return result;
    }
  }
  if(auto const ground{result.swept ? scenery.line(start, end) : scenery.point(end)}) {
    result.kind = flight_collision_kind::scenery;
    result.ground = ground;
    result.model = scenery.at(ground->layer, ground->index).model;
    return result;
  }
  int32_t tallest{0};
  for(auto id : {11, 257, 385, 62}) tallest = std::max(tallest, models.at(id)->height);
  if(result.swept ? crater_line_collision(marks, models, terrain, trig, start, end, tallest) : crater_point_collision(marks, models, terrain, end, tallest)) {
    result.kind = flight_collision_kind::crater;
    result.model = 11;
  }
  return result;
}
bool carrier_landing_contact(bool const registered, int32_t const deck, int32_t const previous_height, int32_t const height) {
  return registered && (height >= deck || previous_height >= deck);
}
} // namespace apache::simulation
