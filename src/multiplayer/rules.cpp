#include "rules.h"
#include <stdexcept>

namespace apache::multiplayer {
mission_rules apply_rules(game_settings const &settings, mission_rules result) {
  result.client_server = true;
  result.infinite_weapons = false;
  result.radio = 100;
  switch(settings.mode) {
  case game_mode::combat:
    result.control = 2;
    result.forces = 9;
    result.rank = settings.second_seat ? 2 : 1;
    result.wind_strength = result.wind_direction = 0;
    result.weather = settings.weather;
    result.time_of_day = settings.night ? 14760000 : 8640000;
    result.no_crashes = !settings.difficult;
    break;
  case game_mode::single_missions:
  case game_mode::apache_vs_hind:
    result.control = 2;
    result.forces = 9;
    result.rank = settings.second_seat ? 2 : 1;
    result.client_server = false;
    result.radio = settings.mode == game_mode::single_missions ? 2 : 100;
    result.infinite_fuel = result.infinite_weapons = result.no_crashes = false;
    break;
  case game_mode::pilot_gunner:
    result.forces = 2;
    result.rank = settings.second_seat ? 2 : 1;
    result.radio = result.control = settings.second_seat ? 1 : 0;
    result.client_server = false;
    result.infinite_fuel = result.infinite_weapons = result.no_crashes = false;
    break;
  case game_mode::multiplayer:
  case game_mode::capture_the_flag:
    result.control = 2;
    result.rank = 1;
    result.forces = 9;
    result.wind_strength = result.wind_direction = result.weather = 0;
    result.time_of_day = 8640000;
    result.no_crashes = !settings.crashes;
    result.infinite_fuel = settings.infinite_fuel;
    result.infinite_weapons = settings.infinite_weapons;
    break;
  default:
    throw std::invalid_argument{"Expected a multiplayer game mode"};
  }
  return result;
}
} // namespace apache::multiplayer
