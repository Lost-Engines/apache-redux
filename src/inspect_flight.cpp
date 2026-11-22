#include "inspect_flight.h"
#include <bit>
#include <iomanip>
#include <istream>
#include <ostream>
#include <stdexcept>
#include <string>
#include "resources/terrain.h"
#include "simulation/flight_systems.h"
#include "simulation/realistic_flight.h"
#include "simulation/rotor_dynamics.h"

namespace {
auto power_values(apache::simulation::powerplant &power) {
  return std::array{&power.fuel, &power.weight, &power.maximum_weight, &power.maximum_fuel_consumption, &power.weight_factor, &power.fuel_consumption, &power.fuel_rate, &power.endurance, &power.collective, &power.engine_rpm, &power.rotor_rpm, &power.engine_torque, &power.rotor_torque, &power.descent_ratio, &power.engine_rpm_target, &power.torque_target, &power.rotor_rpm_target, &power.left_rpm, &power.right_rpm, &power.temperature, &power.left_temperature, &power.right_temperature, &power.overtorque_timer, &power.warning_timer};
}
auto dynamics_values(apache::simulation::rotor_dynamics &state) {
  return std::array{&state.theta, &state.phi, &state.psi, &state.x, &state.y, &state.z, &state.u, &state.v, &state.zdot, &state.r, &state.p, &state.q, &state.power.collective, &state.power.engine_torque, &state.controls.values[0], &state.controls.values[3], &state.controls.values[2], &state.controls.values[1], &state.roll_d, &state.roll_b, &state.roll_c, &state.roll_decay, &state.power.rotor_torque, &state.power.weight, &state.power.maximum_weight, &state.eta, &state.g, &state.max_speed_timer, &state.power.warning_timer, &state.cos_theta, &state.sin_theta, &state.tan_theta, &state.cos_phi, &state.sin_phi, &state.current_landing_pitch, &state.qmax, &state.qdem, &state.pmax, &state.pdem, &state.rmax, &state.rdem, &state.turnrate, &state.psidot, &state.phidot, &state.thetadot, &state.my_roll, &state.roll_a, &state.roll_decay_ss, &state.rho, &state.kge, &state.kdu, &state.beta, &state.Tz, &state.zdotss, &state.Kt, &state.udot, &state.vdot, &state.abs_v, &state.Kdv};
}
}

void inspect_flight_systems(std::istream &input, std::ostream &output) {
  using apache::simulation::evaluate_landing;
  using apache::simulation::flight_controls;
  using apache::simulation::flight_events;
  using apache::simulation::hover_control;
  using apache::simulation::landing_result;
  using apache::simulation::powerplant;
  std::string operation;
  while(input >> operation) {
    powerplant power;
    flight_controls controls;
    hover_control hover;
    flight_events events;
    double dt{0};
    double altitude{0};
    double u{0};
    double v{0};
    double climb{0};
    double pitch{0};
    int32_t delta_time{0};
    int32_t ground{0};
    int32_t lift{0};
    int32_t maximum_lift{0};
    int32_t airspeed{0};
    int32_t rotor_rate{0};
    bool infinite{false};
    bool invulnerable{false};
    bool overtorque{false};
    bool carrier{false};
    std::array<int32_t, 4> raw_controls{};
    unsigned int surface{0};
    unsigned int previous_death{0};
    input >> dt >> delta_time >> altitude >> ground >> lift >> maximum_lift >> airspeed >> infinite >> invulnerable >> overtorque;
    auto const doubles{power_values(power)};
    for(auto *value : doubles) input >> *value;
    input >> power.faults >> power.overheat_message >> power.overtorque_message >> power.rotor_sound;
    for(auto &value : raw_controls) input >> value;
    for(auto &value : controls.values) input >> value;
    for(auto &value : controls.previous) input >> value;
    input >> controls.changed >> carrier >> hover.active >> hover.previous >> u >> v >> climb >> pitch >> surface >> previous_death;
    if(!input) throw std::runtime_error{"Incomplete flight-system case"};
    landing_result landing;
    if(operation == "fuel") {
      power.consume_fuel(dt, airspeed, infinite, invulnerable, events);
    } else if(operation == "engine") {
      rotor_rate = power.update_engines(dt, altitude, ground, lift, maximum_lift);
    } else if(operation == "temperature") {
      power.update_temperature(dt);
    } else if(operation == "faults") {
      power.update_faults(dt, delta_time, overtorque, events);
    } else if(operation == "controls") {
      controls.update(raw_controls, carrier);
    } else if(operation == "hover") {
      hover.update(u, v, climb, pitch, controls.changed, power.collective, events);
    } else if(operation == "toggle") {
      hover.toggle(u);
    } else if(operation == "landing") {
      landing = evaluate_landing(u, climb, surface, carrier, previous_death);
    } else {
      throw std::runtime_error{"Unknown flight-system operation"};
    }
    output << std::hex;
    for(auto const *value : doubles) output << std::bit_cast<uint64_t>(*value) << ' ';
    for(auto const value : controls.values) output << std::bit_cast<uint64_t>(value) << ' ';
    for(auto const value : controls.previous) output << std::bit_cast<uint64_t>(value) << ' ';
    output << std::bit_cast<uint64_t>(u) << ' ' << std::bit_cast<uint64_t>(v) << std::dec << ' ';
    output << power.faults << ' ' << power.overheat_message << ' ' << power.overtorque_message << ' ' << power.rotor_sound << ' ' << rotor_rate << ' ' << controls.changed << ' ' << hover.active << ' ' << hover.previous << ' ' << landing.grade << ' ' << landing.death << ' ' << landing.landed << ' ' << landing.award;
    for(auto const event : events) output << ' ' << static_cast<unsigned int>(event);
    output << '\n';
  }
}

