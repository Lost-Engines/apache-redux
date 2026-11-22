#include "peer_group.h"
#include <bit>

namespace apache::multiplayer {
peer_group::peer_group(transport_options options, std::string name) : wire{std::move(options)}, roster{wire.node(), wire.address()}, room{state} {
  for(auto &player : state.players) player.flags = 0xff00;
  state.players[0].aircraft.model = 64; state.players[0].flags = 0;
  room.own_name = name.substr(0, 8); state.players[0].name = room.own_name;
  room.events.identify = [this](uint32_t to){ send(to, identification(message_type::identity, state.local, room.own_name, 64)); };
  room.events.announce = [this]{ announce(); };
  room.events.refresh = room.events.recount = [this]{ refresh(); };
  room.events.cd_reply = [this](uint32_t to){ send(to, packet{message_type::cd_reply, state.local, room.own_name}); };
}
void peer_group::refresh() {
  room.other_players = static_cast<int32_t>(roster.other_count());
  if(changed) changed();
}
void peer_group::reorder(std::array<connection, 16> const &previous) {
  std::array<player_replica, 16> players{};
  for(auto &player : players) player.flags = 0xff00;
  for(size_t i{0}; i < roster.entries.size(); ++i) {
    auto const node{roster.entries[i].node}; if(!node) continue;
    for(size_t j{0}; j < previous.size(); ++j) if(previous[j].node == node) { players[i] = std::move(state.players[j]); break; }
    players[i].flags = (players[i].flags & ~0xff00u) | static_cast<uint32_t>(i << 8);
  }
  state.players = std::move(players); state.local = roster.local(); state.server = 0;
  state.players[state.local].name = room.own_name; state.players[state.local].aircraft.model = 64;
  refresh();
}
void peer_group::announce() {
  send(identification(message_type::name, state.local, room.own_name, 64));
}
void peer_group::send(uint32_t const destination, packet const &message) {
  if(destination >= roster.entries.size() || destination == state.local || !roster.entries[destination].node) return;
  wire.send(roster.entries[destination].address, message);
}
void peer_group::send(packet const &message) {
  for(uint32_t i{0}; i < roster.entries.size(); ++i) send(i, message);
}
void peer_group::name(std::string value) {
  if(state.active) return;
  room.own_name = value.substr(0, 8); state.players[state.local].name = room.own_name; announce(); refresh();
}
void peer_group::ready() {
  if(state.active || roster.other_count() == 0) return;
  state.players[state.local].flags |= 0x10000;
  send(preferences(state.local, room.preferences));
  send(packet{message_type::ready, state.local, room.own_name});
  if(room.events.buttons) room.events.buttons(false);
  refresh();
}
bool peer_group::all_ready() const {
  if(roster.other_count() == 0) return false;
  for(size_t i{0}; i < roster.entries.size(); ++i) if(roster.entries[i].node && !(state.players[i].flags & 0x10000)) return false;
  return true;
}
void peer_group::freeze() {
  roster.sorted = false;
  state.active = true;
}
void peer_group::resume() {
  state.active = false; roster.sorted = true; discovered = false;
  for(auto &player : state.players) player.flags &= ~0x70000u;
  auto const previous{roster.entries}; roster.observe(wire.node(), wire.address(), 0); reorder(previous);
  if(room.events.buttons) room.events.buttons(true);
}
void peer_group::leave() {
  send(packet{message_type::disconnected, state.local, room.own_name}); room.active = state.active = false;
}
void peer_group::forget(uint32_t const slot) {
  if(slot < roster.entries.size() && slot != state.local) roster.remove(roster.entries[slot].node);
}
void peer_group::poll(uint32_t const now) {
  if(!room.active) return;
  for(auto &incoming : wire.poll()) {
    auto const previous{roster.entries}; auto const before{roster.find(incoming.node)};
    if(!before && (state.active || incoming.message.type() != message_type::discover)) continue;
    auto const source{roster.observe(incoming.node, incoming.source, now)}; if(!source) continue;
    if(!before || *before != *source) reorder(previous);
    if(!before) send(*source, identification(message_type::discover, state.local, room.own_name, 64));
    auto &message{incoming.message};
    if(message.type() == message_type::discover) message.word(16, UINT32_MAX);
    else if(message.type() == message_type::name && !state.active) message.word(16, *source);
    else if(message.type() != message_type::server_state && message.type() != message_type::two_player_settings && message.sender() != *source) continue;
    if(message.type() == message_type::disconnected && !state.active) {
      auto const old{roster.entries}; roster.remove(incoming.node); reorder(old); continue;
    }
    if(message.type() == message_type::server_state && *source != state.server) continue;
    if(state.active && game_packet) game_packet(message, std::bit_cast<int32_t>(now));
    else room.receive(message, *source);
  }
  if(!state.active) {
    auto const previous{roster.entries}; if(roster.expire(now)) reorder(previous);
    if(!discovered || std::bit_cast<int32_t>(now - next_discovery) >= 0) {
      discovered = true; next_discovery = now + 600;
      auto const message{identification(message_type::discover, state.local, room.own_name, 64)};
      wire.discover(message); send(message);
      if(state.players[state.local].flags & 0x10000) ready();
    }
  }
}
} // namespace apache::multiplayer
