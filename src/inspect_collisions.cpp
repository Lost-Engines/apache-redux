#include "inspect_collisions.h"
#include <algorithm>
#include <istream>
#include <map>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>
#include "simulation/actor_movement.h"
#include "simulation/collisions.h"
#include "simulation/target_visibility.h"

void inspect_collisions(std::istream &input, std::ostream &output, apache::resources::data_store const &data) {
  namespace math = apache::math;
  namespace render = apache::render;
  namespace resources = apache::resources;
  namespace simulation = apache::simulation;
  auto const models{resources::read_models(data.read("3DDATA/3DOBJS.BIN"))};
  math::trigonometry const trig{data};
  render::mobile_scene mobiles{models};
  simulation::craters craters;
  std::vector<resources::scenery_placement> scenery;
  std::vector<resources::scenery_placement> extra;
  std::unique_ptr<simulation::scenery_collisions> collisions;
  std::optional<resources::terrain> terrain;
  int32_t tallest_crater{0};
  for(auto id : {11, 257, 385, 62}) tallest_crater = std::max(tallest_crater, models.at(id)->height);
  auto const point{[&]{
    math::vector3 p;
    for(auto &v : p) input >> v;
    return p;
  }};
  auto const pose{[&]{
    auto p{point()};
    uint32_t h{0};
    uint32_t pitch{0};
    uint32_t r{0};
    input >> h >> pitch >> r;
    return math::pose{p, trig.transformation(h, pitch, r)};
  }};
  auto const ignored{[&]()->std::optional<uint32_t> {
    int64_t id{-1};
    input >> id;
    return id < 0 ? std::nullopt : std::optional{static_cast<uint32_t>(id)};
  }};
  auto const ignored_scenery{[&]()->std::optional<std::array<size_t, 2>> {
    int64_t layer{-1};
    int64_t id{-1};
    input >> layer >> id;
    return layer < 0 || id < 0 ? std::nullopt : std::optional{std::array{static_cast<size_t>(layer), static_cast<size_t>(id)}};
  }};
  auto const print_scenery{[&](std::optional<simulation::scenery_hit> const &hit){
    if(hit) {
      output << hit->layer << ' ' << hit->index << ' ' << hit->position[0] << ' ' << hit->position[1] << ' ' << hit->position[2] << '\n';
    } else {
      output << "-1 -1 0 0 0\n";
    }
  }};
  std::string command;
  while(input >> command) {
    if(command == "los") {
      auto const start{point()};
      auto const end{point()};
      unsigned int precision{0};
      bool weighted{false};
      int32_t maximum{0};
      size_t count{0};
      input >> precision >> weighted >> maximum >> count;
      std::map<std::array<int32_t, 2>, int32_t> heights;
      for(size_t i{0}; i != count; ++i) {
        auto const p{point()};
        heights[{p[0], p[1]}] = p[2];
      }
      std::vector<std::array<int32_t, 2>> queries;
      auto const result{simulation::terrain_visibility(start, end, precision, weighted, maximum, [&](int32_t x, int32_t y){
        queries.push_back({x, y});
        auto const it{heights.find({x, y})};
        return it == heights.end() ? 0 : it->second;
      })};
      output << result.clear << ' ' << result.last_visible.has_value() << ' ';
      for(auto v : result.last_visible.value_or(math::vector3{})) output << v << ' ';
      output << queries.size();
      for(auto const &q : queries) output << ' ' << q[0] << ' ' << q[1];
      output << '\n';
    } else if(command == "visible") {
      auto const start{point()};
      auto const end{point()};
      simulation::target_identity source;
      simulation::target_identity target;
      for(auto *item : {&source, &target}) {
        unsigned int kind{0};
        input >> kind >> item->layer >> item->index;
        item->kind = static_cast<simulation::target_kind>(kind);
      }
      output << simulation::line_reaches_target(start, end, source, target, *terrain, *collisions, mobiles, models, trig) << '\n';
    } else if(command == "flight") {
      auto const start{point()};
      auto const end{point()};
      auto const player{ignored()};
      auto const hit{simulation::check_flight_collision(start, end, player, mobiles, *collisions, craters, *terrain, models, trig)};
      output << static_cast<unsigned int>(hit.kind) << ' ' << hit.model << ' ' << (hit.mobile ? static_cast<int64_t>(*hit.mobile) : -1) << ' ' << (hit.ground ? static_cast<int64_t>(hit.ground->layer) : -1) << ' ' << (hit.ground ? static_cast<int64_t>(hit.ground->index) : -1);
      for(auto v : hit.ground ? hit.ground->position : math::vector3{}) output << ' ' << v;
      output << ' ' << hit.swept << '\n';
    } else if(command == "carrier-contact") {
      bool registered{false};
      int32_t deck{0};
      int32_t previous{0};
      int32_t height{0};
      input >> registered >> deck >> previous >> height;
      output << simulation::carrier_landing_contact(registered, deck, previous, height) << '\n';
    } else if(command == "maximum-height") {
      output << terrain->maximum_height() << '\n';
    } else if(command == "reset") {
      std::string zone;
      input >> zone;
      scenery = resources::read_scenery(data.read(zone + ".WZ/GNDOBJS.BIN"));
      extra.clear();
      collisions = std::make_unique<simulation::scenery_collisions>(models, trig, scenery, extra);
      terrain.emplace(data.read(zone + ".WZ/GRIDPOST.BIN"));
      mobiles.clear();
      craters.reset();
    } else if(command == "extra") {
      uint16_t model{0};
      int32_t x{0};
      int32_t y{0};
      input >> model >> x >> y;
      extra.push_back({static_cast<uint16_t>((x >> 22) * 64 + (y >> 22)), model, x, y});
      collisions = std::make_unique<simulation::scenery_collisions>(models, trig, scenery, extra);
    } else if(command == "perp") {
      auto const start{point()};
      auto const end{point()};
      auto const target{point()};
      output << simulation::perpendicular_distance(start, end, target, trig) << '\n';
    } else if(command == "intercept") {
      auto const origin{point()};
      int32_t speed{0};
      input >> speed;
      simulation::movement_target target;
      target.position = point();
      target.direction = point();
      unsigned int iterations{0};
      input >> target.speed >> iterations;
      auto const result{simulation::intercept_point(origin, speed, target, iterations)};
      output << result[0] << ' ' << result[1] << ' ' << result[2] << '\n';
    } else if(command == "range") {
      auto const p{point()};
      int32_t radius{0};
      input >> radius;
      auto const hits{collisions->in_range(p, radius)};
      output << hits.size();
      for(auto const &hit : hits) {
        output << ' ' << hit.layer << ' ' << hit.index;
        for(auto x : hit.position) output << ' ' << x;
      }
      output << '\n';
    } else if(command == "mobile-range") {
      auto const p{point()};
      int32_t radius{0};
      input >> radius;
      auto const hits{simulation::mobiles_in_range(mobiles, p, radius)};
      output << hits.size();
      for(auto id : hits) output << ' ' << id;
      output << '\n';
    } else if(command == "point") {
      auto const p{point()};
      auto const ignore{ignored_scenery()};
      print_scenery(collisions->point(p, ignore));
    } else if(command == "line") {
      auto const start{point()};
      auto const end{point()};
      auto const ignore{ignored_scenery()};
      print_scenery(collisions->line(start, end, ignore));
    } else if(command == "add") {
      uint32_t model{0};
      input >> model;
      auto const transform{pose()};
      auto const id{mobiles.add(model, transform)};
      render::maintain_model(mobiles.at(id));
    } else if(command == "move") {
      uint32_t id{0};
      input >> id;
      mobiles.move(id, pose());
      render::maintain_model(mobiles.at(id));
    } else if(command == "remove") {
      uint32_t id{0};
      input >> id;
      mobiles.remove(id);
    } else if(command == "mobile-point") {
      auto const p{point()};
      auto const first{ignored()};
      auto const second{ignored()};
      auto const hit{simulation::mobile_point_collision(mobiles, models, p, first, second)};
      output << (hit ? static_cast<int64_t>(*hit) : -1) << '\n';
    } else if(command == "mobile-line") {
      auto const start{point()};
      auto const end{point()};
      auto const first{ignored()};
      auto const second{ignored()};
      auto const hit{simulation::mobile_line_collision(mobiles, models, trig, start, end, first, second)};
      output << (hit ? static_cast<int64_t>(*hit) : -1) << '\n';
    } else if(command == "box") {
      uint32_t id{0};
      input >> id;
      auto const p{point()};
      auto const &object{mobiles.at(id)};
      auto const hit{simulation::bounding_box_hit(object, p, models)};
      output << simulation::rotated_box_contains(object, p, models) << ' ' << (hit ? static_cast<int64_t>(hit->id) : -1) << '\n';
    } else if(command == "mark") {
      size_t index{0};
      input >> index;
      auto &mark{craters.long_lived.at(index)};
      mark.position = point();
      input >> mark.model >> mark.heading >> mark.age;
      mark.scale = point();
    } else if(command == "crater-point") {
      output << simulation::crater_point_collision(craters, models, *terrain, point(), tallest_crater) << '\n';
    } else if(command == "crater-line") {
      auto const start{point()};
      auto const end{point()};
      output << simulation::crater_line_collision(craters, models, *terrain, trig, start, end, tallest_crater) << '\n';
    } else {
      throw std::runtime_error{"Unknown collision inspection operation"};
    }
    if(!input) throw std::runtime_error{"Incomplete collision inspection operation"};
  }
}
