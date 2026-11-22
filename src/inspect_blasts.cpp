#include "inspect_blasts.h"
#include <algorithm>
#include <istream>
#include <ostream>
#include <stdexcept>
#include <string>
#include "simulation/blast_damage.h"

void inspect_blasts(std::istream &input, std::ostream &output, apache::resources::data_store const &data) {
  namespace math = apache::math;
  namespace resources = apache::resources;
  namespace simulation = apache::simulation;
  auto models{resources::read_models(data.read("3DDATA/3DOBJS.BIN"))};
  resources::executable_data const executable{data};
  resources::initialise_model_metadata(models, executable);
  simulation::combat_tables const tables{executable};
  math::trigonometry const trig{data};
  apache::render::mobile_scene mobiles{models};
  std::array<std::vector<resources::scenery_placement>, 2> placements;
  std::array<std::vector<resources::scenery_placement>, 2> initial;
  std::vector<simulation::damage_recipient> actors;
  std::vector<simulation::damage_recipient> initial_actors;
  std::unique_ptr<simulation::scenery_collisions> scenery;
  std::vector<int64_t> events;
  auto const read{[&](simulation::target_identity const &id){
    if(id.kind == simulation::target_kind::scenery) {
      auto const &record{placements.at(id.layer).at(id.index)};
      simulation::damage_recipient target;
      target.model = record.model;
      target.flags = record.status;
      target.position = {record.x, record.y, 0};
      return target;
    }
    auto result{actors.at(id.index)};
    auto const &object{mobiles.at(id.index)};
    result.model = object.id;
    result.flags = object.status;
    result.position = object.world.position;
    return result;
  }};
  auto const write{[&](simulation::target_identity const &id, simulation::damage_recipient const &target){
    if(id.kind == simulation::target_kind::scenery) {
      auto &record{placements.at(id.layer).at(id.index)};
      record.model = static_cast<uint16_t>(target.model);
      record.status = static_cast<uint16_t>(target.flags);
    } else {
      actors.at(id.index) = target;
      mobiles.at(id.index).status = target.flags;
    }
  }};
  simulation::blast_callbacks callbacks;
  callbacks.read = read;
  callbacks.write = write;
  auto const event{[&](int code, simulation::target_identity const &id){
    events.insert(events.end(), {code, static_cast<int64_t>(id.kind), static_cast<int64_t>(id.layer), id.index, read(id).model});
  }};
  callbacks.action = [&](simulation::target_identity const &id, simulation::damage_action action){
    if(action != simulation::damage_action::kill) throw std::runtime_error{"Unexpected player damage in blast fixture"};
    event(0, id);
    auto target{read(id)};
    target.flags |= 1;
    if(id.kind == simulation::target_kind::scenery) target.model = static_cast<uint32_t>(std::max(0, models.at(target.model)->metadata[5]));
    write(id, target);
  };
  callbacks.partial_explosion = [&](simulation::target_identity const &id){event(1, id);};
  callbacks.auxiliary = [&](simulation::target_identity const &id){
    auto const target{read(id)};
    events.insert(events.end(), {2, (target.flags & 7) >> 1, target.model, id.kind == simulation::target_kind::mobile ? static_cast<int64_t>(id.index) : -1});
  };
  callbacks.weapon_explosion = [&](math::vector3 const &p){events.insert(events.end(), {3, p[0], p[1], p[2]});};
  auto const point{[&]{
    math::vector3 p;
    for(auto &v : p) input >> v;
    return p;
  }};
  std::string command;
  while(input >> command) {
    if(command == "reset") {
      std::string zone;
      input >> zone;
      placements[0] = resources::read_scenery(data.read(zone + ".WZ/GNDOBJS.BIN"));
      resources::initialise_scenery(placements[0]);
      placements[1].clear();
      initial = placements;
      scenery = std::make_unique<simulation::scenery_collisions>(models, trig, placements[0], placements[1]);
      actors.clear();
      initial_actors.clear();
      mobiles.clear();
    } else if(command == "extra") {
      uint16_t model{0};
      uint16_t status{0};
      int32_t x{0};
      int32_t y{0};
      input >> model >> status >> x >> y;
      placements[1].push_back({static_cast<uint16_t>((x >> 22) * 64 + (y >> 22)), model, x, y, 0, 0, status});
      initial[1] = placements[1];
      scenery = std::make_unique<simulation::scenery_collisions>(models, trig, placements[0], placements[1]);
    } else if(command == "add") {
      simulation::damage_recipient actor;
      unsigned int damage{0};
      input >> actor.model >> actor.flags >> actor.network >> damage;
      actor.damage = static_cast<uint8_t>(damage);
      actor.position = point();
      actor.mobile = actor.actor = true;
      auto const id{mobiles.add(actor.model, {actor.position, trig.transformation(0, 0, 0)})};
      mobiles.at(id).status = actor.flags;
      actors.push_back(actor);
      initial_actors.push_back(actor);
    } else if(command == "restore") {
      for(size_t i{0}; i != placements.size(); ++i) std::ranges::copy(initial[i], placements[i].begin());
      actors = initial_actors;
      for(uint32_t i{0}; i != actors.size(); ++i) mobiles.at(i).status = actors[i].flags;
    } else if(command == "explode") {
      unsigned int operation{0};
      unsigned int kind{0};
      unsigned int source_kind{0};
      unsigned int weapon{0};
      simulation::target_identity target;
      simulation::damage_source source;
      simulation::damage_environment environment;
      simulation::damage_random random;
      int level{0};
      input >> operation >> kind >> target.layer >> target.index >> weapon >> source_kind >> source.side >> source.actor >> source.network >> environment.mode >> environment.flow >> level >> random.weapon >> random.round >> random.global;
      target.kind = static_cast<simulation::target_kind>(kind);
      source.kind = static_cast<simulation::target_kind>(source_kind);
      auto const impact{point()};
      environment.difficulty_percent = tables.skill(level).damage_percent;
      environment.decoy_modifier = tables.skill(level).decoy_modifier;
      events.clear();
      simulation::blast_damage blast{weapon, source, environment, random, models, *scenery, mobiles, callbacks};
      if(operation == 0) {
        blast.detonate(impact);
      } else {
        blast.hit(target, impact);
      }
      uint64_t hash{14695981039346656037ull};
      auto const mix{[&](uint32_t value){
        for(unsigned int i{0}; i != 4; ++i) {
          hash = (hash ^ (value & 255)) * 1099511628211ull;
          value >>= 8;
        }
      }};
      for(auto const &layer : placements) {
        for(auto const &record : layer) {
          mix(record.model);
          mix(record.status);
        }
      }
      for(uint32_t i{0}; i != actors.size(); ++i) {
        mix(mobiles.at(i).id);
        mix(mobiles.at(i).status);
        mix(actors[i].damage);
      }
      output << blast.limits.damage << ' ' << blast.limits.radius << ' ' << random.weapon << ' ' << random.round << ' ' << random.global << ' ' << hash << ' ' << events.size();
      for(auto x : events) output << ' ' << x;
      output << '\n';
    } else {
      throw std::runtime_error{"Unknown blast inspection operation"};
    }
    if(!input) throw std::runtime_error{"Incomplete blast inspection operation"};
  }
}
