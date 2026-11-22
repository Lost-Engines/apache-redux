#include "mission_crew.h"
#include <algorithm>
#include <bit>
#include <stdexcept>

namespace apache::simulation {
mission_crew::mission_crew(mission_runtime &mission, mission_network &connection)
  : runtime{mission},
    network{connection} {
  resources::reader pointers{runtime.executable.at(0x4e0464, 80), "Crew radio message pointers"};
  auto const effects{runtime.executable.at(0x4e04dc, 20)};
  for(size_t role{0}; role < 2; ++role) {
    for(size_t i{0}; i < 10; ++i) {
      resources::reader text{runtime.executable.at(pointers.u32(), 18), "Crew radio message"};
      messages[role][i] = text.terminated_text(18);
      samples[role][i] = effects[role * 10 + i];
    }
  }
  network.state.crew_received = [this](uint32_t, auto const &state){receive(state);};
}
mission_crew::~mission_crew() {
  network.state.crew_received = {};
}
void mission_crew::receive(multiplayer::crew_state const &state) {
  if(!runtime.pilot || !runtime.player_armament || !runtime.sensors || !runtime.spatial.player) return;
  auto &pilot{*runtime.pilot};
  auto &arm{*runtime.player_armament};
  auto &sensors{*runtime.sensors};
  if(network.state.seat == multiplayer::crew_role::gunner) {
    auto const &p{state.pilot};
    auto readings{pilot.readings()};
    for(size_t i{0}; i < 3; ++i) readings.angles[i] = static_cast<uint32_t>(p.angles[i]);
    readings.position = {p.position, runtime.trig.transformation(readings.angles[0], readings.angles[1], readings.angles[2])};
    readings.airspeed = p.airspeed;
    readings.sidespeed = p.sidespeed;
    readings.climb = p.climb;
    readings.torque = p.torque;
    readings.fuel = p.fuel;
    readings.engines = p.engines;
    pilot.receive_shared(readings, p.landed, p.flight_time, p.weight);
    if(p.landing && landing) landing(p.landing);
    next_frame_ticks = p.frame_ticks;
  } else {
    auto const &g{state.gunner};
    multiplayer::target_state id{g.target_kind, g.target_kind == 2 ? math::vector3{g.drone, g.task, 0} : g.position};
    auto const resolved{network.resolve(id)};
    sensors.state.current = resolved.value_or(target_reference{});
    sensors.state.current.position = g.position;
    sensors.state.current.classification = std::bit_cast<int32_t>(g.target_source);
    sensors.current_display = g.display_position;
    *arm.target = sensors.state.current;
    arm.mounts.hydra_elevation = g.hydra_elevation;
  }
  if(state.message && state.message <= 10 && radio) {
    size_t const role{static_cast<size_t>(network.state.seat)};
    size_t const index{static_cast<size_t>(state.message - 1)};
    radio(messages[role][index], samples[role][index]);
  }
  if(state.leaving) leaving = true;
  if(death) {
    death(state.death);
  } else if(state.death && state.death <= 12) pilot.request_death(state.death);
  auto const old_mode{arm.control_mode};
  arm.control_mode = 2;
  try {
    for(auto code : state.keys) {
      if(key) key(code);
    }
  }
  catch(...) {
    arm.control_mode = old_mode;
    throw;
  }
  arm.control_mode = old_mode;
}
multiplayer::packet mission_crew::transmit(unsigned int const ticks) {
  if(network.state.local >= network.state.players.size()) throw std::runtime_error{"Invalid local shared-cockpit participant"};
  if(!runtime.pilot || !runtime.player_armament || !runtime.sensors) throw std::runtime_error{"Shared cockpit requires a player aircraft"};
  auto const &pilot{*runtime.pilot};
  outgoing.death = static_cast<uint8_t>(pilot.death());
  if(network.state.seat == multiplayer::crew_role::pilot) {
    auto const &readings{pilot.readings()};
    auto const &power{pilot.power()};
    auto &p{outgoing.pilot};
    for(size_t i{0}; i < 3; ++i) p.angles[i] = std::bit_cast<int32_t>(readings.angles[i]);
    p.position = readings.position.position;
    p.flight_time = power.endurance;
    p.climb = readings.climb;
    p.fuel = readings.fuel;
    p.weight = pilot.model_weight;
    p.torque = readings.torque;
    p.sidespeed = readings.sidespeed;
    p.airspeed = readings.airspeed;
    p.engines = readings.engines;
    p.frame_ticks = static_cast<uint8_t>(ticks);
    p.landed = pilot.landed();
  } else {
    auto const &target{runtime.sensors->state.current};
    auto const id{network.identify(target)};
    auto &g{outgoing.gunner};
    g.hydra_elevation = runtime.player_armament->mounts.hydra_elevation;
    g.display_position = runtime.sensors->current_display;
    g.position = target.position;
    g.target_kind = id.kind;
    g.target_source = static_cast<uint32_t>(target.classification);
    if(id.kind == 2) {
      g.drone = std::bit_cast<int16_t>(static_cast<uint16_t>(id.data[0]));
      g.task = std::bit_cast<int16_t>(static_cast<uint16_t>(id.data[1]));
    }
  }
  return multiplayer::crew(network.state.local, network.state.players[network.state.local].name, multiplayer::encode_crew(network.state.seat, outgoing));
}
} // namespace apache::simulation
