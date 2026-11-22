#include "inspect_arcade.h"
#include <bit>
#include <istream>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string>
#include "resources/terrain.h"
#include "simulation/arcade_flight.h"

namespace {
auto arcade_values(apache::simulation::arcade_flight &d) {
  auto &p{d.power};
  auto &c{d.controls};
  return std::array{&d.x, &d.y, &d.z, &d.heading, &d.pitch, &d.roll, &d.delta_heading, &d.delta_pitch, &d.delta_roll, &d.delta_throttle, &d.forward_velocity, &d.lateral_velocity, &d.vertical_velocity, &d.delta_forward_velocity, &d.delta_lateral_velocity, &d.delta_vertical_velocity, &d.lateral_ss, &d.lateral_vmax, &d.max_heading,
      &p.fuel, &p.weight, &p.maximum_fuel_consumption, &p.weight_factor, &p.fuel_consumption, &p.fuel_rate, &p.endurance, &p.engine_rpm, &p.rotor_rpm, &p.left_temperature, &p.right_temperature,
      &c.values[0], &c.values[1], &c.values[2], &c.values[3], &c.previous[0], &c.previous[1], &c.previous[2], &c.previous[3]};
}
} // anonymous namespace

void inspect_arcade(std::istream &input, std::ostream &output) {
  using apache::simulation::arcade_flight;
  using apache::simulation::flight_events;
  std::string operation;
  while(input >> operation) {
    arcade_flight d;
    flight_events events;
    double dt{0};
    bool absolute{false};
    unsigned int input_type{0};
    int32_t throttle{0};
    input >> dt >> absolute >> input_type >> throttle;
    auto &p{d.power};
    auto &c{d.controls};
    auto values{arcade_values(d)};
    for(auto *v : values) input >> *v;
    for(auto &v : d.readings.engines) input >> v;
    std::array<int32_t, 4> raw{};
    for(auto &v : raw) input >> v;
    input >> p.faults >> p.overheat_message >> p.rotor_sound >> c.changed >> d.hover.active >> d.hover.previous >> d.landed >> d.infinite_fuel >> d.invulnerable >> d.readings.airspeed;
    if(!input) throw std::runtime_error{"Incomplete Arcade comparison case"};
    if(operation == "inputs") {
      d.update_inputs(raw);
    } else if(operation == "rates") {
      d.update_rates(dt, events);
    } else if(operation == "motion") {
      d.update_motion(dt, absolute, input_type, throttle, events);
    } else if(operation == "fuel") {
      d.consume_fuel(dt, events);
    } else if(operation == "hover") {
      d.update_hover(dt, events);
    } else if(operation == "toggle") {
      d.toggle_hover();
    } else {
      throw std::runtime_error{"Unknown Arcade comparison operation"};
    }
    output << std::hex;
    for(auto const *v : values) output << std::bit_cast<uint64_t>(*v) << ' ';
    for(auto const v : d.readings.engines) output << std::bit_cast<uint64_t>(v) << ' ';
    output << std::dec << p.faults << ' ' << p.overheat_message << ' ' << p.rotor_sound << ' ' << c.changed << ' ' << d.hover.active << ' ' << d.hover.previous << ' ' << d.landed << ' ' << d.infinite_fuel << ' ' << d.invulnerable << ' ' << d.readings.airspeed;
    for(auto const event : events) output << ' ' << static_cast<unsigned int>(event);
    output << '\n';
  }
}

