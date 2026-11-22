#include <algorithm>
#include <iostream>
#include <stdexcept>
#include "frontend/mission_frontend.h"
#include "frontend/mission_link.h"

namespace apache {
void inspect_network_live(resources::data_store const &data, unsigned int const mode, std::ostream &out, bool const expect_rejection) {
  auto require{[](bool condition, std::string const &message){ if(!condition) throw std::runtime_error{message}; }};
  require(mode >= 1 && mode <= 5, "Loopback mode must be 1 to 5");
  frontend::mission_options options; options.video_mode = 0; options.detail = 1; options.arcade = false;
  auto first{std::make_unique<frontend::mission_frontend>(data, options)}, second{std::make_unique<frontend::mission_frontend>(data, options)};
  auto &a{*first}; auto &b{*second};
  a.settings.values.set("Network name", "PILOT"); b.settings.values.set("Network name", "GUNNER");
  platform::network_address constexpr loopback{0x7f000001, 0};
  a.network.transport = {.bind{loopback}, .broadcast{false}, .peers{}, .node{100}};
  bool const two{mode <= 3}; a.interface.show("_flight_screen"); a.network.open(two, "PILOT");
  b.network.transport = {.bind{loopback}, .broadcast{false}, .peers{a.network.peers->address()}, .node{200}};
  b.interface.show("_flight_screen"); b.network.open(two, "GUNNER");
  render::framebuffer menu_frame{640,480}; int32_t clock{0}; unsigned int clicks{0};
  auto click{[&](frontend::mission_frontend &app, std::string_view name, int row = -1){
    app.interface.render(menu_frame); auto const area{app.interface.named(name).area};
    int const y{row < 0 ? (area.top + area.bottom) / 2 : area.top + row * static_cast<int>(app.interface.named(name).font_height + 1) + 2};
    app.interface.move((area.left + area.right) / 2, y); app.interface.button(true); app.interface.button(false); app.interface.move(-1,-1); ++clicks;
  }};
  auto pump{[&]{ for(int i{0}; i < 8; ++i) { a.poll_network(static_cast<uint32_t>(clock)); b.poll_network(static_cast<uint32_t>(clock)); } }};
  pump(); require(a.network.peers->room.other_players == 1 && b.network.peers->room.other_players == 1, "Lobby discovery");
  if(two) {
    click(b, "_blue_leader_button"); pump();
    for(unsigned int i{0}; i < (mode == 2 ? 2u : mode == 3 ? 1u : 0u); ++i) { click(a, "_two_player_game_mode_button"); pump(); }
    require(static_cast<unsigned int>(a.network.peers->room.mode) == mode && static_cast<unsigned int>(b.network.peers->room.mode) == mode, "Two-player mode selection");
    require(a.network.peers->room.rank == 1 && b.network.peers->room.rank == 2 && a.network.peers->room.remote_rank == 2 && b.network.peers->room.remote_rank == 1, "Complementary roles reach both lobbies");
  } else if(mode == 5) { click(a, "_cheat_capture_the_flag_button"); pump(); }
  require(static_cast<unsigned int>(a.network.peers->room.mode) == mode && static_cast<unsigned int>(b.network.peers->room.mode) == mode, "Game type reaches both lobbies");
  if(!two) {
    click(a, "_cheat_infinite_fuel_button"); click(a, "_cheat_unlimited_weapons_button"); click(a, "_cheat_no_collision_button"); pump();
    require(a.network.connection().game.infinite_fuel && b.network.connection().game.infinite_weapons && !b.network.connection().game.crashes, "Network preference radio buttons and transmission");
    click(a, "_cheat_limited_fuel_button"); click(a, "_cheat_limited_weapons_button"); click(a, "_cheat_collision_button"); pump();
  }
  click(a, two ? "_start_game_button" : "_network_takeoff_button"); pump();
  click(b, two ? "_start_game_button" : "_network_takeoff_button"); pump();
  if(expect_rejection) {
    require(!a.network.selecting() && !b.network.selecting() && !a.takeoff() && !b.takeoff(), "Incomplete capture mission is rejected before takeoff");
    require(a.interface.dismiss_popup() || b.interface.dismiss_popup(), "Missing mission data is explained in the menu");
    out << "{\"incomplete_capture_rejected\":true,\"transport\":\"IPv4 loopback, broadcast disabled\"}" << std::endl; return;
  }
  require(a.network.selecting() && b.network.selecting(), "Both players proceed from ready lobby");
  if(mode == 2 || mode == 3) {
    require(a.interface.current() == "_world_screen" && b.interface.current() == "_world_screen", "Both players see world selection");
    click(a, "_world_screen_yemen_button"); pump();
    require(a.interface.current() == "_mission_select_screen" && b.interface.current() == "_mission_select_screen", "World choice delivered");
    click(a, "_mission_selection_list", 0); pump();
    click(a, "_mission_selection_load_button"); pump();
    require(a.interface.current() == "_map_screen" && b.interface.current() == "_map_screen", "Mission briefing delivered");
    click(a, "_map_takeoff_button"); pump();
  }
  auto left{a.takeoff()}, right{b.takeoff()}; require(left && right, "Both missions launched");
  require(left->network && right->network, "Network mission contexts");
  require(static_cast<unsigned int>(left->network->lifecycle.mode) == mode && static_cast<unsigned int>(right->network->lifecycle.mode) == mode, "Requested mode reaches both flight runtimes");
  require(left->network->state.local == 0 && right->network->state.local == 1, "Stable aircraft connection slots");
  auto link_a{std::make_unique<frontend::mission_link>(*a.network.peers, *left, [&]{return clock;})};
  auto link_b{std::make_unique<frontend::mission_link>(*b.network.peers, *right, [&]{return clock;})};
  link_a->yield = [&]{ ++clock; link_b->poll(); return clock < 20000; };
  link_b->yield = [&]{ ++clock; link_a->poll(); return clock < 20000; };
  for(; clock < 2000 && (!link_a->ready() || !link_b->ready()); ++clock) { link_a->poll(); link_b->poll(); }
  require(link_a->ready() && link_b->ready(), "Initial APNET synchronization completes");
  require(!left->finished && !right->finished, "Both connections remain active");
  auto const start_clock{clock}; uint64_t hash{0};
  for(unsigned int frame{0}; frame < 40; ++frame) {
    clock += 10;
    if(frame == 3) { left->post(0x1c); right->post(0x1b); }
    if(frame == 6) { left->post(0x3d); right->post(0x3c); }
    if(frame == 15) left->post(0x4032);
    if(frame == 16) { left->post(0x23); left->post(0x17); }
    if(frame == 17) left->post(0x1c);
    if(frame == 20) { left->post(0x3b); right->post(0x19); }
    if(frame == 12) { left->post(0x2e); right->post(0x21); }
    left->step(10); right->step(10); left->draw(); right->draw();
    require(!left->finished && !right->finished, "Flight stays connected");
    require(clock < start_clock + 1000, "Frame synchronization does not time out");
    hash ^= resources::fingerprint(left->frame.image.pixels) ^ resources::fingerprint(right->frame.image.pixels);
  }
  require(left->chat && right->chat && !left->chat->writing, "Chat editor sends and closes");
  require(std::ranges::any_of(right->chat->messages, [](auto const &text){ return text.starts_with("> PILOT: hi"); }), "Chat arrives at the other aircraft");
  require(!left->help_requested && !right->paused, "Original network help and pause restrictions preserve synchronization");
  if(mode == 3) {
    require(left->crew && right->crew, "Pilot/gunner state exchange enabled");
    require(right->crew->next_frame_ticks == 10, "Pilot timing arrives in crew packet");
  } else {
    require(left->network->actor(1).has_value() && right->network->actor(0).has_value(), "Remote aircraft allocated");
    auto const &received{right->network->state.players[0].aircraft.location.position};
    auto const &sent{left->runtime.pilot->readings().position.position};
    for(size_t i{0}; i < 3; ++i) require(std::abs(static_cast<int64_t>(received[i]) - sent[i]) < 100000, "Received aircraft position tracks its peer");
  }
  link_a.reset(); link_b->poll();
  if(mode == 1 || mode >= 4) require(right->network->state.server == 1, "Server departure selects remaining player");
  if(mode == 3) require(right->finished || !right->network->state.active, "Shared cockpit ends when partner leaves");
  link_b.reset(); a.complete(std::move(left)); b.complete(std::move(right));
  require(!a.network.peers->state.active && !b.network.peers->state.active, "Debrief releases frozen connection roster");
  out << "{\"mode\":" << mode << ",\"frames\":80,\"menu_clicks\":" << clicks << ",\"initial_ticks\":" << start_clock << ",\"frame_hash\":" << hash << ",\"transport\":\"IPv4 loopback, broadcast disabled\"}" << std::endl;
}
} // namespace apache

