#include "inspect_projectiles.h"
#include <istream>
#include <ostream>
#include <stdexcept>
#include <string>
#include "resources/terrain.h"
#include "simulation/projectile_dynamics.h"
#include "simulation/projectile_launch.h"

void inspect_projectiles(std::istream &input, std::ostream &output, apache::resources::data_store const &data) {
  namespace simulation = apache::simulation;
  apache::math::trigonometry const trig{data};
  auto const weapons{simulation::weapon_definitions()};
  std::optional<apache::resources::terrain> terrain;
  auto const models{apache::resources::read_models(data.read("3DDATA/3DOBJS.BIN"))};
  auto const point{[&]{
    apache::math::vector3 v;
    for(auto &x : v) input >> x;
    return v;
  }};
  auto const print{[&](auto const &v){
    for(auto x : v) output << x << ' ';
  }};
  std::string command;
  while(input >> command) {
    if(command == "launch") {
      simulation::projectile_launch_state round;
      simulation::projectile_launch_inputs state;
      unsigned int id{0};
      uint32_t random{0};
      uint32_t weapon_random{0};
      int32_t now{0};
      input >> id >> random >> weapon_random >> round.launched_at >> now;
      state.pose.position = point();
      for(auto &v : state.pose.rotation) input >> v;
      state.source.position = point();
      state.source.direction = point();
      input >> state.source.speed >> state.has_target >> state.simple_value;
      simulation::construct_projectile(round, id, weapons.at(id), state, trig, [&](int32_t x, int32_t y){return terrain->elevation(x, y);}, random, weapon_random);
      simulation::launch_projectile(round, now);
      output << round.weapon << ' ' << round.body.motion.distance << ' ' << round.launched_at << ' ' << round.body.motion.speed << ' ' << round.random << ' ' << round.body.motion.flags << ' ';
      print(round.body.pose.position);
      print(round.body.pose.rotation);
      print(round.body.previous);
      print(round.body.angles);
      output << round.body.seeker.flags << ' ' << round.body.seeker.retry_time << ' ' << round.impact_at << ' ' << round.simple_value << ' ';
      print(round.impact);
      output << random << ' ' << weapon_random << ' ';
      if(weapons.at(id).movement_class != 3) {
        auto object{simulation::projectile_model(round, weapons.at(id), models)};
        output << object.id << ' ' << object.status << ' ' << object.flags << ' ' << object.children.size();
        for(auto const &child : object.children) output << ' ' << (child ? child->id : 0);
      } else {
        output << "0 0 0 0";
      }
      output << '\n';
    } else if(command == "terrain") {
      std::string zone;
      input >> zone;
      terrain.emplace(data.read(zone + ".WZ/GRIDPOST.BIN"));
    } else if(command == "bound") {
      auto const start{point()};
      auto const move{point()};
      auto const result{simulation::bound_projectile_move(start, move, [&](int32_t x, int32_t y){return terrain->elevation(x, y);})};
      output << result.hit_terrain << ' ';
      print(result.end);
      print(result.displacement);
      print(result.impact);
      output << '\n';
    } else if(command == "closest") {
      auto const start{point()};
      auto const end{point()};
      auto const target{point()};
      auto const result{simulation::closest_point(start, end, target, trig)};
      output << result.distance << ' ';
      print(result.position);
      output << '\n';
    } else if(command == "move") {
      size_t weapon{0};
      int32_t delta{0};
      simulation::projectile_state round;
      unsigned int adjusted{0};
      bool has_target{false};
      bool actor{false};
      simulation::movement_target target;
      input >> weapon >> delta >> round.motion.speed >> round.motion.distance >> round.motion.flags;
      round.pose.position = point();
      for(auto &v : round.pose.rotation) input >> v;
      round.previous = point();
      for(auto &v : round.angles) input >> v;
      input >> round.seeker.flags >> round.seeker.retry_time >> adjusted >> has_target >> actor;
      round.seeker.adjusted = static_cast<simulation::seeker_target>(adjusted);
      target.position = point();
      target.direction = point();
      input >> target.speed;
      auto const result{simulation::calculate_projectile_move(round, weapons.at(weapon), has_target ? std::optional{target} : std::nullopt, actor, trig, delta)};
      output << round.motion.flags << ' ';
      print(round.pose.position);
      print(round.pose.rotation);
      print(round.angles);
      output << round.seeker.flags << ' ' << round.seeker.retry_time << ' ' << static_cast<int>(round.seeker.adjusted) << ' ' << result.notify_target << ' ' << result.explosion.has_value() << ' ';
      print(result.displacement);
      output << '\n';
    } else {
      throw std::runtime_error{"Unknown projectile operation"};
    }
    if(!input) throw std::runtime_error{"Incomplete projectile operation"};
  }
}
