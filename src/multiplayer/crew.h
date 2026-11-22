#pragma once
#include <optional>
#include <span>
#include <vector>
#include "math/geometry.h"

namespace apache::multiplayer {
enum class crew_role { pilot, gunner };
struct pilot_state {
  std::array<int32_t, 3> angles{};
  math::vector3 position{};
  double flight_time{0};
  int32_t climb{0}, fuel{0}, weight{0}, torque{0}, sidespeed{0}, airspeed{0};
  std::array<double, 8> engines{};
  uint8_t frame_ticks{0}, landing{0};
  bool landed{false};
};
struct gunner_state {
  int32_t hydra_elevation{0};
  math::vector3 display_position{}, position{};
  uint32_t target_kind{0}, target_source{0};
  int16_t drone{0}, task{0};
};
struct crew_state {
  pilot_state pilot;
  gunner_state gunner;
  bool leaving{false};
  uint8_t death{0}, message{0};
  std::vector<uint16_t> keys;
  // Original notify_two_player_key_event_: false means accepted/not shared;
  // true suppresses an action already queued, or a full ten-key queue.
  bool notify_key(uint16_t key, unsigned int game_mode = 3);
};
// Packing consumes one-shot messages, landing reports and key events.
std::array<uint8_t, 104> encode_crew(crew_role role, crew_state &state);
std::optional<crew_state> decode_crew(crew_role sender_role, std::span<uint8_t const> bytes);
} // namespace apache::multiplayer
