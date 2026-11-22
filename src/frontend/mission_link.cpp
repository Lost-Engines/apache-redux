#include "mission_link.h"
#include <algorithm>
#include <bit>
#include <stdexcept>

namespace apache::frontend {
namespace {
bool due(int32_t now, int32_t deadline) {
  return math::subtract(now, deadline) >= 0;
}
}
mission_link::mission_link(multiplayer::peer_group &group, mission_session &session, std::function<int32_t()> timer)
  : peers{group},
    mission{session},
    clock{std::move(timer)} {
  if(!mission.network || !clock) throw std::invalid_argument{"A network mission and clock are required"};
  auto &state{mission.network->state};
  peers.freeze();
  peers.state.server = state.server;
  synchronised_server = state.server;
  state.war_seed = mission.runtime.random.value;
  state.weapon_seed = mission.runtime.weapon_random.value;
  state.wind_strength = mission.runtime.mission.wind_strength;
  state.wind_direction = mission.runtime.mission.wind_direction;
  if(state.client_server && state.server == state.local) state.players[state.local].flags |= 0x20000;
  peers.game_packet = [this](auto const &message, int32_t now){receive(message, now);};
  mission.network->send = [this](auto const &message){
    if(message.type() == multiplayer::message_type::client_state) {
      peers.send(mission.network->state.server, message);
    } else {
      peers.send(message);
    }
  };
  mission.network->lifecycle.events.unregister = [this](uint32_t slot){peers.forget(slot);};
  mission.network->lifecycle.events.next_server = [this](uint32_t slot){return next_server(slot);};
  mission.network_clock = clock;
  mission.poll_network = [this]{poll();};
  mission.wait_network = [this]{wait();};
  deadline = clock();
}
mission_link::~mission_link() {
  close();
  peers.game_packet = {};
  mission.poll_network = mission.wait_network = {};
  mission.network_clock = {};
  mission.network->send = {};
  mission.network->lifecycle.events.unregister = {};
  mission.network->lifecycle.events.next_server = {};
}
uint32_t mission_link::next_server(uint32_t const previous) const {
  auto const &state{mission.network->state};
  for(uint32_t offset{1}; offset < 16; ++offset) {
    auto const i{(previous + offset) % 16};
    if(peers.members().entries[i].node && ((state.players[i].flags >> 8) & 255) != 255) return i;
  }
  return state.local;
}
void mission_link::receive(multiplayer::packet const &message, int32_t const now) {
  using enum multiplayer::message_type;
  auto &network{*mission.network};
  auto &state{network.state};
  if(message.type() == chat) {
    std::string text;
    for(size_t i{24}; i < 104 && message.bytes()[i]; ++i) text += static_cast<char>(message.bytes()[i]);
    if(mission.chat) mission.chat->add("> " + message.name() + ": " + text, mission.runtime.timing.now);
    return;
  }
  if(message.type() == menu_selection && message.word(44) == 19) {
    network.receive(multiplayer::packet{left, message.sender(), message.name()}, now);
    return;
  }
  network.receive(message, now);
  peers.state.server = state.server;
  if(complete && state.client_server && state.local == state.server && message.type() == synchronise) {
    peers.send(message.sender(), multiplayer::packet{server_ready, state.local, state.players[state.local].name});
    mission.send_server_frame();
  }
}
void mission_link::initial_state() {
  auto const &state{mission.network->state};
  auto const &runtime{mission.runtime};
  math::vector3 position{runtime.pilot->readings().position.position};
  for(auto const &actor : runtime.actors.units) {
    if(runtime.mission.tasks[actor.location.task].user && actor.serial == static_cast<int32_t>(state.local) && actor.location.member == 0) {
      position = actor.object.world.position;
      break;
    }
  }
  peers.send(multiplayer::initial_state(state.local, state.players[state.local].name, position, runtime.random.value, runtime.weapon_random.value));
}
void mission_link::initialised() {
  if(transferring) {
    transferring = false;
    return;
  }
  auto &state{mission.network->state};
  if(!state.client_server) {
    mission.runtime.random.value = state.war_seed;
    mission.runtime.weapon_random.value = state.weapon_seed;
    state.number_initial_synced = 0;
  } else {
    state.players[state.local].flags |= 0x20000;
  }
  mission.initialise_remote_players();
  complete = true;
}
void mission_link::poll() {
  if(closed || mission.finished) return;
  auto &state{mission.network->state};
  auto const now{clock()};
  peers.state.server = state.server;
  peers.poll(static_cast<uint32_t>(now));
  if(!state.active) {
    mission.finished = true;
    return;
  }
  if(complete && state.client_server && synchronised_server != state.server) {
    // A successor needs a fresh ready notification. The original departure
    // receiver clears server_in_game, so state packets alone cannot restart
    // client uploads. Keep the existing aircraft and scores during handover.
    synchronised_server = state.server;
    transferring = true;
    attempts = 0;
    deadline = now;
    state.reset_server();
  }
  if(complete && !transferring) return;
  uint32_t needed{0};
  for(uint32_t i{0}; i < 16; ++i) {
    if(i != state.local && peers.members().entries[i].node) ++needed;
  }
  if(!state.client_server) {
    if(due(now, deadline)) {
      initial_state();
      deadline = math::add(now, 2000);
      if(++attempts > 7) {
        state.active = false;
        mission.finished = true;
        return;
      }
    }
    if(state.number_initial_synced >= needed) initialised();
  } else if(state.server == state.local) {
    if(due(now, deadline)) {
      peers.send(multiplayer::packet{multiplayer::message_type::server_waiting, state.local, state.players[state.local].name});
      deadline = math::add(now, 600);
      uint32_t synced{0};
      for(uint32_t i{0}; i < 16; ++i) {
        if(i != state.local && (state.players[i].flags & 0x20000)) ++synced;
      }
      if(synced == needed) {
        peers.send(multiplayer::packet{multiplayer::message_type::server_ready, state.local, state.players[state.local].name});
        mission.send_server_frame();
        state.all_synced = true;
        initialised();
      } else if(++attempts >= 15) {
        state.active = false;
        mission.finished = true;
      }
    }
  } else if(state.server_in_game) {
    initialised();
  } else if(due(now, deadline)) {
    auto const model{static_cast<int32_t>(mission.runtime.actors.units[*mission.runtime.timing.player].object.id)};
    peers.send(state.server, multiplayer::identification(multiplayer::message_type::synchronise, state.local, state.players[state.local].name, model));
    deadline = math::add(now, 600);
    if(++attempts > 10 && !state.server_waiting) {
      state.active = false;
      mission.finished = true;
    }
  }
}
void mission_link::wait() {
  if(!ready() || closed) return;
  auto &state{mission.network->state};
  poll();
  if(!ready() || !state.active || mission.finished) return;
  if(state.client_server && state.server == state.local) {
    mission.send_server_frame();
    return;
  }
  auto const started{clock()};
  int32_t retry{math::add(started, 200)};
  unsigned int failures{0};
  auto waiting{[&]{
    return state.client_server ? state.server_in_game && !state.new_server_data : state.number_synced < static_cast<uint32_t>(std::max(0, mission.network->lifecycle.other_players));
  }};
  while(state.active && !mission.finished && waiting()) {
    poll();
    auto const now{clock()};
    if(state.client_server && due(now, state.answer_deadline)) {
      state.server = next_server(state.server);
      state.answer_deadline = math::add(now, 2000);
      peers.state.server = state.server;
      break;
    }
    if(!state.client_server && due(now, retry)) {
      initial_state();
      retry = math::add(now, 400);
      if(++failures > 4) {
        mission.communications.add("Network Failed", 3, 100);
        mission.communications.add("Re-Syncing", 3, 100);
        break;
      }
    }
    if(waiting() && (!yield || !yield())) {
      state.active = false;
      mission.finished = true;
      break;
    }
  }
  if(state.client_server) mission.network_wait_ticks = math::subtract(clock(), started);
}
void mission_link::close() {
  if(closed) return;
  auto const &state{mission.network->state};
  auto const type{!state.client_server ? multiplayer::message_type::left : state.server == state.local ? multiplayer::message_type::server_left : multiplayer::message_type::client_left};
  peers.send(multiplayer::packet{type, state.local, state.players[state.local].name});
  closed = true;
}
} // namespace apache::frontend