namespace apache {
void inspect_network_group(resources::data_store const &data, unsigned int const count, std::ostream &out) {
  auto const require{[](bool condition, std::string_view message){ if(!condition) throw std::runtime_error{std::string{message}}; }};
  require(count >= 3 && count <= 8, "Group check requires 3 to 8 Apache players");
  std::vector<std::unique_ptr<multiplayer::peer_group>> peers;
  std::vector<std::unique_ptr<frontend::mission_session>> missions;
  std::vector<std::unique_ptr<frontend::mission_link>> links;
  int32_t clock{0};
  for(unsigned int i{0}; i < count; ++i) {
    multiplayer::transport_options wire{.bind{0x7f000001, 0}, .broadcast{false}, .peers{}, .node{100 + i}};
    for(auto const &other : peers) wire.peers.push_back(other->address());
    peers.push_back(std::make_unique<multiplayer::peer_group>(wire, "PLAYER" + std::to_string(i)));
  }
  for(unsigned int pass{0}; pass < count * 8; ++pass) for(auto &peer : peers) peer->poll(static_cast<uint32_t>(clock));
  for(unsigned int i{0}; i < count; ++i) { require(peers[i]->room.other_players == static_cast<int32_t>(count - 1), "Every peer discovers the full roster"); peers[i]->ready(); }
  for(unsigned int pass{0}; pass < count * 8; ++pass) for(auto &peer : peers) peer->poll(static_cast<uint32_t>(clock));
  for(unsigned int i{0}; i < count; ++i) {
    require(peers[i]->all_ready(), "Full group agrees on readiness");
    frontend::mission_options options; options.video_mode = 0; options.detail = 1; options.arcade = false;
    options.connection.emplace(); auto &connection{*options.connection}; connection.game.mode = multiplayer::game_mode::multiplayer;
    connection.local = peers[i]->state.local; connection.server = peers[i]->state.server; connection.players = peers[i]->state.players; connection.defer_initialisation = true;
    missions.push_back(std::make_unique<frontend::mission_session>(data, resources::mission_briefing{.flow{5}, .zone{2}}, options));
    links.push_back(std::make_unique<frontend::mission_link>(*peers[i], *missions[i], [&]{ return clock; }));
  }
  for(unsigned int i{0}; i < count; ++i) links[i]->yield = [&, i]{ ++clock; for(unsigned int j{0}; j < count; ++j) if(i != j && links[j]) links[j]->poll(); return clock < 20000; };
  auto pump{[&]{ for(auto &link : links) if(link) link->poll(); }};
  for(; clock < 2000; ++clock) { pump(); if(std::ranges::all_of(links, [](auto const &link){ return link->ready(); })) break; }
  require(clock < 2000, "Full group starts without timeout");
  auto const tick{[&](unsigned int first){ clock += 10; for(unsigned int i{first}; i < count; ++i) missions[i]->step(10); for(unsigned int i{first}; i < count; ++i) missions[i]->draw(); pump(); }};
  for(unsigned int frame{0}; frame < 6; ++frame) tick(0);
  // The host leaves. Every remaining client must nominate the same successor.
  links[0].reset(); for(unsigned int pass{0}; pass < 4; ++pass) pump();
  for(unsigned int i{1}; i < count; ++i) require(missions[i]->network->state.server == 1, "Group agrees on the new server");
  auto const before{missions[1]->network->state.client_frames};
  for(unsigned int frame{0}; frame < 6; ++frame) tick(1);
  auto const received{missions[1]->network->state.client_frames - before};
  for(unsigned int i{1}; i < count; ++i) require(!missions[i]->finished, "Remaining players stay in flight");
  out << "{\"players\":" << count << ",\"frames\":" << 6 * count + 6 * (count - 1) << ",\"handover_client_updates\":" << received << ",\"transport\":\"IPv4 loopback, broadcast disabled\"}" << std::endl;
  require(received > 0, "Client positions continue reaching the replacement server");
}
} // namespace apache
