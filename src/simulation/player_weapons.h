#pragma once
#include <functional>
#include <optional>
#include "math/geometry.h"
#include "resources/executable.h"
#include "weapon_info.h"

namespace apache::simulation {
struct player_pylon {
  unsigned int weapon{0};
  int32_t ammunition{0};
};
struct player_weapon_context {
  int32_t now{0};
  int32_t control_mode{2};
  int32_t game_mode{0};
  double frame_time{0.05};
  bool dead{false};
  bool landed{false};
  bool active{true};
  bool ground_radar{false};
  bool air_radar{false};
  unsigned int target_kind{0};
  uint32_t faults{0};
};
struct player_shot {
  unsigned int weapon;
  math::pose pose;
  bool guided;
  bool laser;
  bool untracked;
};
struct player_weapon_callbacks {
  std::function<bool(uint16_t)> key_event;
  std::function<void(unsigned int)> cockpit_sound;
  std::function<void(unsigned int)> launch_sound;
  std::function<void()> clear_impact_times;
  std::function<void()> cycle_target;
  std::function<void()> fired_at;
  std::function<math::pose(unsigned int, size_t)> launch_pose;
  std::function<std::optional<size_t>(player_shot const &)> launch;
  std::function<void(size_t)> missile_launched;
  std::function<void(size_t)> impact_time;
  std::function<void(size_t)> remote_fire;
  std::function<void(size_t, int32_t)> remove_missile;
};
class player_weapons {
  std::array<std::array<unsigned int, 3>, 9> next_list{};
  std::array<weapon_info, 55> definitions{weapon_definitions()};
  bool accepts(uint16_t key, player_weapon_context const &context) const;
  void announce() const;
public:
  std::array<player_pylon, 9> pylons{};
  std::array<int32_t, 9> ammunition{};
  std::array<int32_t, 9> selected{};
  std::array<int32_t, 9> deadlines{};
  unsigned int current{1};
  unsigned int helmet_mode{0};
  bool armed{false};
  bool infinite{false};
  bool automatic_chaff{true};
  bool gun_fired{false};
  player_weapon_callbacks events;
  explicit player_weapons(resources::executable_data const &executable);
  void rearm();
  bool available(unsigned int weapon, int32_t now) const;
  bool next(player_weapon_context const &context);
  bool select(uint16_t key, player_weapon_context const &context);
  bool toggle_countermeasures(uint16_t key, player_weapon_context const &context);
  bool fire(uint16_t key, player_weapon_context const &context);
};
} // namespace apache::simulation
