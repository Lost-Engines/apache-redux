#include "inspect_targets.h"
#include <iomanip>
#include <istream>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>
#include "simulation/targets.h"

void inspect_targets(std::istream &input, std::ostream &output, apache::resources::data_store const &data) {
  namespace math = apache::math;
  namespace resources = apache::resources;
  namespace simulation = apache::simulation;
  auto models{resources::read_models(data.read("3DDATA/3DOBJS.BIN"))}; resources::initialise_model_metadata(models, resources::executable_data{data});
  math::trigonometry const trig{data}; apache::render::mobile_scene mobiles{models};
  auto const object_id{mobiles.add(7, {})};
  std::array<resources::scenery_placement, 1> placements{};
  simulation::scenery_collisions const scenery{models, trig, placements};
  std::optional<resources::terrain> terrain;
  simulation::mobile_target_state state;
  std::unique_ptr<simulation::target_world> world;
  auto const point{[&]{ math::vector3 p; for(auto &v : p) input >> v; return p; }};
  auto const print{[&](auto const &v){ for(auto x : v) output << x << ' '; }};
  std::string command;
  while(input >> command) {
    if(command == "reset") {
      std::string zone; input >> zone; terrain.emplace(data.read(zone + ".WZ/GRIDPOST.BIN"));
      world = std::make_unique<simulation::target_world>(*terrain, scenery, mobiles, models, trig, [&](uint32_t){ return state; });
    } else if(command == "describe") {
      simulation::target_reference target;
      unsigned int kind{0}, damage{0}; uint32_t model{0}, flags{0}; int32_t now{0}; std::string name;
      input >> kind >> target.classification >> model >> flags >> now;
      target.identity = {static_cast<simulation::target_kind>(kind), 0, object_id}; target.position = point();
      math::pose pose; pose.position = point(); uint32_t heading{0}, pitch{0}, roll{0}; input >> heading >> pitch >> roll; pose.rotation = trig.transformation(heading, pitch, roll);
      input >> state.type >> state.classification >> state.speed >> state.player_airspeed >> state.destroyed >> state.conditions >> damage >> state.player >> state.ground_radar >> state.air_radar >> state.chaff_at >> state.flare_at >> state.chaff_valid >> state.flare_valid >> std::quoted(name);
      state.damage = static_cast<uint8_t>(damage); state.user_name = name;
      placements[0].model = static_cast<uint16_t>(model); placements[0].status = static_cast<uint16_t>(flags);
      auto &object{mobiles.at(object_id)}; object.id = model; object.status = flags; mobiles.move(object_id, pose);
      auto const result{world->describe(target, now)};
      output << result.valid << ' '; print(result.movement.position); print(result.movement.direction);
      output << result.movement.speed << ' ' << result.movement.classification << ' ' << result.model << ' ' << result.status << ' ' << result.side << ' ' << result.damage << ' ' << result.radar << ' ' << result.designated << ' ' << result.chaff << ' ' << result.flare << ' ' << target.classification << ' ' << std::quoted(result.name) << ' ' << std::quoted(result.description) << '\n';
    } else if(command == "sectors") {
      auto const a{point()}, b{point()}; int32_t range{0}; input >> range;
      output << simulation::within_target_sectors(a, b, range) << '\n';
    } else if(command == "equivalent") {
      simulation::target_reference a, b;
      for(auto *ref : {&a, &b}) { unsigned int kind{0}; input >> kind >> ref->identity.layer >> ref->identity.index; ref->identity.kind = static_cast<simulation::target_kind>(kind); ref->position = point(); }
      output << simulation::equivalent_targets(a, b) << '\n';
    } else throw std::runtime_error{"Unknown target inspection operation"};
    if(!input) throw std::runtime_error{"Incomplete target inspection operation"};
  }
}
