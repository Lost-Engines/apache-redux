#include <algorithm>
#include "mission_frontend.h"

namespace apache::frontend {
void mission_frontend::network_error(std::string const &message) {
  leave_network();
  interface.popup({{message, 0}, {"OK", 0}}, [](int){});
}
bool mission_frontend::text_input(unsigned int const value) {
  try {
    return network.key(value) || scores.key(value) || pilots.key(value);
  }
  catch(std::exception const &error) {
    network_error(error.what());
    return true;
  }
}
void mission_frontend::network_controls() {
  bool const remote{network.selecting() && network.remote_controlled()};
  for(auto &w : interface.objects()) {
    auto const &action{w.action};
    if(action == "notify_map_yemen_button_" || action == "notify_map_korea_button_" || action == "notify_map_cyprus_button_" || action == "notify_mission_selection_list_" || action == "notify_mission_selection_load_" || action == "notify_takeoff_button_") w.flags = (w.flags & ~0x8000u) | (remote ? 0u : 0x8000u);
  }
  visible("_world_remote_controlled_box", remote);
  visible("_mission_remote_controlled_box", remote);
  visible("_map_remote_controlled_box", remote);
  visible("_map_tool_box", !network.selecting());
}
void mission_frontend::start_network() {
  if(!network.peers) return;
  if(network.peers->room.other_players >= 8) {
    network_error("This mission supports at most eight Apache players");
    return;
  }
  if(network.connection().game.mode == multiplayer::game_mode::capture_the_flag) {
    auto const bytes{data.read("MISSDATA/CAPTURE.DTA")};
    auto const definition{resources::read_mission_definition(std::string{bytes.begin(), bytes.end()})};
    bool const complete{definition.tasks.size() >= 4 && !definition.tasks[2].actors.empty()
      && definition.tasks[2].actors[0].waypoints.size() >= 16
      && std::ranges::any_of(definition.tasks[3].actors, [](auto const &actor){return actor.model == 994;})};
    if(!complete) {
      leave_network();
      interface.popup({{"This copy of CAPTURE.DTA is incomplete", 0}, {"Its flag and respawn points are missing", 0}, {"Use the mission file from the US or Japanese CD", 0}, {"OK", 0}}, [](int){});
      return;
    }
  }
  network.peers->freeze();
  auto choice{options};
  choice.connection = network.connection();
  auto const mode{choice.connection->game.mode};
  result.reset();
  if(mode == multiplayer::game_mode::combat || mode == multiplayer::game_mode::multiplayer || mode == multiplayer::game_mode::capture_the_flag) {
    resources::mission_briefing briefing;
    briefing.zone = zone = 2;
    briefing.flow = flow = mode == multiplayer::game_mode::capture_the_flag ? 6 : 5;
    briefing.title = flow == 6 ? "Capture the Flag" : "Combat";
    prepared = std::make_unique<mission_session>(data, std::move(briefing), choice);
    departing = true;
  } else {
    flow = 3;
    interface.show("_world_screen");
    network_controls();
  }
}
void mission_frontend::receive_selection(unsigned int const code, int const value) {
  if(!network.selecting()) return;
  if(code >= 13 && code <= 15) {
    zone = static_cast<int>(code - 12);
    preferences.zone = zone;
    list();
  } else if(code == 17) {
    select(value);
  } else if(code == 16 && selected >= 0) {
    prepare(definitions[listed[static_cast<size_t>(selected)]].id);
  } else if(code == 18) departing = prepared != nullptr;
}
void mission_frontend::leave_network() {
  if(network.peers) network.send_selection(19);
  network.close();
  planner.close();
  prepared.reset();
  result.reset();
  departing = false;
  network_controls();
  interface.show("_flight_screen");
}
void mission_frontend::poll_network(uint32_t const ticks) {
  if(!network.peers) return;
  try {
    network.poll(ticks);
  }
  catch(std::exception const &error) {
    network_error(error.what());
    return;
  }
  if(network.peers && !network.peers->room.active) {
    leave_network();
    interface.popup({{"Other player left the game", 0}, {"OK", 0}}, [](int){});
  }
}
} // namespace apache::frontend
