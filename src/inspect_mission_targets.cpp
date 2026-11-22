#include "inspect_mission_targets.h"
#include <istream>
#include <ostream>
#include <stdexcept>
#include "resources/features.h"
#include "simulation/mission_targets.h"

void inspect_mission_targets(std::istream &input, std::ostream &output, apache::resources::data_store const &data, std::string const &zone, std::string const &path) {
  namespace math = apache::math;
  namespace resources = apache::resources;
  namespace simulation = apache::simulation;
  auto const bytes{data.read(path)};
  auto const mission{resources::read_mission_definition(std::string{bytes.begin(), bytes.end()})};
  resources::terrain const terrain{data.read(zone + ".WZ/GRIDPOST.BIN")};
  auto models{resources::read_models(data.read("3DDATA/3DOBJS.BIN"))};
  resources::executable_data const executable{data};
  resources::initialise_model_metadata(models, executable);
  unsigned int const map{zone == "YEMEN" ? 1u : zone == "KOREA" ? 2u : zone == "CYPRUS" ? 3u : zone == "FORT" ? 4u : 0u};
  simulation::mission_scenery scene{mission, resources::read_scenery(data.read(zone + ".WZ/GNDOBJS.BIN")), terrain, models, map};
  resources::feature_layer const features{data, zone, false};
  resources::feature_layer const super_features{data, zone, true};
  math::trigonometry const trig{data};
  auto const formations{resources::read_formations(executable)};
  simulation::actor_environment environment{
    .height{[&](int32_t x, int32_t y){return terrain.elevation(x, y);}},
    .surface{[&](int32_t x, int32_t y){return resources::surface_type(features, super_features, x, y);}}
  };
  simulation::combat_random random;
  random.next();
  simulation::mission_actors actors{mission, formations, models, trig, environment, map, random.global_seed};
  random.global_seed = actors.random_seed;
  apache::render::mobile_scene mobiles{models};
  simulation::mission_spatial spatial{mission, scene, actors, mobiles, terrain};
  actors.synchronize_models();
  simulation::scenery_collisions structures{models, trig, scene.base, scene.extra};
  simulation::mission_targets targets{mission, scene, actors, spatial, random, terrain, structures, mobiles, models, trig};
  auto const id{[&](simulation::shared_target const &target)->int64_t {
    if(!target) return 0;
    auto const &t{target->identity};
    return t.kind == simulation::target_kind::mobile ? 200001 + static_cast<int64_t>(spatial.actor_index(t.index).value()) : 1 + t.index + t.layer * 100000;
  }};
  std::string command;
  while(input >> command) {
    if(command == "move") {
      size_t index{0};
      math::vector3 position;
      input >> index;
      for(auto &v : position) input >> v;
      spatial.move(actors.units.at(index), position);
    } else if(command == "query") {
      bool ring{false};
      bool randomised{false};
      int32_t side{0};
      int32_t minimum{0};
      int32_t maximum{0};
      unsigned int kind{0};
      math::vector3 origin;
      input >> ring >> randomised >> kind >> side >> minimum >> maximum;
      for(auto &v : origin) input >> v;
      auto target{ring ? targets.search(origin, side, minimum, maximum, static_cast<simulation::search_kind>(kind), randomised) : targets.in_sector(origin[0] >> 22, origin[1] >> 22, side, origin, static_cast<simulation::search_kind>(kind), randomised)};
      output << id(target) << ' ' << random.value << ' ' << random.global_seed;
      if(target) {
        auto const d{targets.describe(*target)};
        for(auto v : d.movement.position) output << ' ' << v;
        for(auto v : d.movement.direction) output << ' ' << v;
        output << ' ' << d.movement.speed << ' ' << d.movement.classification << ' ' << d.model << ' ' << d.status << ' ' << d.side << ' ' << d.damage << ' ' << d.radar << ' ' << d.designated << ' ' << d.chaff << ' ' << d.flare << ' ' << target->classification;
      }
      output << '\n';
    } else {
      throw std::runtime_error{"Unknown mission target command"};
    }
    if(!input) throw std::runtime_error{"Incomplete mission target command"};
  }
}
