#include "inspect_mission_spatial.h"
#include <istream>
#include <ostream>
#include <stdexcept>
#include "resources/features.h"
#include "simulation/actor_reactions.h"
#include "simulation/ground_movement.h"
#include "simulation/mission_spatial.h"

void inspect_mission_spatial(std::istream &input, std::ostream &output, apache::resources::data_store const &data, std::string const &zone, std::string const &path) {
  namespace resources = apache::resources;
  namespace simulation = apache::simulation;
  auto const bytes{data.read(path)}; auto const mission{resources::read_mission_definition(std::string{bytes.begin(), bytes.end()})};
  resources::terrain const terrain{data.read(zone + ".WZ/GRIDPOST.BIN")}; auto models{resources::read_models(data.read("3DDATA/3DOBJS.BIN"))}; resources::executable_data const executable{data}; resources::initialise_model_metadata(models, executable);
  unsigned int const map{zone == "YEMEN" ? 1u : zone == "KOREA" ? 2u : zone == "CYPRUS" ? 3u : zone == "FORT" ? 4u : 0u};
  simulation::mission_scenery scene{mission, resources::read_scenery(data.read(zone + ".WZ/GNDOBJS.BIN")), terrain, models, map};
  resources::feature_layer const features{data, zone, false}, super_features{data, zone, true}; apache::math::trigonometry const trig{data}; auto const formations{resources::read_formations(executable)};
  simulation::actor_environment environment{.height{[&](int32_t x, int32_t y){ return terrain.elevation(x, y); }}, .surface{[&](int32_t x, int32_t y){ return resources::surface_type(features, super_features, x, y); }}};
  simulation::combat_random random; random.next(); simulation::mission_actors actors{mission, formations, models, trig, environment, map, random.global_seed}; random.global_seed = actors.random_seed;
  apache::render::mobile_scene mobiles{models}; simulation::mission_spatial spatial{mission, scene, actors, mobiles, terrain};
  environment.relocate = [&](simulation::actor_state &actor, apache::math::vector3 const &position){ spatial.move(actor, position); };
  simulation::scenery_collisions structures{models, trig, scene.base, scene.extra};
  simulation::navigation_environment navigation{.height{environment.height}, .tallest_object{structures.tallest_object}, .obstacle{[&](auto const &from, auto const &to)->std::optional<simulation::navigation_obstacle> { auto hit{structures.line(from, to)}; if(!hit) return {}; return simulation::navigation_obstacle{hit->position, models.at(structures.at(hit->layer, hit->index).model)->height}; }}, .relocate{environment.relocate}};
  std::vector<simulation::actor_armament> armaments(actors.units.size()); std::vector<int32_t> alive(mission.tasks.size());
  for(auto const &actor : actors.units) ++alive[actor.location.task];
  auto const kill{[&](size_t i){ auto &actor{actors.units.at(i)}; std::vector<simulation::actor_state *> followers; for(auto const &group : actors.groups) if(group.units.front() == i) for(size_t j{group.units.size()}; j-- > 1;) followers.push_back(&actors.units[group.units[j]]); simulation::kill_actor(actor, armaments[i], alive[actor.location.task], followers, [&]{ spatial.unplace(i); }, {}); }};
  simulation::movement_callbacks callbacks{.kill{kill}, .place{[&](size_t i){ spatial.place(i); }}, .unplace{[&](size_t i){ spatial.unplace(i); }}};
  simulation::navigation_tables const performance{executable}; simulation::actor_movement movement{actors, mission, formations, models, trig, environment, navigation, performance, random, callbacks};
  actors.synchronize_models(); size_t frame{0};
  auto const placed{mobiles.on_place}, removed{mobiles.on_remove};
  auto const event{[&](int kind, uint32_t id, size_t sector){ output << "event " << frame << ' ' << kind << ' ' << spatial.actor_index(id).value() << ' ' << sector; for(auto v : mobiles.at(id).world.position) output << ' ' << v; output << '\n'; }};
  mobiles.on_place = [&, placed](uint32_t id, size_t sector){ event(1, id, sector); placed(id, sector); };
  mobiles.on_remove = [&, removed](uint32_t id, size_t sector){ event(0, id, sector); removed(id, sector); };
  auto const reference{[&](simulation::shared_target const &t)->int64_t { if(!t) return 0; return t->identity.kind == simulation::target_kind::mobile ? 200001 + static_cast<int64_t>(spatial.actor_index(t->identity.index).value()) : 1 + t->identity.index + t->identity.layer * 100000; }};
  auto const snapshot{[&]{
    output << "snapshot " << frame << ' ' << random.value << ' ' << random.global_seed; for(auto n : spatial.grid.counts()) output << ' ' << n; for(auto n : spatial.grid.peak_counts()) output << ' ' << n; output << '\n';
    for(size_t i{0}; i != actors.units.size(); ++i) {
      auto const &a{actors.units[i]}; output << "actor " << i << ' ' << (a.conditions & ~0x8000u) << ' ' << a.object.status; for(auto v : a.object.world.position) output << ' ' << v; for(auto v : a.object.world.rotation) output << ' ' << v; for(auto v : a.angles) output << ' ' << v; for(auto v : a.velocity) output << ' ' << v; output << ' ' << a.speed << ' ' << (a.waypoint ? static_cast<int64_t>(*a.waypoint) : -1) << ' ' << a.destroyed << ' ' << a.desired_speed << ' ' << a.speed_ramp_target << ' ' << a.retreat_until << '\n';
    }
    for(size_t sector{0}; sector != 4096; ++sector) {
      auto const ids{mobiles.sector(sector)}; if(!ids.empty()) { output << "mobile " << sector; for(auto id : ids) output << ' ' << spatial.actor_index(id).value(); output << '\n'; }
      for(unsigned int side{1}; side <= 2; ++side) if(auto const *cell{spatial.grid.sector(static_cast<int32_t>(sector / 64), static_cast<int32_t>(sector % 64), side)}) {
        output << "sector " << sector << ' ' << side << ' ' << reference(cell->radar); for(auto f : cell->floor) output << ' ' << f; output << ' ' << static_cast<unsigned int>(cell->air.count); for(auto const &t : cell->air.targets) output << ' ' << reference(t); output << ' ' << static_cast<unsigned int>(cell->ground.count); for(auto const &t : cell->ground.targets) output << ' ' << reference(t); output << '\n';
      }
    }
  }};
  simulation::movement_time timing; std::string command;
  while(input >> command) {
    if(command == "mode") input >> timing.arcade >> timing.game_flow;
    else if(command == "kill") { size_t i{0}; input >> i; kill(i); } else if(command == "retreat") { size_t i{0}; input >> i; auto &actor{actors.units.at(i)}; input >> actor.retreat_until; actor.conditions |= 2; } else if(command == "advance") { size_t task{0}; bool advance{false}; int32_t delta{0}; input >> task >> advance >> delta; for(auto const &group : actors.groups) if(group.task == task) { simulation::ground_movement ground{actors.units, group, mission.tasks[task], formations[group.definition.formation], trig, environment}; ground.finish_battle(advance, delta); } } else if(command == "snapshot") snapshot();
    else if(command == "step") { bool capture{false}; input >> timing.now >> timing.delta >> capture; movement.update(timing); actors.synchronize_models(); if(capture) snapshot(); ++frame; } else throw std::runtime_error{"Unknown mission spatial command"};
    if(!input) throw std::runtime_error{"Incomplete mission spatial command"};
  }
}
