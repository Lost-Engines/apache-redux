#pragma once
#include <optional>
#include "math/geometry.h"
#include "resources/reader.h"

namespace apache::multiplayer {
enum class message_type : uint32_t {
  discover = 2,
  identity = 3,
  peers_changed = 4,
  name = 5,
  preferences = 6,
  menu_selection = 7,
  weather = 8,
  ready = 9,
  initial_state = 10,
  position = 12,
  fired = 13,
  killed_player = 16,
  died = 17,
  left = 18,
  disconnected = 19,
  restarted = 20,
  crashed = 21,
  collision = 22,
  chaff = 23,
  flare = 24,
  crew = 25,
  mobile_kills = 26,
  scenery_kills = 27,
  flag_won = 28,
  client_state = 29,
  server_state = 30,
  server_ready = 31,
  server_left = 32,
  client_left = 33,
  nominate_server = 34,
  server_waiting = 35,
  scores = 36,
  synchronise = 38,
  two_player_settings = 39,
  chat = 40,
  cd_question = 41,
  cd_reply = 42,
};
class packet {
  resources::bytes storage;
public:
  packet(message_type type, uint32_t sender, std::string_view name = {});
  static std::optional<packet> decode(std::span<uint8_t const> source);
  message_type type() const;
  uint32_t sender() const;
  std::string name() const;
  std::span<uint8_t const> bytes() const;
  uint32_t word(size_t offset) const;
  void word(size_t offset, uint32_t value);
  void copy(size_t offset, std::span<uint8_t const> source);
  void pose(math::pose const &value);
  math::pose pose() const;
};
struct aircraft_state {
  math::pose location;
  int32_t speed{0};
  int32_t model{0};
};
struct target_state {
  uint32_t kind{0};
  math::vector3 data{};
};
packet identification(message_type type, uint32_t sender, std::string_view name, int32_t model);
packet nomination(uint32_t sender, std::string_view name, double clock);
packet two_player_settings(std::array<int32_t, 5> const &values);
packet chat(uint32_t sender, std::string_view name, std::string_view text);
packet crew(uint32_t sender, std::string_view name, std::span<uint8_t const, 104> payload);
packet kills(uint32_t sender, std::string_view name, bool scenery, uint8_t count, std::span<uint8_t const> records);
packet flag_won(uint32_t sender, std::string_view name);
packet preferences(uint32_t sender, std::array<int32_t, 5> const &values);
packet weather(uint32_t sender, std::string_view name, int32_t strength, int32_t direction);
packet position(uint32_t sender, math::pose location, int32_t speed, target_state const &target);
packet restarted(uint32_t sender, std::string_view name, math::pose const &location);
packet client_state(uint32_t sender, math::pose const &location, int32_t speed, double frame_delta);
packet server_state(std::array<aircraft_state, 16> const &players);
std::array<aircraft_state, 16> server_state(packet const &message);
packet scores(uint32_t sender, std::string_view name, std::array<int32_t, 16> const &values);
packet fired(uint32_t sender, std::string_view name, uint32_t weapon, uint32_t shot_seed, target_state const &target, math::vector3 const &gun_direction);
packet event(message_type type, uint32_t sender, std::string_view name, int32_t first, int32_t second = 0);
packet menu_selection(uint32_t sender, std::string_view name, uint32_t selection, std::array<int32_t, 5> const &arguments);
packet initial_state(uint32_t sender, std::string_view name, math::vector3 const &position, uint32_t war_seed, uint32_t weapon_seed);
} // namespace apache::multiplayer
