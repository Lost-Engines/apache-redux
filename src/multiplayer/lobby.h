#pragma once
#include "replication.h"
#include "rules.h"

namespace apache::multiplayer {
struct lobby_callbacks {
  std::function<void(uint32_t, std::string_view)> connection_name;
  std::function<void(uint32_t)> identify;
  std::function<void()> announce;
  std::function<void()> refresh;
  std::function<void()> recount;
  std::function<void()> preferences_changed;
  std::function<void()> reset_connection;
  std::function<void()> settings_changed;
  std::function<void()> start;
  std::function<void(bool)> buttons;
  std::function<void(unsigned int, std::array<int32_t, 5> const &)> selection;
  std::function<void(std::string_view)> message;
  std::function<void(std::string_view)> chat;
  std::function<void(uint32_t)> cd_reply;
};
// Menu-side branches of apnet_receive_. Scene-side branches live in replication
// and lifecycle; platform connection discovery supplies the transport slot.
class lobby {
  replication &state;
  void named(uint32_t sender, std::string const &name);
public:
  explicit lobby(replication &state) : state{state} {}
  game_mode mode{game_mode::multiplayer};
  int32_t flow{5};
  int32_t other_players{0};
  int32_t remote_rank{0};
  int32_t second_product{0};
  int32_t selected_mission{0};
  int32_t rank{1};
  bool active{true};
  bool cd_available{true};
  bool cd_found{false};
  std::array<int32_t, 5> preferences{0, 0, 0, 1, 1};
  std::array<int32_t, 5> two_player{1, 0, 0, 0, 0};
  std::string own_name;
  std::string last_message;
  lobby_callbacks events;
  bool receive(packet const &message, uint32_t transport_slot);
};
} // namespace apache::multiplayer
