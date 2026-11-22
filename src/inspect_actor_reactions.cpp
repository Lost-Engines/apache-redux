#include "inspect_actor_reactions.h"
#include <istream>
#include <ostream>
#include <stdexcept>
#include <string>
#include "simulation/actor_reactions.h"

void inspect_actor_reactions(std::istream &input, std::ostream &output, apache::resources::data_store const &data) {
  namespace simulation = apache::simulation;
  auto const types{simulation::actor_types()};
  auto const weapons{simulation::weapon_definitions()};
  apache::math::trigonometry const trig{data};
  auto const read{[&](auto &v){
    for(auto &x : v) input >> x;
  }};
  auto const write{[&](auto const &v){
    for(auto x : v) output << ' ' << x;
  }};
  std::string command;
  while(input >> command) {
    simulation::actor_state actor;
    simulation::actor_armament armament;
    std::vector<int64_t> events;
    bool target{false};
    if(command == "kill") {
      int32_t alive{0};
      size_t count{0};
      input >> actor.conditions >> actor.object.status >> actor.destroyed >> actor.speed >> target >> actor.retreat_until >> alive >> count;
      if(count > 100) throw std::runtime_error{"Too many follower records"};
      std::vector<simulation::actor_state> followers(count);
      std::vector<simulation::actor_state *> pointers;
      for(auto &f : followers) {
        input >> f.conditions >> f.speed;
        pointers.push_back(&f);
      }
      if(target) armament.target = std::make_shared<simulation::target_reference>();
      simulation::kill_actor(actor, armament, alive, pointers, [&]{events.push_back(1);}, [&]{events.push_back(2);});
      output << actor.conditions << ' ' << actor.object.status << ' ' << actor.destroyed << ' ' << actor.speed << ' ' << bool(armament.target) << ' ' << actor.retreat_until << ' ' << alive;
      for(auto const &f : followers) output << ' ' << f.conditions << ' ' << f.speed;
      output << ' ' << events.size();
      write(events);
      output << '\n';
    } else {
      input >> actor.object.id >> actor.conditions >> actor.object.status >> actor.classification >> actor.weapon >> actor.weapon_slot >> actor.retreat_until >> target;
      read(actor.ammunition);
      read(armament.deadlines);
      input >> armament.ground_maximum >> armament.ground_minimum >> armament.air_maximum >> armament.air_minimum;
      simulation::actor_reaction_context context;
      bool clear{false};
      simulation::combat_warning_times warnings;
      simulation::combat_random weapon_random;
      uint32_t war_random{0};
      input >> context.now >> context.task >> context.attacker_side >> context.attacker_model >> context.player >> clear >> context.attacker.classification >> context.view_heading;
      input >> weapon_random.value >> war_random >> weapon_random.global_seed >> warnings.ships_attacked >> warnings.radar >> warnings.bandit;
      read(context.view_position);
      read(actor.object.world.position);
      read(context.attacker.position);
      context.reference = std::make_shared<simulation::target_reference>(simulation::target_reference{{simulation::target_kind::mobile, 0, 2}, context.attacker.position, context.attacker.classification});
      if(target) armament.target = std::make_shared<simulation::target_reference>(simulation::target_reference{{simulation::target_kind::fixed, 0, 1}, {}, 1});
      simulation::actor_reaction_callbacks callbacks;
      callbacks.visible = [&]{
        events.push_back(1);
        return clear;
      };
      callbacks.target_released = [&]{events.push_back(2);};
      callbacks.sound = [&](unsigned int id, bool cockpit){
        events.push_back(cockpit ? 3 : 4);
        events.push_back(id);
      };
      auto const &type{types.at(actor.object.id)};
      if(command == "acquire") {
        simulation::acquire_attacker(actor, armament, type, weapons, war_random, weapon_random, context, callbacks);
      } else if(command == "fired") {
        simulation::notify_actor_fired_at(actor, armament, type, weapons, warnings, war_random, weapon_random, context, callbacks);
      } else if(command == "targeted") {
        simulation::notify_actor_targeted(actor, armament, type, weapons, warnings, war_random, weapon_random, trig, context, callbacks);
      } else {
        throw std::runtime_error{"Unknown actor reaction"};
      }
      output << actor.conditions << ' ' << actor.weapon << ' ' << actor.weapon_slot << ' ' << actor.retreat_until << ' ' << (armament.target ? armament.target->identity.index : 0);
      write(armament.deadlines);
      output << ' ' << weapon_random.value << ' ' << war_random << ' ' << weapon_random.global_seed << ' ' << warnings.ships_attacked << ' ' << warnings.radar << ' ' << warnings.bandit << ' ' << events.size();
      write(events);
      output << '\n';
    }
    if(!input) throw std::runtime_error{"Incomplete actor reaction"};
  }
}
