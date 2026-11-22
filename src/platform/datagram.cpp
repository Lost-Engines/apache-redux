#include "datagram.h"
#include <array>
#include <charconv>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif // NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <cerrno>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#endif // _WIN32

namespace apache::platform {
namespace {
#ifdef _WIN32
using socket_type = SOCKET;
using socket_length = int;
socket_type constexpr invalid_socket{INVALID_SOCKET};
int last_error() {
  return WSAGetLastError();
}
bool would_block(int code) {
  return code == WSAEWOULDBLOCK || code == WSAEINTR;
}
void close(socket_type handle) {
  closesocket(handle);
}
struct socket_runtime {
  socket_runtime() {
    WSADATA data;
    if(WSAStartup(MAKEWORD(2, 2), &data)) throw std::runtime_error{"Cannot initialize network sockets"};
  }
  ~socket_runtime() {
    WSACleanup();
  }
};
#else
using socket_type = int;
using socket_length = socklen_t;
socket_type constexpr invalid_socket{-1};
int last_error() {
  return errno;
}
bool would_block(int code) {
  return code == EAGAIN || code == EWOULDBLOCK || code == EINTR;
}
void close(socket_type handle) {
  ::close(handle);
}
struct socket_runtime {};
#endif // _WIN32
sockaddr_in native(network_address value) {
  sockaddr_in result{};
  result.sin_family = AF_INET;
  result.sin_addr.s_addr = htonl(value.host);
  result.sin_port = htons(value.port);
  return result;
}
network_address portable(sockaddr_in const &value) {
  return {ntohl(value.sin_addr.s_addr), ntohs(value.sin_port)};
}
} // anonymous namespace
std::string network_address::text() const {
  return std::to_string(host >> 24) + "." + std::to_string((host >> 16) & 255) + "." + std::to_string((host >> 8) & 255) + "." + std::to_string(host & 255) + ":" + std::to_string(port);
}
network_address network_address::resolve(std::string const &text, uint16_t const default_port) {
  [[maybe_unused]] socket_runtime runtime;
  std::string host{text};
  unsigned int port{default_port};
  if(auto colon{host.find(':')}; colon != std::string::npos) {
    auto const tail{std::string_view{host}.substr(colon + 1)};
    auto const [end, error]{std::from_chars(tail.data(), tail.data() + tail.size(), port)};
    if(error != std::errc{} || end != tail.data() + tail.size() || port > 65535) throw std::invalid_argument{"Invalid network port"};
    host.resize(colon);
  }
  if(host.empty() || host == "*") return {0, static_cast<uint16_t>(port)};
  addrinfo hints{};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;
  addrinfo *result{nullptr};
  if(getaddrinfo(host.c_str(), nullptr, &hints, &result) || !result) throw std::runtime_error{"Cannot resolve network address: " + host};
  std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> owned{result, freeaddrinfo};
  return {ntohl(reinterpret_cast<sockaddr_in const *>(result->ai_addr)->sin_addr.s_addr), static_cast<uint16_t>(port)};
}
struct datagram_socket::implementation {
  [[no_unique_address]] socket_runtime runtime;
  socket_type handle{invalid_socket};
  network_address local;
  ~implementation() {
    if(handle != invalid_socket) close(handle);
  }
};
datagram_socket::datagram_socket(network_address const bind, bool const broadcast) : impl{std::make_unique<implementation>()} {
  impl->handle = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if(impl->handle == invalid_socket) throw std::runtime_error{"Cannot create UDP socket"};
#ifdef _WIN32
  u_long mode{1};
  if(ioctlsocket(impl->handle, FIONBIO, &mode)) throw std::runtime_error{"Cannot make UDP socket nonblocking"};
#else
  auto const flags{fcntl(impl->handle, F_GETFL, 0)};
  if(flags < 0 || fcntl(impl->handle, F_SETFL, flags | O_NONBLOCK) < 0 || fcntl(impl->handle, F_SETFD, FD_CLOEXEC) < 0) throw std::runtime_error{"Cannot make UDP socket nonblocking"};
#endif // _WIN32
  if(broadcast) {
    int enabled{1};
    if(setsockopt(impl->handle, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<char const *>(&enabled), sizeof enabled)) throw std::runtime_error{"Cannot enable LAN discovery"};
  }
  auto local{native(bind)};
  if(::bind(impl->handle, reinterpret_cast<sockaddr const *>(&local), sizeof local)) throw std::runtime_error{"Cannot bind UDP socket at " + bind.text()};
  socket_length size{sizeof local};
  if(getsockname(impl->handle, reinterpret_cast<sockaddr *>(&local), &size)) throw std::runtime_error{"Cannot read local UDP address"};
  impl->local = portable(local);
}
datagram_socket::~datagram_socket() = default;
network_address datagram_socket::address() const {
  return impl->local;
}
bool datagram_socket::send(network_address const destination, std::span<uint8_t const> const data) {
  if(data.empty() || data.size() > 2048 || !destination.port) throw std::invalid_argument{"Invalid UDP datagram"};
  auto const address{native(destination)};
  auto const result{sendto(impl->handle, reinterpret_cast<char const *>(data.data()), static_cast<int>(data.size()), 0, reinterpret_cast<sockaddr const *>(&address), sizeof address)};
  if(result < 0) {
    if(would_block(last_error())) return false;
    throw std::runtime_error{"Cannot send UDP datagram to " + destination.text()};
  }
  return static_cast<size_t>(result) == data.size();
}
std::optional<datagram> datagram_socket::receive() {
  std::array<uint8_t, 2049> data{};
  sockaddr_in from{};
  socket_length size{sizeof from};
  auto const count{recvfrom(impl->handle, reinterpret_cast<char *>(data.data()), static_cast<int>(data.size()), 0, reinterpret_cast<sockaddr *>(&from), &size)};
  if(count < 0) {
    auto const error{last_error()};
    if(would_block(error)) return {};
#ifdef _WIN32
    if(error == WSAEMSGSIZE || error == WSAECONNRESET) return datagram{portable(from), {}};
#else
    if(error == ECONNREFUSED) return datagram{portable(from), {}};
#endif // _WIN32
    throw std::runtime_error{"Cannot receive UDP datagram"};
  }
  if(count > 2048 || from.sin_family != AF_INET) return datagram{portable(from), {}};
  return datagram{portable(from), resources::bytes{data.begin(), data.begin() + count}};
}
} // namespace apache::platform