void inspect_flight_dynamics(std::istream &input, std::ostream &output) {
  using apache::simulation::flight_events;
  using apache::simulation::rotor_dynamics;
  std::string operation;
  while(input >> operation) {
    rotor_dynamics state;
    flight_events events;
    double dt{0};
    double ground{0};
    int32_t delta{0};
    int32_t throttle{0};
    bool carrier{false};
    bool landed{false};
    bool absolute{false};
    unsigned int input_type{0};
    unsigned int random{0};
    input >> dt >> delta >> ground >> carrier >> landed >> absolute >> input_type >> throttle >> random;
    auto const fields{dynamics_values(state)};
    for(auto *value : fields) input >> *value;
    input >> state.power.faults >> state.wind_strength >> state.wind_gust >> state.max_speed_message >> state.Tzw >> state.zdotT >> state.zdotTmax;
    if(!input) throw std::runtime_error{"Incomplete flight-dynamics case"};
    if(operation == "rates") {
      state.update_rates(dt, ground, carrier, landed, absolute, input_type, throttle, events);
    } else if(operation == "aerodynamics") {
      state.update_aerodynamics(dt, delta, ground, carrier, random, events);
    } else {
      throw std::runtime_error{"Unknown flight-dynamics operation"};
    }
    output << std::hex;
    for(auto const *value : fields) output << std::bit_cast<uint64_t>(*value) << ' ';
    output << std::dec << state.power.faults << ' ' << state.wind_strength << ' ' << state.wind_gust << ' ' << state.max_speed_message << ' ' << state.Tzw << ' ' << state.zdotT << ' ' << state.zdotTmax;
    for(auto const event : events) output << ' ' << static_cast<unsigned int>(event);
    output << '\n';
  }
}

