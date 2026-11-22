#pragma once
#include "configuration/settings.h"
#include "menu.h"
#include "mission_session.h"
#include "multiplayer/peer_group.h"

namespace apache::frontend {
class network_menu {
  menu &interface;
  configuration::settings &settings;
  bool start_requested{false};
  bool two_player{false};
  bool starting{false};
  bool editing_name{false};
  bool editing_host{false};
  unsigned int channel{0};
  std::string own_name;
  std::string host;
  std::string entry;
  std::array<std::string, 14> messages;
  void visible(std::string_view name, bool value);
  void connect();
  void refresh();
  void preferences();
  void controls();
  void buttons(bool enabled);
  void selection(unsigned int code, std::array<int32_t, 5> const &arguments);
  void announce_selection(unsigned int code, int value = 0);
public:
  multiplayer::transport_options transport;
  std::unique_ptr<multiplayer::peer_group> peers;
  std::function<void()> start;
  std::function<void(unsigned int, int)> mission_selection;
  network_menu(menu &interface, configuration::settings &settings);
  void open(bool two, std::string name);
  void close();
  void poll(uint32_t ticks);
  bool activate(widget &object);
  bool key(unsigned int character);
  void message(std::string_view text);
  mission_connection connection() const;
  void completed(simulation::mission_network const &network);
  bool selecting() const {
    return starting;
  }
  bool remote_controlled() const {
    return peers && two_player && peers->room.rank == 2;
  }
  void send_selection(unsigned int code, int value = 0) {
    announce_selection(code, value);
  }
};
} // namespace apache::frontend
