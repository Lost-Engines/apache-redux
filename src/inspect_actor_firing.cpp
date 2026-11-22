#include "inspect_actor_firing.h"
#include <istream>
#include <ostream>
#include <stdexcept>
#include <string>
#include "simulation/actor_firing.h"

void inspect_actor_firing(std::istream &input, std::ostream &output, apache::resources::data_store const &data) {
  namespace simulation = apache::simulation;
  apache::math::trigonometry const trig{data};
  simulation::combat_tables const tables{apache::resources::executable_data{data}};
  auto const weapons{simulation::weapon_definitions()};
  auto const types{simulation::actor_types()};
  auto const read{[&](auto &values){
    for(auto &v : values) input >> v;
  }};
  auto const write{[&](auto const &values){
    for(auto v : values) output << ' ' << v;
  }};
  std::string command;
  while(input >> command) {
    simulation::actor_state actor;
    simulation::actor_armament armament;
    input >> actor.object.id >> actor.conditions >> actor.weapon >> actor.weapon_slot;
    read(actor.ammunition);
    auto const &type{types.at(actor.object.id)};
    bool result{false};
    std::vector<int64_t> events;
    simulation::combat_random random;
    if(command == "supply") {
      simulation::refresh_weapon_supply(actor, armament, type, weapons);
    } else if(command == "fire") {
      read(armament.deadlines);
      input >> armament.ground_maximum >> armament.ground_minimum >> armament.air_maximum >> armament.air_minimum;
      simulation::actor_firing_context context;
      bool clear{false};
      bool allocate{false};
      bool assigned{false};
      int kind{0};
      int round{0};
      int level{0};
      input >> context.task >> context.now >> context.weather >> context.night >> clear >> allocate >> assigned >> kind >> round >> actor.retreat_until >> context.source_height >> level >> random.value >> random.global_seed;
      read(actor.object.world.position);
      read(actor.object.world.rotation);
      input >> actor.speed;
      read(context.target.position);
      read(context.target.direction);
      input >> context.target.speed >> context.target.classification;
      context.source.identity = {simulation::target_kind::mobile, 0, 100};
      context.player = simulation::target_reference{{simulation::target_kind::mobile, 0, 200}, {}, 0};
      if(assigned) armament.target = std::make_shared<simulation::target_reference>(simulation::target_reference{{static_cast<simulation::target_kind>(kind), 0, 300}, context.target.position, context.target.classification});
      if(round) armament.round = static_cast<size_t>(round);
      simulation::actor_firing_callbacks callbacks;
      auto const record{[&](auto const &values){
        for(auto v : values) events.push_back(v);
      }};
      callbacks.visible = [&](auto const &a, auto const &b){
        events.push_back(2);
        record(a);
        record(b);
        return clear;
      };
      callbacks.construct = [&](simulation::projectile_request const &request)->std::optional<size_t> {
        auto const movement{weapons.at(request.weapon).movement_class};
        events.push_back(movement == 3 ? 1 : movement == 1 ? 3 : 4);
        events.push_back(request.weapon);
        record(request.launch.pose.position);
        if(movement != 3) record(request.launch.pose.rotation);
        events.push_back(request.source.identity.index);
        if(movement == 2) {
          events.push_back(request.designator ? request.designator->identity.index : 0);
          events.push_back(request.selected ? request.selected->identity.index : 0);
        }
        return allocate ? std::optional<size_t>{1} : std::nullopt;
      };
      callbacks.launch = [&](size_t){events.push_back(5);};
      callbacks.fired_at = [&](auto const &, auto const &){events.push_back(6);};
      callbacks.target_released = [&]{events.push_back(8);};
      callbacks.bomb_released = [&]{events.push_back(9);};
      callbacks.remove_flame = [&](size_t){events.push_back(10);};
      result = simulation::fire_actor(actor, armament, type, weapons, trig, tables.skill(level), random, context, callbacks);
    } else {
      throw std::runtime_error{"Unknown actor firing operation"};
    }
    if(!input) throw std::runtime_error{"Incomplete actor firing operation"};
    output << result << ' ' << actor.conditions << ' ' << actor.weapon << ' ' << actor.weapon_slot;
    write(actor.ammunition);
    write(armament.deadlines);
    output << ' ' << armament.ground_maximum << ' ' << armament.ground_minimum << ' ' << armament.air_maximum << ' ' << armament.air_minimum
      << ' ' << bool(armament.target) << ' ' << armament.round.value_or(0) << ' ' << actor.retreat_until << ' ' << random.value << ' ' << random.global_seed << ' ' << events.size();
    write(events);
    output << '\n';
  }
}
