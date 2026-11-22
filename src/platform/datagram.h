#pragma once
#include <compare>
#include <memory>
#include <optional>
#include <string>
#include "resources/reader.h"

namespace apache::platform {
struct network_address {
  uint32_t host{0};                                                            // IPv4 in host byte order.
  uint16_t port{0};
  auto operator<=>(network_address const &) const = default;
  std::string text() const;
  static network_address resolve(std::string const &text, uint16_t default_port);
};
struct datagram {
  network_address source;
  resources::bytes bytes;
};
class datagram_socket {
  struct implementation;
  std::unique_ptr<implementation> impl;
public:
  explicit datagram_socket(network_address bind, bool broadcast = false);
  ~datagram_socket();
  datagram_socket(datagram_socket const &) = delete;
  datagram_socket &operator=(datagram_socket const &) = delete;
  network_address address() const;
  bool send(network_address destination, std::span<uint8_t const> data);
  std::optional<datagram> receive();
};
} // namespace apache::platform
