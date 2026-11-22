#include "inspect_navigation.h"
#include <istream>
#include <ostream>
#include <stdexcept>
#include "simulation/actor_navigation.h"

void inspect_navigation(std::istream &input, std::ostream &output, apache::resources::data_store const &data) {
  namespace math = apache::math;
  namespace simulation = apache::simulation;
  math::trigonometry const trig{data}; simulation::navigation_tables const tables{apache::resources::executable_data{data}};
  bool ship{false}; size_t type{0};
  while(input >> ship >> type) {
    simulation::actor_state unit; simulation::navigation_order order;
    for(auto &v : unit.object.world.position) input >> v;
    for(auto &v : unit.object.world.rotation) input >> v;
    for(auto &v : unit.angles) input >> v;
    for(auto &v : unit.velocity) input >> v;
    input >> unit.speed >> unit.desired_speed >> unit.speed_ramp_target >> unit.conditions >> unit.classification;
    for(auto &v : order.target) input >> v;
    int32_t previous{INT32_MIN}, carrier{INT32_MIN};
    input >> order.waypoint_height >> order.waypoint_speed >> order.arrival_radius >> previous >> carrier >> order.has_target >> order.player_task >> order.arcade >> order.has_waypoint >> order.target_classification;
    if(previous != INT32_MIN) order.previous_height = previous;
    if(carrier != INT32_MIN) order.carrier_speed = carrier;
    int32_t ground{0}, ahead{0}, tallest{0}, delta{0}; bool hit{false}; simulation::navigation_obstacle obstacle;
    input >> ground >> ahead >> tallest >> hit;
    for(auto &v : obstacle.position) input >> v;
    input >> obstacle.height >> delta;
    if(!input) throw std::runtime_error{"Incomplete navigation case"};
    auto const origin{unit.object.world.position};
    simulation::navigation_environment const world{
      .height{[&](int32_t x, int32_t y){ return x == origin[0] && y == origin[1] ? ground : ahead; }},
      .tallest_object{tallest},
      .obstacle{[&](math::vector3 const &, math::vector3 const &)->std::optional<simulation::navigation_obstacle>{ return hit ? std::optional{obstacle} : std::nullopt; }},
    };
    auto performance{ship ? tables.ships.at(type) : tables.aircraft.at(type)};
    if(unit.classification == 1 && order.arcade) performance.acceleration *= 4;
    auto const result{simulation::navigate_actor(unit, order, performance, trig, world, delta)};
    output << result.arrived << ' ' << result.outside_world << ' ' << unit.conditions;
    for(auto v : unit.object.world.position) output << ' ' << v;
    for(auto v : unit.object.world.rotation) output << ' ' << v;
    for(auto v : unit.angles) output << ' ' << v;
    for(auto v : unit.velocity) output << ' ' << v;
    output << ' ' << unit.speed << ' ' << unit.desired_speed << ' ' << unit.speed_ramp_target << '\n';
  }
}
