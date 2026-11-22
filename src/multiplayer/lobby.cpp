#include "lobby.h"
#include <bit>

namespace apache::multiplayer {
void lobby::named(uint32_t const sender, std::string const &name) {
  auto &player{state.players[sender]};
  player.name = name;
  player.flags = (player.flags & ~0xff00u) | (sender << 8);
}
bool lobby::receive(packet const &message, uint32_t const transport_slot) {
  auto sender{message.sender()};
  if(message.type() == message_type::discover && sender == UINT32_MAX) sender = transport_slot;
  if(sender >= state.players.size() || state.local >= state.players.size() || transport_slot >= state.players.size()) return false;
  switch(message.type()) {
  case message_type::discover:
    if(state.active || (message.sender() == state.local && message.sender() != UINT32_MAX)) return false;
    named(sender, message.name());
    state.players[sender].aircraft.model = std::bit_cast<int32_t>(message.word(76));
    second_product = message.word(76) == 64 ? 1 : 2;
    if(events.connection_name) events.connection_name(transport_slot, message.name());
    if(events.refresh) events.refresh();
    if(events.identify) events.identify(transport_slot);
    if(events.announce) events.announce();
    return true;
  case message_type::identity:
    if(state.active) return false;
    if(events.recount) events.recount();
    if(events.refresh) events.refresh();
    return true;
  case message_type::peers_changed:
    if(state.active) return false;
    if(events.recount) events.recount();
    return true;
  case message_type::name:
    if(state.active) return false;
    if(sender != state.local) {
      named(sender, message.name());
      if(events.connection_name) events.connection_name(transport_slot, message.name());
      if(mode != game_mode::single_player && mode != game_mode::multiplayer && mode != game_mode::capture_the_flag && other_players > 0) {
        // The positive-count branch always uses this text, even with two peers.
        std::string const text{"Found one other player"};
        if(text != last_message) {
          if(events.message) events.message(text);
          last_message = text;
        }
      }
    }
    if(events.refresh) events.refresh();
    if(events.recount) events.recount();
    return true;
  case message_type::preferences:
    if(state.active || (flow != 5 && flow != 6)) return false;
    for(size_t i{0}; i < preferences.size(); ++i) preferences[i] = std::bit_cast<int32_t>(message.word(24 + 4 * i));
    if(events.preferences_changed) events.preferences_changed();
    return true;
  case message_type::menu_selection:
    {
    if(sender == state.local) return false;
    auto const selected{message.word(44)};
    std::array<int32_t, 5> arguments;
    for(size_t i{0}; i < arguments.size(); ++i) arguments[i] = std::bit_cast<int32_t>(message.word(48 + 4 * i));
    if(selected == 19) {
      other_players = 0;
      active = false;
      return true;
    }
    if(selected == 17) selected_mission = arguments[2];
    if(selected == 16) arguments[2] = selected_mission;
    if((selected >= 1 && selected <= 11) || (selected >= 13 && selected <= 18)) {
      if(events.selection) events.selection(selected, arguments);
      if(selected == 10 || selected == 11) remote_rank = selected == 10 ? 1 : 2;
      if((selected == 1 || selected == 10 || selected == 11) && events.reset_connection) events.reset_connection();
      return true;
    }
    return false;
  }
  case message_type::ready:
    if(events.refresh) events.refresh();
    state.players[sender].flags |= 0x10000;
    if(events.buttons) events.buttons(false);
    return true;
  case message_type::synchronise:
    if(state.local != state.server) return false;
    if((state.players[state.local].flags & 0x20000) && !(state.players[sender].flags & 0x20000)) {
      state.players[sender].flags |= 0x20000;
      state.players[sender].aircraft.model = std::bit_cast<int32_t>(message.word(76));
    }
    return true;
  case message_type::two_player_settings:
    if(events.reset_connection) events.reset_connection();
    for(size_t i{0}; i < two_player.size(); ++i) two_player[i] = std::bit_cast<int32_t>(message.word(4 + 4 * i));
    two_player[1] = static_cast<uint8_t>(1 - static_cast<uint8_t>(two_player[1]));
    two_player[3] = static_cast<uint8_t>(two_player[3]);
    two_player[4] = static_cast<uint8_t>(two_player[4]);
    if(events.settings_changed) events.settings_changed();
    if(two_player[0] == 1 || two_player[0] == 2) {
      own_name = two_player[1] ? "Player 2" : "Player 1";
      rank = two_player[1] ? 2 : 1;
    }
    if(events.start) events.start();
    return true;
  case message_type::chat:
    {
    std::string text;
    for(size_t i{24}; i < 104 && message.bytes()[i]; ++i) text += static_cast<char>(message.bytes()[i]);
    if(events.chat) events.chat("> " + message.name() + " : " + text);
    return true;
  }
  case message_type::cd_question:
    if(cd_available && events.cd_reply) events.cd_reply(sender);
    return true;
  case message_type::cd_reply:
    if(state.local == state.server) cd_found = true;
    return true;
  default:
    return false;
  }
}
} // namespace apache::multiplayer
