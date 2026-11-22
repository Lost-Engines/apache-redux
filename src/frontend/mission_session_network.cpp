#include <algorithm>
#include <stdexcept>
#include "mission_session.h"

namespace apache::frontend {
void mission_session::network_crash_complete() {
  auto const mode{network->lifecycle.mode};
  if(mode == multiplayer::game_mode::pilot_gunner) return;
  views->context.crash_timer = INT32_MAX;
  if(mode == multiplayer::game_mode::single_missions || mode == multiplayer::game_mode::apache_vs_hind) {
    auto const other{network->state.local < 2 ? network->actor(1 - network->state.local) : std::nullopt};
    if(other && !runtime.actors.units[*other].destroyed) {
      drone = other; select_view(11); views->context.following = network->lifecycle.following = true;
    } else {
      if(network->send) network->send(multiplayer::packet{multiplayer::message_type::left, network->state.local, network->state.players[network->state.local].name});
      finished = true;
    }
    return;
  }
  select_view(views->modes.home); views->context.crash_timer = 0;
  runtime.spatial.place(*runtime.timing.player); restart_player();
}
void mission_session::restart_player() {
  if(briefing.flow == 6 && (runtime.mission.tasks.size() < 3 || runtime.mission.tasks[2].actors.empty() || runtime.mission.tasks[2].actors[0].waypoints.size() < 16)) {
    communications.add("Capture mission has no respawn points", 3, 2000); finished = true; return;
  }
  auto &unit{runtime.actors.units[*runtime.timing.player]};
  auto const *route{&runtime.mission.tasks.at(unit.location.task).actors.at(unit.location.group).waypoints};
  unsigned int start{0};
  if(briefing.flow == 5 || briefing.flow == 6) {
    auto const count{briefing.flow == 5 ? 7u : 16u};
    do { start = next_random() % count; } while(start == last_start_position);
    if(briefing.flow == 6) route = &runtime.mission.tasks.at(2).actors.at(0).waypoints;
  }
  if(start >= route->size()) { communications.add("Mission has no respawn point", 3, 2000); finished = true; return; }
  last_start_position = start;
  auto position{route->at(start).position}; position[2] = runtime.terrain.elevation(position[0], position[1]);
  auto &arm{*runtime.player_armament};
  auto const flags{unit.object.flags}, status{unit.object.status};
  unit.object = render::construct_model(runtime.models, 103, unit.object.world, simulation::player_pod_models(arm.inventory.pylons, runtime.models));
  unit.object.flags = flags; unit.object.status = status;
  runtime.pilot->reposition(position);
  if(network->send) network->send(multiplayer::restarted(network->state.local, network->state.players[network->state.local].name, unit.object.world));
  arm.inventory.rearm(); *arm.target = {}; arm.mounts.hydra_elevation = 0;
  for(size_t i{0}; i < 4; ++i) controls.keyboard[i] = 0;
  controls.configuration.resolve(); controls.flight.controls.fill(0); controls.external.fill(0);
  runtime.sensors->state = simulation::player_targeting{static_cast<unsigned int>(network->lifecycle.mode)};
  runtime.sensors->state.tads = false; runtime.sensors->optical = {}; runtime.sensors->optical_symbol.reset();
  instruments->state.displays.reset(); communications = {}; ending.cause = 0;
  runtime.pilot->restart(briefing.flow); runtime.pilot->set_faults(0);
  auto &damage{runtime.combat.attention[*runtime.timing.player]}; damage.damage = damage.processed_damage = 0;
  runtime.player_damage->state.processed_damage = 0; runtime.player_damage->state.faults = 0;
  runtime.flight_active = true; network->state.dead = false;
}
void mission_session::landing_award() {
  if(!network) return;
  auto const mode{network->lifecycle.mode};
  if(mode == multiplayer::game_mode::capture_the_flag) {
    for(auto const &actor : runtime.actors.units) if(actor.location.task == 3 && actor.object.id == 994) {
      auto const &position{runtime.pilot->readings().position.position};
      int64_t const x{math::subtract(actor.object.world.position[0], position[0])}, y{math::subtract(actor.object.world.position[1], position[1])};
      if(math::integer_sqrt(static_cast<uint64_t>(x * x) + static_cast<uint64_t>(y * y)) < 51200) {
        network->flag_winner = network->state.local;
        if(network->send) network->send(multiplayer::flag_won(network->state.local, network->state.players[network->state.local].name));
        finished = true; return;
      }
      break;
    }
  } else if(mode != multiplayer::game_mode::combat && mode != multiplayer::game_mode::multiplayer) return;
  // Rearming preserves the selected target; it resets weapon and pod state.
  auto const target{*runtime.player_armament->target};
  runtime.player_armament->rearm(runtime.mission.weapons); *runtime.player_armament->target = target;
  runtime.pilot->set_faults(0); runtime.pilot->refuel();
}
void mission_session::initialise_remote_players() {
  if(!network || crew) return;
  auto const mode{network->lifecycle.mode};
  for(uint32_t i{0}; i < network->state.players.size(); ++i) {
    auto &entry{network->state.players[i]};
    if(i == network->state.local || ((entry.flags >> 8) & 255) == 255) continue;
    auto model{entry.aircraft.model}; size_t task{0};
    if(model == 64 || model == 103) model = 64;
    else if(model == 222 || model == 607) { model = 990; task = 1; }
    if(network->assign(i, static_cast<uint32_t>(model), task, mode, briefing.flow, static_cast<unsigned int>(briefing.zone))) {
      // initialise_network_users_ starts the received attitude at identity.
      entry.aircraft.location.rotation = math::identity;
    }
  }
  if(mode == multiplayer::game_mode::combat || mode == multiplayer::game_mode::multiplayer || mode == multiplayer::game_mode::capture_the_flag) network->remove_unassigned();
}
void mission_session::send_frame() {
  auto &state{network->state}; auto const &readings{runtime.pilot->readings()};
  if(state.client_server) {
    if(state.local == state.server) { if(poll_network) poll_network(); } else if(state.server_in_game && state.new_server_data && !state.dead && network->send) {
      network->send(multiplayer::client_state(state.local, readings.position, readings.airspeed, frame_time - network_wait_ticks / 200));
      state.new_server_data = false;
    }
  } else {
    network->send_casualties();
    if(network->send) network->send(multiplayer::position(state.local, readings.position, readings.airspeed, network->identify(runtime.sensors->state.current)));
    runtime.random.global_seed = 1;
  }
}
void mission_session::send_server_frame() {
  if(!network || !network->state.client_server || network->state.server != network->state.local || !network->send) return;
  auto &state{network->state}; auto const &readings{runtime.pilot->readings()};
  auto &local{state.players[state.local].aircraft};
  local.location = readings.position; local.speed = readings.airspeed; local.model = static_cast<int32_t>(runtime.actors.units[*runtime.timing.player].object.id);
  std::array<multiplayer::aircraft_state, 16> players;
  for(size_t i{0}; i < players.size(); ++i) players[i] = state.players[i].aircraft;
  network->send(multiplayer::server_state(players));
  for(auto &player : state.players) player.flags &= ~0x40000u;
}
} // namespace apache::frontend
