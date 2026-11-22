#pragma once
#include "mission_flight.h"
#include "mission_weapons.h"
#include "player_sight.h"
#include "player_warnings.h"
#include "player_weapon_mounts.h"

namespace apache::simulation {
struct player_armament_events {
  std::function<void(unsigned int)> cockpit_sound;
  std::function<void(unsigned int)> launch_sound;
  std::function<void()> cycle_target;
  std::function<void()> incoming_sound;
  std::function<void(bool)> gun_sound;
  std::function<void(size_t, bool, std::string_view)> warning_message;
  std::function<void(size_t)> missile_camera;
};
class mission_player_weapons {
  size_t player;
  mission_actors &actors;
  mission_spatial &spatial;
  mission_targets &targets;
  mission_combat &combat;
  mission_weapons &weapons;
  mission_flight &flight;
  resources::model_archive const &models;
  player_armament_events events;
  std::array<weapon_info, 55> definitions{weapon_definitions()};
  render::model_instance &body() {
    return actors.units[player].object;
  }
  math::vector3 aiming_direction() const {
    return helmet_direction ? helmet_direction() : helmet;
  }
  player_weapon_context context(int32_t now, double dt) const;
  std::optional<size_t> launch(player_shot const &shot);
public:
  player_weapons inventory;
  player_weapon_mounts mounts;
  player_weapon_sight sight;
  player_impact_times impacts;
  player_warnings warnings;
  bool gun_fired_last{false};
  bool in_cockpit{true};
  shared_target target{std::make_shared<target_reference>()};
  math::vector3 helmet{0, INT32_MAX, 0};
  std::function<math::vector3()> helmet_direction;
  unsigned int control_mode{2};
  unsigned int game_mode{0};
  bool active{true};
  bool ground_radar{false};
  bool air_radar{false};
  mission_player_weapons(size_t player, mission_actors &actors, mission_spatial &spatial, mission_targets &targets, mission_combat &combat, mission_weapons &weapons, mission_flight &flight, resources::model_archive const &models, resources::executable_data const &executable, math::trigonometry const &trig, std::array<int32_t, 9> const &loadout, player_armament_events events = {});
  void rearm(std::array<int32_t, 9> const &loadout);
  void prepare(int32_t now, int32_t delta, int32_t hydra_input);
  void update_hydra(int32_t now, int32_t delta, int32_t hydra_input);
  void evaluate_sight(int32_t now);
  void finish_update(int32_t now);
  bool fire(int32_t now, double dt, uint16_t key);
  bool select(int32_t now, uint16_t key);
  bool release_countermeasure(int32_t now, unsigned int weapon, uint16_t key);
  bool toggle_countermeasures(uint16_t key) {
    return inventory.toggle_countermeasures(key, context(0, 0));
  }
  void synchronize_countermeasures(bool receive);
  void removed(size_t round, unsigned int weapon) {
    impacts.remove(round, weapon);
  }
};
} // namespace apache::simulation
