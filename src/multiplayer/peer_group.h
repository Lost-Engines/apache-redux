#pragma once
#include "connections.h"
#include "lobby.h"
#include "transport.h"

namespace apache::multiplayer {
// Owns LAN discovery and the lobby roster. Gameplay keeps using the same
// connection slots after freeze(), and supplies its existing APNET receiver.
class peer_group {
  transport wire;
  connections roster;
  uint32_t next_discovery{0};
  bool discovered{false};
  void reorder(std::array<connection, 16> const &previous);
  void announce();
  void refresh();
public:
  replication state;
  lobby room;
  std::function<void()> changed;
  std::function<void(packet const &, int32_t)> game_packet;
  peer_group(transport_options options, std::string name);
  peer_group(peer_group const &) = delete;
  peer_group &operator=(peer_group const &) = delete;
  platform::network_address address() const {
    return wire.address();
  }
  connections const &members() const {
    return roster;
  }
  void poll(uint32_t now);
  void send(packet const &message);
  void send(uint32_t destination, packet const &message);
  void name(std::string value);
  void ready();
  bool all_ready() const;
  void freeze();
  void resume();
  void leave();
  void forget(uint32_t slot);
};
} // namespace apache::multiplayer
