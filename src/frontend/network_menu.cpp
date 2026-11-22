#include "network_menu.h"
#include <algorithm>
#include <stdexcept>

namespace apache::frontend {
network_menu::network_menu(menu &ui, configuration::settings &preferences) : interface{ui}, settings{preferences} {}
void network_menu::visible(std::string_view const name, bool const value) {
  auto &w{interface.named(name)};
  w.flags = (w.flags & ~1u) | static_cast<unsigned int>(value);
}
void network_menu::message(std::string_view const text) {
  std::move(messages.begin() + 1, messages.end(), messages.begin()); messages.back() = text;
  size_t i{0}; for(auto &w : interface.objects()) if(w.name == "Modem Message Line") w.text = messages[i++];
}
void network_menu::buttons(bool const enabled) {
  for(auto &w : interface.objects()) if(w.action == "notify_weapons_button_" || w.action == "notify_fuel_button_" || w.action == "notify_collision_button_" || w.action == "notify_game_type_button_") w.flags = (w.flags & ~0x8000u) | (enabled ? 0x8000u : 0u);
}
void network_menu::refresh() {
  if(!peers) return;
  auto const &state{peers->state}; auto const &members{peers->members().entries};
  int64_t score{0}; for(auto const &p : state.players) score += p.score;
  size_t row{0}; auto objects{interface.objects()};
  auto const first{static_cast<size_t>(&interface.named("_login_slot_aircraft") - objects.data())};
  for(size_t i{0}; i < members.size(); ++i) if(members[i].node) {
    auto const &p{state.players[i]}; auto const base{first + row * 4};
    objects[base].text = "AH64"; objects[base + 1].text = p.name;
    objects[base + 2].text = score ? std::to_string(p.score) : (p.flags & 0x10000) ? "Ready!" : (p.flags & 0x20000) ? "In Flight" : i == state.server ? "Server" : "Client";
    ++row;
  }
  for(; row < 16; ++row) for(size_t i{0}; i < 4; ++i) objects[first + row * 4 + i].text.clear();
  if(!editing_name) interface.named("_player_name").text = peers->room.own_name;
}
void network_menu::preferences() {
  if(!peers) return;
  auto &room{peers->room}; auto const &p{room.preferences};
  interface.named("_cheat_unlimited_weapons_button").on = p[1] != 0; interface.named("_cheat_limited_weapons_button").on = p[1] == 0;
  interface.named("_cheat_infinite_fuel_button").on = p[0] != 0; interface.named("_cheat_limited_fuel_button").on = p[0] == 0;
  interface.named("_cheat_collision_button").on = p[3] != 0; interface.named("_cheat_no_collision_button").on = p[3] == 0;
  interface.named("_cheat_deathmatch_button").on = p[4] == 1; interface.named("_cheat_capture_the_flag_button").on = p[4] != 1;
  if(!two_player) { room.mode = p[4] == 1 ? multiplayer::game_mode::multiplayer : multiplayer::game_mode::capture_the_flag; room.flow = p[4] == 1 ? 5 : 6; }
}
void network_menu::controls() {
  if(!peers || !two_player) return;
  auto &room{peers->room}; room.mode = static_cast<multiplayer::game_mode>(room.two_player[0]);
  bool const combat{room.mode == multiplayer::game_mode::combat}, shared{room.mode == multiplayer::game_mode::pilot_gunner};
  room.rank = room.two_player[1] ? 2 : 1; room.flow = combat ? 5 : 3;
  interface.named("_two_player_game_mode_button").text = combat ? "Combat" : shared ? "Pilot/Gunner" : "Single Missions";
  interface.named("_red_leader_button").text = shared ? "Pilot" : "Player 1"; interface.named("_blue_leader_button").text = shared ? "Gunner" : "Player 2";
  interface.named("_red_leader_button").on = room.rank == 1; interface.named("_blue_leader_button").on = room.rank == 2;
  for(auto const name : {"_weather_box", "_start_time_box", "_modem_difficulty_box"}) visible(name, combat);
  auto const index{static_cast<size_t>(&interface.named("_weather_buttons") - interface.objects().data())};
  for(size_t i{0}; i < 4; ++i) interface.objects()[index + i].on = static_cast<int32_t>(i) == room.two_player[2];
  interface.named("_day_time_only_button").on = room.two_player[3] == 0; interface.named("_day_and_night_button").on = room.two_player[3] != 0;
  interface.named("_no_crashing_button").on = room.two_player[4] == 0; interface.named("_crashing_button").on = room.two_player[4] != 0;
  interface.bounds("_start_game_box", {330, combat ? 365 : 272, 620, combat ? 386 : 293});
}
void network_menu::connect() {
  auto options{transport}; if(options.bind.port) options.bind.port = static_cast<uint16_t>(options.bind.port + channel);
  if(!host.empty()) options.peers.push_back(platform::network_address::resolve(host, options.bind.port));
  if(peers) { peers->leave(); peers.reset(); }
  peers = std::make_unique<multiplayer::peer_group>(std::move(options), own_name);
  peers->changed = [this]{
    refresh();
    if(two_player && !starting && peers->room.other_players) announce_selection(peers->room.two_player[1] ? 11 : 10);
  };
  auto &room{peers->room};
  room.preferences = {settings.values.integer("Network infinite fuel").value_or(0), settings.values.integer("Network unlimited weapons").value_or(0), 0, settings.values.integer("Network collisions").value_or(1), settings.values.integer("Network deathmatch").value_or(1)};
  room.events.preferences_changed = [this]{ preferences(); };
  room.events.buttons = [this](bool enabled){ buttons(enabled); };
  room.events.message = room.events.chat = [this](std::string_view text){ message(text); };
  room.events.selection = [this](auto code, auto const &arguments){ selection(code, arguments); };
  room.events.settings_changed = [this]{ controls(); };
  room.events.start = [this]{ start_requested = true; };
  preferences(); controls(); refresh(); buttons(true);
}
void network_menu::open(bool const two, std::string name) {
  two_player = two; starting = editing_name = editing_host = false; messages.fill({}); channel = 0;
  own_name = std::string{settings.values.get("Network name").value_or(name)}.substr(0, 8); if(own_name.empty()) own_name = "PLAYER";
  host = settings.values.get("Network host").value_or("");
  connect(); interface.named("_status_text").text = "Briefing";
  if(two_player) {
    for(auto const name : {"_direct_comms_button", "_modem_comms_button", "_com_port_box", "_port_interrupt_box", "_baud_rate_box", "_modem_type_box", "_modem_type_selection_box", "_modem_phone_dial_type_box", "_modem_answer_type_box", "_modem_command_box", "_modem_dial_or_hangup_box", "_modem_phone_book_box", "_modem_screen_graphic2", "_modem_screen_graphic3"}) visible(name, false);
    visible("_network_channel_box", true); visible("_modem_phone_number_box", true);
    interface.named("_network_comms_button").on = true;
    for(auto &w : interface.objects()) if(w.text == "Phone No") w.text = "Host";
    interface.named("_modem_phone_number_button").text = host.empty() ? "Automatic discovery" : host;
    for(auto &w : interface.objects()) if(w.action == "notify_network_channel_") w.on = w.text == "1";
    message("Looking for another player"); interface.show("_modem_screen");
  } else interface.show("_network_screen");
}
void network_menu::close() {
  if(peers) { peers->leave(); peers.reset(); }
  start_requested = starting = editing_name = editing_host = false; buttons(true);
}
void network_menu::announce_selection(unsigned int const code, int const value) {
  if(peers) peers->send(multiplayer::menu_selection(peers->state.local, peers->room.own_name, code, {0, -1, value, 0, 0}));
}
void network_menu::selection(unsigned int const code, std::array<int32_t, 5> const &arguments) {
  if(!peers) return;
  auto &p{peers->room.two_player};
  if(code == 1) { auto const previous{arguments[2]}; p[0] = previous == 1 ? 3 : previous == 2 ? 1 : 2; } else if(code >= 2 && code <= 5) p[2] = static_cast<int32_t>(code - 2);
  else if(code == 6 || code == 7) p[3] = static_cast<int32_t>(code - 6);
  else if(code == 8 || code == 9) p[4] = static_cast<int32_t>(code - 8);
  else if(code >= 13 && code <= 18 && mission_selection) mission_selection(code, arguments[2]);
  controls();
}
bool network_menu::activate(widget &object) {
  if(!peers) return false;
  auto const &action{object.action}; auto &room{peers->room};
  if(action == "notify_enter_name_button_") { editing_name = true; entry = room.own_name; object.on = true; } else if(action == "notify_two_player_phone_number_entry_") { editing_host = true; entry = host; } else if(action == "notify_network_channel_") { channel = static_cast<unsigned int>(std::stoi(object.text) - 1); auto const values{room.two_player}; connect(); peers->room.two_player = values; controls(); message("Looking for another player"); } else if(action == "notify_two_player_connection_type_") { interface.named("_network_comms_button").on = true; } else if(action == "notify_two_player_game_mode_") { auto const old{room.two_player[0]}; announce_selection(1, old); selection(1, {0, -1, old, 0, 0}); } else if(action == "notify_two_player_position_") { room.two_player[1] = &object == &interface.named("_blue_leader_button"); announce_selection(room.two_player[1] ? 11 : 10); controls(); } else if(action == "notify_two_player_weather_") {
    auto const index{static_cast<unsigned int>(&object - &interface.named("_weather_buttons"))}; announce_selection(2 + index); selection(2 + index, {});
  } else if(action == "notify_two_player_start_time_") { auto const code{&object == &interface.named("_day_and_night_button") ? 7u : 6u}; announce_selection(code); selection(code, {}); } else if(action == "notify_two_player_difficulty_") { auto const code{&object == &interface.named("_crashing_button") ? 9u : 8u}; announce_selection(code); selection(code, {}); } else if(action == "notify_weapons_button_" || action == "notify_fuel_button_" || action == "notify_collision_button_" || action == "notify_game_type_button_") {
    auto &p{room.preferences};
    if(action == "notify_fuel_button_") p[0] = &object == &interface.named("_cheat_infinite_fuel_button");
    else if(action == "notify_weapons_button_") p[1] = &object == &interface.named("_cheat_unlimited_weapons_button");
    else if(action == "notify_collision_button_") p[3] = &object == &interface.named("_cheat_collision_button");
    else p[4] = &object == &interface.named("_cheat_deathmatch_button");
    for(auto const &[key, value] : std::array<std::pair<std::string_view, int>, 4>{{{"Network infinite fuel", p[0]}, {"Network unlimited weapons", p[1]}, {"Network collisions", p[3]}, {"Network deathmatch", p[4]}}}) settings.set(key, value);
    peers->send(multiplayer::preferences(peers->state.local, p)); preferences();
  } else if(action == "notify_network_takeoff_button_" || action == "notify_two_player_start_mission_") {
    if(!room.other_players) { if(two_player) message("You must have one other player"); return true; }
    if(two_player && room.other_players != 1) { message("Game already running. Select another channel"); return true; }
    if(two_player && room.rank == room.remote_rank) { message("Must select different ranks"); return true; }
    peers->ready();
  } else return false;
  return true;
}
bool network_menu::key(unsigned int const c) {
  if(!editing_name && !editing_host) return false;
  if(c == 8) { if(!entry.empty()) entry.pop_back(); } else if(c == 27) entry.clear();
  else if(c == 10 || c == 13) {
    if(editing_name) { own_name = entry; peers->name(entry); settings.values.set("Network name", entry); interface.named("_enter_name_button").on = false; } else { host = entry; auto const values{peers->room.two_player}; connect(); peers->room.two_player = values; controls(); settings.values.set("Network host", host); }
    editing_name = editing_host = false; settings.save();
  } else if(c >= 32 && c < 127 && entry.size() < (editing_name ? 8u : 80u)) entry += static_cast<char>(c);
  if(editing_name) interface.named("_player_name").text = entry;
  if(editing_host) interface.named("_modem_phone_number_button").text = entry;
  return true;
}
void network_menu::poll(uint32_t const ticks) {
  if(!peers) return;
  peers->poll(ticks);
  if(!peers->room.active) return;
  if(start_requested) { start_requested = false; starting = true; if(start) start(); return; }
  if(!starting && !peers->state.active && peers->all_ready()) {
    if(two_player && (peers->room.other_players != 1 || peers->room.rank == peers->room.remote_rank)) return;
    starting = true; if(start) start();
  }
}
mission_connection network_menu::connection() const {
  if(!peers) throw std::runtime_error{"Network lobby is not open"};
  mission_connection result; auto const &room{peers->room}; result.local = peers->state.local; result.server = peers->state.server; result.players = peers->state.players; result.defer_initialisation = true;
  result.game = {room.mode, room.two_player[1] != 0, room.two_player[3] != 0, room.two_player[4] != 0, room.two_player[2], room.preferences[0] != 0, room.preferences[1] != 0, room.preferences[3] != 0};
  return result;
}
void network_menu::completed(simulation::mission_network const &network) {
  if(!peers) return;
  for(size_t i{0}; i < 16; ++i) peers->state.players[i].score = network.state.players[i].score;
  auto const winner{network.flag_winner};
  std::string const winner_name{winner ? network.state.players[*winner].name : ""};
  peers->resume(); peers->game_packet = {}; starting = false; interface.named("_status_text").text = "Debrief"; refresh();
  if(winner) message(winner_name + " captured the flag");
}
} // namespace apache::frontend