void inspect_flight_trace(std::istream &input, std::ostream &output, apache::resources::data_store const &data) {
  namespace math = apache::math;
  namespace simulation = apache::simulation;
  math::trigonometry const trig{data};
  std::optional<apache::resources::terrain> terrain;
  simulation::realistic_flight state;
  std::string operation;
  simulation::flight_environment environment;
  math::pose carrier;
  math::pose relative;
  int32_t carrier_speed{0};
  uint32_t carrier_heading{0};
  environment.height = [&](int32_t const x, int32_t const y){return state.on_carrier ? environment.carrier_deck + 2304 : terrain->elevation(x, y);};
  environment.place = [&](math::pose const &pose){
    if(!state.on_carrier) return pose;
    auto result{math::attach(carrier, relative)};
    result.position[2] = math::add(result.position[2], 2304);
    return result;
  };
  environment.leave_carrier = [&](simulation::realistic_flight &flight){flight.transfer_from_carrier(carrier_speed, carrier_heading, trig);};
  unsigned int surface{0};
  environment.surface = [&](int32_t, int32_t){return surface;};
  while(input >> operation) {
    simulation::flight_step_result result;
    if(operation == "init") {
      std::string zone;
      simulation::flight_start start;
      input >> zone;
      for(auto &v : start.position) input >> v;
      input >> start.heading >> start.pitch >> start.roll >> start.ground_height >> start.dry_weight >> start.fuel >> start.faults >> start.quick_flight;
      input >> start.on_carrier >> start.wind_strength >> start.wind_direction >> environment.carrier_deck >> carrier_speed >> carrier_heading;
      for(auto *pose : {&carrier, &relative}) {
        for(auto &v : pose->position) input >> v;
        for(auto &v : pose->rotation) input >> v;
      }
      terrain.emplace(data.read(zone + ".WZ/GRIDPOST.BIN"));
      state.initialise(start, trig, environment);
    } else if(operation == "step") {
      if(!terrain) throw std::runtime_error{"Flight trace must start with initialization"};
      double dt{0};
      int32_t delta{0};
      unsigned int random{0};
      simulation::flight_input controls;
      input >> dt >> delta;
      for(auto &v : controls.controls) input >> v;
      input >> controls.absolute_throttle >> controls.type >> random >> surface;
      result = state.advance(dt, delta, controls, environment, trig, random);
    } else if(operation == "wind") {
      input >> state.dynamics.wind_strength >> state.wind_direction;
      continue;
    } else if(operation == "fuel") {
      input >> state.dynamics.power.fuel;
      continue;
    } else if(operation == "faults") {
      input >> state.dynamics.power.faults;
      continue;
    } else if(operation == "hover") {
      input >> state.dynamics.hover.active;
      continue;
    } else if(operation == "follow") {
      input >> state.terrain_follow;
      continue;
    } else {
      throw std::runtime_error{"Unknown flight-trace operation"};
    }
    if(!input) throw std::runtime_error{"Incomplete flight-trace operation"};
    auto &d{state.dynamics};
    output << std::hex;
    for(auto const *v : dynamics_values(d)) output << std::bit_cast<uint64_t>(*v) << ' ';
    for(auto const *v : power_values(d.power)) output << std::bit_cast<uint64_t>(*v) << ' ';
    for(auto const v : {state.dx, state.dy, state.dz, state.ground_height, state.preset_altitude, state.pull_up_timer}) output << std::bit_cast<uint64_t>(v) << ' ';
    for(auto const v : d.controls.previous) output << std::bit_cast<uint64_t>(v) << ' ';
    for(auto const v : state.readings.engines) output << std::bit_cast<uint64_t>(v) << ' ';
    for(auto const v : state.attitude) output << std::bit_cast<uint64_t>(v) << ' ';
    output << std::dec << d.power.faults << ' ' << d.wind_strength << ' ' << d.wind_gust << ' ' << d.max_speed_message << ' ' << d.Tzw << ' ' << d.zdotT << ' ' << d.zdotTmax << ' ' << d.controls.changed << ' ' << d.hover.active << ' ' << d.hover.previous << ' ' << state.landed << ' ' << state.on_carrier << ' ' << state.landing_question_asked << ' ' << state.terrain_follow << ' ' << state.reset_altitude << ' ' << state.pull_up_message << ' ' << state.death;
    for(auto const v : state.readings.position.position) output << ' ' << v;
    for(auto const v : state.readings.position.rotation) output << ' ' << v;
    for(auto const v : state.readings.angles) output << ' ' << v;
    output << ' ' << state.readings.airspeed << ' ' << state.readings.sidespeed << ' ' << state.readings.climb << ' ' << state.readings.torque << ' ' << state.readings.fuel << ' ' << result.rotor_rate << ' ' << (result.landing ? result.landing->grade : 0);
    for(auto const event : result.events) output << ' ' << static_cast<unsigned int>(event);
    output << '\n';
  }
}
