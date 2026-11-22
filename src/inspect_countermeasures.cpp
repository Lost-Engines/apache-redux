#include "inspect_countermeasures.h"
#include <istream>
#include <ostream>
#include <stdexcept>
#include <string>
#include "simulation/countermeasures.h"

void inspect_countermeasures(std::istream &input, std::ostream &output, apache::resources::data_store const &data) {
  namespace simulation = apache::simulation;
  simulation::combat_tables const tables{apache::resources::executable_data{data}};
  auto const weapons{simulation::weapon_definitions()};
  std::string command;
  while(input >> command) {
    simulation::countermeasure_actor actor;
    simulation::countermeasure_controls controls;
    unsigned int weapon{0}, key{0}; int32_t now{0}, speed{0}; int level{0}; uint32_t seed{0}; bool handled{false};
    input >> weapon >> now >> level >> key >> actor.classification >> actor.player >> actor.conditions >> actor.retreat_until >> actor.chaff_at >> actor.flare_at >> actor.random >> seed;
    for(auto &x : actor.position) input >> x;
    apache::math::vector3 position; for(auto &x : position) input >> x;
    input >> speed >> controls.mode >> controls.faults >> controls.infinite >> controls.automatic >> handled;
    for(auto *values : {&controls.ammunition, &controls.pylon_ammunition, &controls.deadline}) for(auto &x : *values) input >> x;
    if(!input) throw std::runtime_error{"Incomplete countermeasure inspection operation"};
    std::vector<std::array<int32_t, 2>> events;
    simulation::countermeasure_callbacks callbacks;
    callbacks.launch = [&](unsigned int id){ events.push_back({0, static_cast<int32_t>(id)}); return true; };
    callbacks.warning = [&](int32_t arrival){ events.push_back({1, arrival}); };
    callbacks.key_event = [&](uint16_t event){ events.push_back({2, event}); return handled; };
    callbacks.released = [&](unsigned int id){ events.push_back({3, static_cast<int32_t>(id)}); };
    int32_t result{0};
    if(command == "delay") {
      simulation::combat_random random{actor.random, seed}; result = simulation::countermeasure_delay(weapons.at(weapon), tables.skill(level), random); actor.random = random.value; seed = random.global_seed;
    } else if(command == "request") result = simulation::request_countermeasure(actor, controls, weapon, now, tables.skill(level), seed, callbacks);
    else if(command == "player") result = simulation::release_player_countermeasure(actor, controls, weapon, static_cast<uint16_t>(key), now, tables.skill(level), seed, callbacks);
    else if(command == "notify") simulation::notify_incoming_missile(actor, controls, weapons.at(weapon), position, speed, now, tables.skill(level), seed, callbacks);
    else throw std::runtime_error{"Unknown countermeasure inspection operation"};
    output << result << ' ' << actor.conditions << ' ' << actor.retreat_until << ' ' << actor.chaff_at << ' ' << actor.flare_at << ' ' << actor.random << ' ' << seed;
    for(auto const *values : {&controls.ammunition, &controls.pylon_ammunition, &controls.deadline}) for(auto x : *values) output << ' ' << x;
    output << ' ' << events.size(); for(auto const &event : events) for(auto x : event) output << ' ' << x;
    output << '\n';
  }
}
