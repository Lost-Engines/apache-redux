#pragma once
#include <array>
#include <optional>
#include "platform/datagram.h"

namespace apache::multiplayer {
struct connection {
  uint64_t node{0};
  platform::network_address address;
  uint32_t last_seen{0};
};
// A local identity plus at most fifteen peers, as in the original IPX table.
// Lobby sorting is deterministic. Slot numbers remain fixed during flight.
class connections {
  uint64_t own_node;
  void reorder();
public:
  explicit connections(uint64_t local_node, platform::network_address address);
  std::array<connection, 16> entries{};
  bool sorted{true};
  std::optional<uint32_t> find(uint64_t node) const;
  uint32_t local() const;
  size_t other_count() const;
  std::optional<uint32_t> observe(uint64_t node, platform::network_address address, uint32_t now);
  bool remove(uint64_t node);
  bool expire(uint32_t now, uint32_t age = 4000);
};
} // namespace apache::multiplayer
