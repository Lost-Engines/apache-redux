#pragma once
#include "mission_runtime.h"
#include "multiplayer/casualties.h"
#include "multiplayer/lifecycle.h"
#include "multiplayer/replication.h"
#include "multiplayer/rules.h"

namespace apache::simulation {
// Connects APNET state to the mission's existing render, collision and target objects.
class mission_network {
  mission_runtime &runtime;
  std::array<std::optional<size_t>, 16> assignments;
  void replace_model(size_t actor_index, uint32_t model, unsigned int map);
  void send_player_kill(uint32_t victim);
public:
  multiplayer::replication state;
  multiplayer::lifecycle lifecycle;
  multiplayer::casualties casualties;
  std::function<void(target_identity const &, uint8_t)> remote_killed;
  std::function<void(multiplayer::packet const &)> send;
  bool enabled{false};
  std::optional<uint32_t> flag_winner;
  explicit mission_network(mission_runtime &mission);
  std::optional<size_t> actor(uint32_t connection) const;
  bool bind(uint32_t connection, size_t actor_index);
  bool assign(uint32_t connection, uint32_t model, size_t task, multiplayer::game_mode mode, int flow, unsigned int map);
  void remove_unassigned();
  void record_kill(target_identity const &target, target_identity const &source);
  void player_killed(target_identity const &source);
  void send_casualties();
  bool revive(uint32_t connection, int32_t model);

  std::optional<target_reference> resolve(multiplayer::target_state const &target) const;
  multiplayer::target_state identify(target_reference const &target) const;
  bool receive(multiplayer::packet const &message, int32_t now);
  void update();
};
} // namespace apache::simulation
