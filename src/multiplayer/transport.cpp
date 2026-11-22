#include "transport.h"
#include <algorithm>
#include <random>

namespace apache::multiplayer {
namespace {
std::array<uint8_t, 8> constexpr signature{'A', 'P', 'C', 'H', 'U', 'D', 'P', 1};
uint64_t fresh_node() {
  std::random_device random;
  auto const value{(static_cast<uint64_t>(random()) << 32) | static_cast<uint64_t>(random())};
  return value ? value : 1;
}
}
transport::transport(transport_options settings)
  : socket{settings.bind, settings.broadcast},
    options{std::move(settings)} {
  if(!options.node) options.node = fresh_node();
}
resources::bytes transport::encode(packet const &message) const {
  resources::bytes result{signature.begin(), signature.end()};
  for(unsigned int i{0}; i < 8; ++i) result.push_back(static_cast<uint8_t>(options.node >> (i * 8)));
  result.insert(result.end(), message.bytes().begin(), message.bytes().end());
  return result;
}
bool transport::send(platform::network_address const destination, packet const &message) {
  return socket.send(destination, encode(message));
}
void transport::discover(packet const &message) {
  if(options.broadcast) send({UINT32_MAX, socket.address().port}, message);
  for(auto const &peer : options.peers) send(peer, message);
}
std::vector<received_packet> transport::poll() {
  std::vector<received_packet> result;
  for(unsigned int i{0}; i < 64; ++i) {
    auto value{socket.receive()};
    if(!value) break;
    if(value->bytes.size() < 20 || !std::equal(signature.begin(), signature.end(), value->bytes.begin())) continue;
    uint64_t peer{0};
    for(unsigned int j{0}; j < 8; ++j) peer |= static_cast<uint64_t>(value->bytes[8 + j]) << (j * 8);
    if(!peer || peer == options.node) continue;
    if(auto message{packet::decode(std::span{value->bytes}.subspan(16))}) result.push_back({peer, value->source, std::move(*message)});
  }
  return result;
}
} // namespace apache::multiplayer
