#pragma once
#include "damage.h"
#include "mission_combat.h"
#include "mission_flight.h"

namespace apache::simulation {
struct player_damage_callbacks {
  std::function<void(damage_feedback const &)> feedback;
  std::function<void(bool)> impact_sound;
};
// Applies weapon damage to the same faults and engines used by flight and avionics.
class mission_player_damage {
  mission_flight &flight;
  mission_actors &actors;
  mission_spatial const &spatial;
  mission_targets const &targets;
  mission_combat &combat;
  resources::model_archive const &models;
  player_damage_callbacks events;
public:
  player_damage_state state;
  target_identity killer;
  mission_player_damage(mission_flight &flight, mission_actors &actors, mission_spatial const &spatial, mission_targets const &targets, mission_combat &combat, resources::model_archive const &models, player_damage_callbacks events = {});
  void apply(damage_action action, target_identity const &source, int32_t now);
};
} // namespace apache::simulation
