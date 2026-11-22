#pragma once
#include "firing_solution.h"
#include "targets.h"

namespace apache::simulation {
struct weapon_sight_inputs {
  unsigned int weapon{1}, weather{0};
  bool armed{false}, hydras_locked{false};
  int32_t cloud_bottom{6400000}, cloud_top{6912000};
  math::pose body;
  target_reference target;
  math::vector3 target_position{};
  int32_t target_classification{0};
};
struct weapon_sight_callbacks {
  std::function<bool()> visible;
  std::function<unsigned int()> position_gun;
  std::function<void(unsigned int)> cockpit_sound;
};
class player_weapon_sight {
  math::trigonometry const &trig;
  std::array<weapon_info, 55> definitions{weapon_definitions()};
public:
  unsigned int status{0};
  target_reference previous;
  explicit player_weapon_sight(math::trigonometry const &angles) : trig{angles} {}
  unsigned int evaluate(weapon_sight_inputs const &input, weapon_sight_callbacks const &events);
};
struct missile_impact_time { size_t round; int32_t deadline; };
class player_impact_times {
  std::array<weapon_info, 55> definitions{weapon_definitions()};
public:
  std::vector<missile_impact_time> entries;
  void clear() {
    entries.clear();
  }
  void launch(size_t round, unsigned int weapon, math::vector3 const &position, std::optional<math::vector3> const &target, int32_t now);
  void remove(size_t round, unsigned int weapon);
  int32_t current(int32_t now) const;
};
} // namespace apache::simulation
