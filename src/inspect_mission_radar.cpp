#include "inspect_mission_radar.h"
#include <ostream>
#include "resources/features.h"
#include "simulation/mission_radar.h"

void inspect_mission_radar(std::ostream &output, apache::resources::data_store const &data, std::string const &zone, std::string const &path) {
  namespace resources = apache::resources;
  namespace simulation = apache::simulation;
  auto const bytes{data.read(path)}; auto const mission{resources::read_mission_definition(std::string{bytes.begin(), bytes.end()})};
  resources::terrain const terrain{data.read(zone + ".WZ/GRIDPOST.BIN")}; auto models{resources::read_models(data.read("3DDATA/3DOBJS.BIN"))}; resources::executable_data const executable{data}; resources::initialise_model_metadata(models, executable);
  unsigned int const map{zone == "YEMEN" ? 1u : zone == "KOREA" ? 2u : zone == "CYPRUS" ? 3u : zone == "FORT" ? 4u : 0u};
  simulation::mission_scenery const scene{mission, resources::read_scenery(data.read(zone + ".WZ/GNDOBJS.BIN")), terrain, models, map};
  resources::feature_layer const features{data, zone, false}, super_features{data, zone, true}; apache::math::trigonometry const trig{data}; auto const formations{resources::read_formations(executable)};
  simulation::actor_environment const environment{.height{[&](int32_t x, int32_t y){ return terrain.elevation(x, y); }}, .surface{[&](int32_t x, int32_t y){ return resources::surface_type(features, super_features, x, y); }}};
  simulation::combat_random random; random.next(); simulation::mission_actors actors{mission, formations, models, trig, environment, map, random.global_seed};
  std::vector<simulation::shared_target> targets;
  for(size_t i{0}; i != actors.units.size(); ++i) targets.push_back(std::make_shared<simulation::target_reference>(simulation::target_reference{{simulation::target_kind::mobile, 0, static_cast<uint32_t>(i)}, {}, 0}));
  simulation::war_grid grid; simulation::initialise_mission_radars(grid, scene, actors, targets, terrain);
  auto const id{[](simulation::shared_target const &t)->int64_t { if(!t) return 0; return t->identity.index + 1 + (t->identity.kind == simulation::target_kind::mobile ? 200000 : t->identity.layer * 100000); }};
  auto const snapshot{[&](char const *phase){
    output << phase; for(auto n : grid.counts()) output << ' ' << n; for(auto n : grid.peak_counts()) output << ' ' << n; output << '\n';
    for(int32_t x{0}; x < 64; ++x) for(int32_t y{0}; y < 64; ++y) for(unsigned int side{1}; side <= 2; ++side) if(auto const *cell{grid.sector(x, y, side)}) {
      output << "sector " << x << ' ' << y << ' ' << side << ' ' << id(cell->radar); for(auto f : cell->floor) output << ' ' << f;
      output << ' ' << static_cast<unsigned int>(cell->air.count); for(auto const &t : cell->air.targets) output << ' ' << id(t);
      output << ' ' << static_cast<unsigned int>(cell->ground.count); for(auto const &t : cell->ground.targets) output << ' ' << id(t); output << '\n';
    }
  }};
  snapshot("radar");
  auto const insert{[&](size_t i){ auto const &a{actors.units[i]}; auto const &task{mission.tasks[a.location.task]}; grid.insert(a.object.world.position[0] >> 22, a.object.world.position[1] >> 22, {targets[i], a.object.status, static_cast<int32_t>(a.classification), task.type, false}); }};
  for(auto const &group : actors.groups) { if(!(actors.units[group.units.front()].conditions & 0x400)) insert(group.units.front()); for(size_t member{group.units.size()}; member-- > 1;) insert(group.units[member]); }
  snapshot("contacts");
  for(auto const &actor : actors.units) output << "actor " << actor.object.id << ' ' << actor.object.flags << ' ' << actor.object.status << '\n';
  output << "random " << random.value << ' ' << actors.random_seed << '\n';
}
