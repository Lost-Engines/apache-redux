#pragma once
#include "seeker.h"
#include "target_visibility.h"

namespace apache::simulation {
struct damage_source {
  target_kind kind{target_kind::none};
  int32_t side{0};
  int32_t network{-1};
  bool actor{false};
};
struct damage_recipient {
  uint32_t model{0};
  uint32_t flags{0};
  bool mobile{false};
  bool actor{false};
  bool player{false};
  int32_t network{-1};
  uint8_t damage{0};
  math::vector3 position{};
};
struct blast_limits {
  int32_t damage{0};
  int32_t radius{0};
};
struct damage_random {
  uint32_t weapon{1};
  uint32_t round{1};
  uint32_t global{1};
};
struct damage_environment {
  int32_t mode{0};
  int32_t flow{0};
  int32_t difficulty_percent{100};
  int32_t decoy_modifier{1};
};
enum class damage_action {
  kill,
  player_systems,
  metal_sound,
  explosion_sound,
  recorded_ground_killer,
  recorded_actor_killer
};
struct damage_result {
  int32_t amount{0};
  std::vector<damage_action> actions;
};
damage_result apply_damage(damage_recipient &target, resources::model const &model, damage_source const &source, unsigned int weapon_id, weapon_info const &weapon, math::vector3 const &impact, blast_limits &blast, damage_random &random, damage_environment const &environment);

struct player_damage_state {
  uint32_t faults{0};
  uint32_t random{1};
  int32_t broken_mfds{0};
  int32_t warning_at{0};
  uint8_t processed_damage{0};
  bool invulnerable{false};
  bool ground_radar{false};
  bool air_radar{false};
};
enum class damage_feedback_kind {
  message,
  cockpit_sound,
  radar_toggle
};
struct damage_feedback {
  damage_feedback_kind kind;
  unsigned int value{0};
  std::string_view text;
};
using damage_feedbacks = std::vector<damage_feedback>;
void damage_player_item(player_damage_state &state, unsigned int item, damage_feedbacks &events);
void damage_player_systems(player_damage_state &state, uint8_t damage, int32_t resistance, int32_t now, damage_feedbacks &events);
} // namespace apache::simulation
