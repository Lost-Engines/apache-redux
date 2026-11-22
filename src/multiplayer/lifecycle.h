#pragma once
#include "replication.h"
#include "rules.h"

namespace apache::multiplayer {
struct actor_link {
  size_t id{0};
  int32_t destroyed{0};
  bool local{false};
};
struct lifecycle_events {
  std::function<std::optional<actor_link>(uint32_t)> player;
  std::function<std::optional<actor_link>(int32_t, int32_t)> find;
  std::function<void(size_t)> remove;
  std::function<void(size_t)> leave;
  std::function<void(size_t, math::pose const &, int32_t)> restart;
  std::function<void(size_t, bool)> countermeasure;
  std::function<void(unsigned int)> death;
  std::function<void()> player_killed;
  std::function<void(size_t, uint8_t)> mobile_killed;
  std::function<void(math::vector3 const &, uint8_t)> scenery_killed;
  std::function<void(uint32_t)> flag_won;
  std::function<void(std::string_view, int32_t, int32_t)> notice;
  std::function<void()> scores;
  std::function<void(size_t)> unplace;
  std::function<void(uint32_t)> unregister;
  std::function<uint32_t(uint32_t)> next_server;
};
// APNET's in-flight events. Scene identities and effects belong to the mission.
class lifecycle {
  replication &state;
  void notice(std::string const &text, int32_t priority, int32_t lifetime) const;
  void lost_partner();
  void remove(std::optional<actor_link> actor) const;
public:
  lifecycle_events events;
  game_mode mode{game_mode::single_player};
  int32_t other_players{0};
  bool following{false};
  explicit lifecycle(replication &replicas) : state{replicas} {}
  bool receive(packet const &message, int32_t now = 0);
};
} // namespace apache::multiplayer
