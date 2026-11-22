#pragma once
#include <functional>
#include "math/geometry.h"
#include "weapon_rules.h"

namespace apache::simulation {
struct countermeasure_actor {
  int32_t classification{0};
  bool player{false};
  math::vector3 position{};
  uint32_t conditions{0}, random{1};
  int32_t retreat_until{0}, chaff_at{0}, flare_at{0};
};
struct countermeasure_controls {
  int32_t mode{2};
  uint32_t faults{0};
  bool infinite{false}, automatic{false};
  // Chaff, flare: total remaining, selected pylon count and player release deadline.
  std::array<int32_t, 2> ammunition{}, pylon_ammunition{}, deadline{};
};
struct countermeasure_callbacks {
  std::function<bool(unsigned int)> launch;
  std::function<void(int32_t)> warning;
  std::function<bool(uint16_t)> key_event;
  std::function<void(unsigned int)> released;
};
int32_t countermeasure_delay(weapon_info const &weapon, forces_info const &skill, combat_random &random);
bool release_player_countermeasure(countermeasure_actor &actor, countermeasure_controls &controls, unsigned int weapon, uint16_t key, int32_t now, forces_info const &skill, uint32_t &random_seed, countermeasure_callbacks const &callbacks);
bool request_countermeasure(countermeasure_actor &actor, countermeasure_controls &controls, unsigned int weapon, int32_t now, forces_info const &skill, uint32_t &random_seed, countermeasure_callbacks const &callbacks);
bool notify_incoming_missile(countermeasure_actor &actor, countermeasure_controls &controls, weapon_info const &missile, math::vector3 const &position, int32_t speed, int32_t now, forces_info const &skill, uint32_t &random_seed, countermeasure_callbacks const &callbacks);
} // namespace apache::simulation
