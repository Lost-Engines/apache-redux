#include "connections.h"
#include <algorithm>
#include <stdexcept>

namespace apache::multiplayer {
connections::connections(uint64_t const local_node, platform::network_address const address) : own_node{local_node} {
  if(!local_node) throw std::invalid_argument{"A network node cannot be zero"};
  entries[0] = {local_node, address, 0};
}
void connections::reorder() {
  if(sorted) std::ranges::sort(entries, [](connection const &a, connection const &b){ return a.node && (!b.node || a.node < b.node); });
}
std::optional<uint32_t> connections::find(uint64_t const node) const {
  if(!node) return {};
  for(uint32_t i{0}; i < entries.size(); ++i) if(entries[i].node == node) return i;
  return {};
}
uint32_t connections::local() const {
  return *find(own_node);
}
size_t connections::other_count() const {
  return static_cast<size_t>(std::ranges::count_if(entries, [](auto const &value){ return value.node != 0; })) - 1;
}
std::optional<uint32_t> connections::observe(uint64_t const node, platform::network_address const address, uint32_t const now) {
  if(!node || !address.port) return {};
  if(auto const existing{find(node)}) {
    auto &entry{entries[*existing]};
    // An active slot is bound to its endpoint until the player leaves.
    if(node != own_node && (sorted || entry.address == address)) { entry.address = address; entry.last_seen = now; } else if(node != own_node) return {};
    reorder(); return find(node);
  }
  for(auto &entry : entries) if(!entry.node) {
    entry = {node, address, now}; reorder(); return find(node);
  }
  return {};
}
bool connections::remove(uint64_t const node) {
  if(node == own_node) return false;
  if(auto const found{find(node)}) { entries[*found] = {}; reorder(); return true; }
  return false;
}
bool connections::expire(uint32_t const now, uint32_t const age) {
  if(!sorted) return false;
  bool changed{false};
  for(auto &entry : entries) if(entry.node && entry.node != own_node && now - entry.last_seen > age) { entry = {}; changed = true; }
  if(changed) reorder();
  return changed;
}
} // namespace apache::multiplayer