void inspect_arcade_trace(std::istream &input, std::ostream &output, apache::resources::data_store const &data) {
  namespace math = apache::math;
  namespace simulation = apache::simulation;
  math::trigonometry const trig{data};
  std::optional<apache::resources::terrain> terrain;
  simulation::arcade_flight state;
  simulation::arcade_environment environment;
  math::pose carrier;
  math::pose relative;
  int32_t carrier_speed{0};
  int32_t deck{0};
  uint32_t carrier_heading{0};
  unsigned int surface{0};
  environment.height = [&](int32_t const x, int32_t const y){return state.on_carrier ? deck + 2304 : terrain->elevation(x, y);};
  environment.surface = [&](int32_t, int32_t){return surface;};
  environment.place = [&](math::pose const &pose){
    if(!state.on_carrier) return pose;
    auto result{math::attach(carrier, relative)};
    result.position[2] = math::add(result.position[2], 2304);
    return result;
  };
  environment.leave_carrier = [&](simulation::arcade_flight &flight){flight.transfer_from_carrier(carrier_speed, carrier_heading, trig);};
  std::string operation;
  while(input >> operation) {
    simulation::flight_step_result result;
    if(operation == "init") {
      std::string zone;
      simulation::flight_start start;
      input >> zone;
      for(auto &v : start.position) input >> v;
      input >> start.heading >> start.pitch >> start.roll >> start.ground_height >> start.dry_weight >> start.fuel >> start.faults >> start.quick_flight >> start.on_carrier >> start.wind_strength >> start.wind_direction >> deck >> carrier_speed >> carrier_heading;
      for(auto *pose : {&carrier, &relative}) {
        for(auto &v : pose->position) input >> v;
        for(auto &v : pose->rotation) input >> v;
      }
      input >> start.object_heading;
      terrain.emplace(data.read(zone + ".WZ/GRIDPOST.BIN"));
      state.initialise(start, trig, environment);
    } else if(operation == "step") {
      if(!terrain) throw std::runtime_error{"Arcade trace must start with initialization"};
      double dt{0};
      simulation::flight_input controls;
      input >> dt;
      for(auto &v : controls.controls) input >> v;
      input >> controls.absolute_throttle >> controls.type >> surface;
      result = state.advance(dt, controls, environment, trig);
    } else if(operation == "fuel") {
      input >> state.power.fuel;
      continue;
    } else if(operation == "faults") {
      input >> state.power.faults;
      continue;
    } else if(operation == "hover") {
      input >> state.hover.active;
      continue;
    } else if(operation == "collective") {
      input >> state.delta_vertical_velocity;
      continue;
    } else {
      throw std::runtime_error{"Unknown Arcade trace operation"};
    }
    if(!input) throw std::runtime_error{"Incomplete Arcade trace command"};
    output << std::hex;
    for(auto const *v : arcade_values(state)) output << std::bit_cast<uint64_t>(*v) << ' ';
    for(auto const v : state.readings.engines) output << std::bit_cast<uint64_t>(v) << ' ';
    for(auto const v : {state.height, state.last_frame_ground_height, state.dx, state.dy, state.dz, state.preset_altitude, state.d_height, state.true_dz}) output << std::bit_cast<uint64_t>(v) << ' ';
    for(auto const v : state.attitude) output << std::bit_cast<uint64_t>(v) << ' ';
    output << std::dec << state.power.faults << ' ' << state.power.overheat_message << ' ' << state.power.rotor_sound << ' ' << state.controls.changed << ' ' << state.hover.active << ' ' << state.hover.previous << ' ' << state.landed << ' ' << state.infinite_fuel << ' ' << state.invulnerable << ' ' << state.readings.airspeed << ' ' << state.on_carrier << ' ' << state.reset_altitude << ' ' << state.death;
    for(auto const v : state.readings.position.position) output << ' ' << v;
    for(auto const v : state.readings.position.rotation) output << ' ' << v;
    for(auto const v : state.readings.angles) output << ' ' << v;
    output << ' ' << state.readings.airspeed << ' ' << state.readings.sidespeed << ' ' << state.readings.climb << ' ' << state.readings.torque << ' ' << state.readings.fuel << ' ' << (result.landing ? result.landing->grade : 0);
    for(auto const event : result.events) output << ' ' << static_cast<unsigned int>(event);
    output << '\n';
  }
}
