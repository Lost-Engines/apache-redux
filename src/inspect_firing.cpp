#include "inspect_firing.h"
#include <istream>
#include <ostream>
#include <stdexcept>
#include <string>
#include "simulation/firing_solution.h"

void inspect_firing(std::istream &input, std::ostream &output, apache::resources::data_store const &data) {
  namespace math = apache::math;
  namespace simulation = apache::simulation;
  math::trigonometry const trig{data};
  auto const weapons{simulation::weapon_definitions()};
  auto const point{[&]{
    math::vector3 p;
    for(auto &v : p) input >> v;
    return p;
  }};
  auto const vector{[&](auto const &p){
    for(auto v : p) output << ' ' << v;
  }};
  std::string command;
  while(input >> command) {
    unsigned int id{0};
    input >> id;
    auto const &weapon{weapons.at(id)};
    if(command == "bounds") {
      auto const a{simulation::firing_angles(weapon)};
      output << a.heading << ' ' << a.minimum_pitch << ' ' << a.maximum_pitch << '\n';
    } else if(command == "aim") {
      simulation::firing_inputs state;
      bool clear{false};
      input >> state.source_height >> state.night >> clear;
      state.source.position = point();
      state.source.direction = point();
      state.target.position = point();
      state.target.direction = point();
      input >> state.target.speed;
      std::vector<int32_t> queries;
      auto const solution{simulation::aim_weapon(weapon, state, trig, [&](math::vector3 const &a, math::vector3 const &b){
        for(auto v : a) queries.push_back(v);
        for(auto v : b) queries.push_back(v);
        return clear;
      })};
      output << static_cast<int>(solution.status) << ' ' << queries.size();
      vector(queries);
      if(solution.status == simulation::firing_status::ready) {
        vector(solution.pose.position);
        vector(solution.pose.rotation);
      }
      output << '\n';
    } else if(command == "ballistic") {
      bool moving{false};
      input >> moving;
      auto const source{point()};
      simulation::movement_target target;
      target.position = point();
      target.direction = point();
      input >> target.speed;
      auto const aim{simulation::ballistic_aim(weapon, source, target, moving)};
      output << aim[0] << ' ' << aim[1] << ' ' << aim[2] << '\n';
    } else if(command == "bomb") {
      math::pose source;
      source.position = point();
      for(auto &v : source.rotation) input >> v;
      auto const target{point()};
      auto const aim{simulation::laser_bomb_aim(source, target, weapon, trig)};
      output << static_cast<int>(aim.has_value());
      if(aim) {
        vector(aim->position);
        vector(aim->rotation);
      }
      output << '\n';
    } else {
      throw std::runtime_error{"Unknown firing operation"};
    }
    if(!input) throw std::runtime_error{"Incomplete firing operation"};
  }
}
