#include "mission_player_damage.h"
#include <stdexcept>

namespace apache::simulation {
mission_player_damage::mission_player_damage(mission_flight &pilot, mission_actors &units, mission_spatial const &locations, mission_targets const &queries, mission_combat &decisions, resources::model_archive const &objects, player_damage_callbacks callbacks)
  : flight{pilot},
    actors{units},
    spatial{locations},
    targets{queries},
    combat{decisions},
    models{objects},
    events{std::move(callbacks)} {
  if(!spatial.player) throw std::runtime_error{"Player damage requires an active pilot"};
  state.faults = flight.faults();
}
void mission_player_damage::apply(damage_action const action, target_identity const &source, int32_t const now) {
  auto &attention{combat.attention.at(*spatial.player)};
  switch(action) {
  case damage_action::kill:
    if(flight.invulnerable()) return;
    flight.request_death(4);
    flight.collision_model = source.kind == target_kind::none ? 0 : targets.describe({source, {}, 0}).model;
    attention.damage = attention.processed_damage = state.processed_damage = 0;
    break;
  case damage_action::player_systems:
    {
    state.faults = flight.faults();
    state.invulnerable = flight.invulnerable();
    state.processed_damage = attention.processed_damage;
    damage_feedbacks feedback;
    damage_player_systems(state, attention.damage, models.at(actors.units[*spatial.player].object.id)->metadata[3], now, feedback);
    flight.set_faults(state.faults);
    attention.processed_damage = state.processed_damage;
    for(auto const &event : feedback) {
      if(events.feedback) events.feedback(event);
    }
    break;
  }
  case damage_action::metal_sound:
  case damage_action::explosion_sound:
    if(events.impact_sound) events.impact_sound(action == damage_action::explosion_sound);
    break;
  case damage_action::recorded_ground_killer:
  case damage_action::recorded_actor_killer:
    killer = source;
    break;
  }
}
} // namespace apache::simulation
