#pragma once
#include <cstdint>

namespace apache::multiplayer {
enum class game_mode : unsigned int {
  single_player,
  combat,
  single_missions,
  pilot_gunner,
  multiplayer,
  capture_the_flag,
  apache_vs_hind
};
struct game_settings {
  game_mode mode{game_mode::combat};
  bool second_seat{false};
  bool night{false};
  bool difficult{true};
  int32_t weather{0};
  bool infinite_fuel{false};
  bool infinite_weapons{false};
  bool crashes{true};
};
struct mission_rules {
  int32_t control{2};
  int32_t rank{1};
  int32_t forces{9};
  int32_t radio{100};
  int32_t wind_strength{0};
  int32_t wind_direction{0};
  int32_t weather{0};
  int32_t time_of_day{8640000};
  bool client_server{true};
  bool no_crashes{false};
  bool infinite_fuel{false};
  bool infinite_weapons{false};
};
mission_rules apply_rules(game_settings const &settings, mission_rules mission = {});
} // namespace apache::multiplayer
