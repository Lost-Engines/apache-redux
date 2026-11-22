#pragma once
#include "platform/datagram.h"
#include "packet.h"

namespace apache::multiplayer {
struct transport_options {
  platform::network_address bind{0, 0x1235};
  bool broadcast{true};
  std::vector<platform::network_address> peers;
  uint64_t node{0};                                                            // Zero selects a fresh node identity.
};
struct received_packet {
  uint64_t node;
  platform::network_address source;
  packet message;
};
// APNET payloads over IPv4 UDP. No socket exists until a network game is opened.
// The wrapper distinguishes this port from the original IPX service.
class transport {
  platform::datagram_socket socket;
  transport_options options;
  resources::bytes encode(packet const &message) const;
public:
  explicit transport(transport_options options = {});
  uint64_t node() const {
    return options.node;
  }
  platform::network_address address() const {
    return socket.address();
  }
  bool send(platform::network_address destination, packet const &message);
  void discover(packet const &message);
  // Bounded per call so unrelated traffic cannot stall input/rendering.
  std::vector<received_packet> poll();
};
} // namespace apache::multiplayer
