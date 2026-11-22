#include "mission_player_weapons.h"
#include <algorithm>
#include <stdexcept>

namespace apache::simulation {
mission_player_weapons::mission_player_weapons(size_t const index, mission_actors &units, mission_spatial &locations, mission_targets &queries, mission_combat &decisions, mission_weapons &rounds, mission_flight &pilot, resources::model_archive const &objects, resources::executable_data const &executable, math::trigonometry const &trig, std::array<int32_t, 9> const &loadout, player_armament_events callbacks)
  : player{index}, actors{units}, spatial{locations}, targets{queries}, combat{decisions}, weapons{rounds}, flight{pilot}, models{objects}, events{std::move(callbacks)}, inventory{executable}, mounts{trig}, sight{trig} {
  combat.armaments[player].target = target; combat.player_target = target;
  inventory.events.cockpit_sound = [this](auto id){ if(events.cockpit_sound) events.cockpit_sound(id); };
  inventory.events.launch_sound = [this](auto id){ if(events.launch_sound) events.launch_sound(id); };
  inventory.events.clear_impact_times = [this]{ impacts.clear(); };
  inventory.events.cycle_target = [this]{ if(events.cycle_target) events.cycle_target(); };
  inventory.events.launch_pose = [this](auto id, auto slot){
    auto description{target->identity.kind != target_kind::none ? std::optional{targets.describe(*target).movement} : std::nullopt};
    if(id == 1) mounts.position_gun(weapons.environment.now, inventory.current, body(), description, aiming_direction());
    return mounts.launch_pose(id, slot, inventory.pylons[slot].ammunition, body());
  };
  inventory.events.launch = [this](auto const &shot){ return launch(shot); };
  inventory.events.missile_launched = [this](size_t round){ if(events.missile_camera) events.missile_camera(round); };
  inventory.events.impact_time = [this](size_t index){
    auto &round{weapons.rounds.at(index)}; auto const binding{selected_projectile_target(round, definitions.at(round.state.weapon), targets.world, [this](uint32_t id, target_slot slot){ return weapons.target_slot(id, slot); })};
    auto const *aim{binding.selected ? (round.selected ? &*round.selected : nullptr) : binding.target.get()};
    impacts.launch(index, round.state.weapon, round.state.body.pose.position, aim && aim->identity.kind != target_kind::none ? std::optional{targets.world.position(*aim)} : std::nullopt, weapons.environment.now);
  };
  inventory.events.fired_at = [this]{ combat.notify_fired_at(*target, spatial.targets[player]); };
  inventory.events.remove_missile = [this](size_t slot, int32_t ammo){ mounts.remove_missile(body(), slot, ammo); };
  rearm(loadout);
}
void mission_player_weapons::rearm(std::array<int32_t, 9> const &loadout) {
  for(size_t i{0}; i != 9; ++i) {
    if(loadout[i] < 0 || loadout[i] > 8) throw std::runtime_error{"Invalid player loadout"};
    inventory.pylons[i] = {static_cast<unsigned int>(loadout[i]), 0};
  }
  inventory.rearm(); *target = {}; mounts.hydra_elevation = 0;
  auto const pose{body().world}; auto const status{body().status}; auto const flags{body().flags};
  body() = render::construct_model(models, 103, pose, player_pod_models(inventory.pylons, models)); body().status = status; body().flags = flags; render::maintain_model(body());
  synchronize_countermeasures(false);
}
player_weapon_context mission_player_weapons::context(int32_t const now, double const dt) const {
  return {now, static_cast<int32_t>(control_mode), static_cast<int32_t>(game_mode), dt, flight.death() != 0, flight.landed(), active, ground_radar, air_radar, static_cast<unsigned int>(target->identity.kind), flight.faults()};
}
std::optional<size_t> mission_player_weapons::launch(player_shot const &shot) {
  projectile_request request; request.weapon = shot.weapon; request.source = *spatial.targets[player]; request.launch.pose = shot.pose; request.launch.source = targets.describe(request.source).movement;
  if(shot.guided && target->identity.kind != target_kind::none) request.selected = *target;
  if(shot.laser) request.designator = request.source;
  return weapons.launch_player(request, !shot.guided && shot.untracked);
}
bool mission_player_weapons::fire(int32_t const now, double const dt, uint16_t const key) {
  weapons.environment.now = now;
  return inventory.fire(key, context(now, dt));
}
bool mission_player_weapons::release_countermeasure(int32_t const now, unsigned int const weapon, uint16_t const key) {
  weapons.environment.now = now; synchronize_countermeasures(false);
  bool const released{weapons.release_countermeasure(weapon, key)};
  synchronize_countermeasures(true); return released;
}
bool mission_player_weapons::select(int32_t const now, uint16_t const key) {
  return inventory.select(key, context(now, 0));
}
void mission_player_weapons::prepare(int32_t const now, int32_t const delta, int32_t const hydra_input) {
  update_hydra(now, delta, hydra_input); evaluate_sight(now);
}
void mission_player_weapons::update_hydra(int32_t const now, int32_t const delta, int32_t const hydra_input) {
  std::optional<target_description> selected;
  if(target->identity.kind != target_kind::none) selected = targets.describe(*target);
  mounts.update_hydra(now, delta, hydra_input, body().world, selected ? std::optional{selected->movement.position} : std::nullopt, selected && selected->model < models.size() && models[selected->model] ? models[selected->model]->height : 0);
}
void mission_player_weapons::evaluate_sight(int32_t const now) {
  std::optional<target_description> selected;
  if(target->identity.kind != target_kind::none) selected = targets.describe(*target);
  weapon_sight_inputs input; input.weapon = inventory.current; input.weather = static_cast<unsigned int>(weapons.environment.weather); input.armed = inventory.armed; input.hydras_locked = mounts.hydras_locked; input.cloud_bottom = weapons.environment.cloud_bottom; input.cloud_top = weapons.environment.cloud_top; input.body = body().world; input.target = *target;
  if(selected) { input.target_position = selected->movement.position; input.target_classification = selected->movement.classification; }
  weapon_sight_callbacks callbacks;
  callbacks.visible = [this]{ return weapons.player_line_of_sight(*spatial.targets[player], *target); };
  callbacks.position_gun = [this, now, &selected]{ return mounts.position_gun(now, inventory.current, body(), selected ? std::optional{selected->movement} : std::nullopt, aiming_direction()); };
  callbacks.cockpit_sound = events.cockpit_sound; sight.evaluate(input, callbacks);
}
void mission_player_weapons::finish_update(int32_t const now) {
  if(!in_cockpit) mounts.position_gun(now, inventory.current, body(), target->identity.kind != target_kind::none ? std::optional{targets.describe(*target).movement} : std::nullopt, aiming_direction());
  warnings.present({events.incoming_sound, events.cockpit_sound, events.warning_message});
  if(gun_fired_last != inventory.gun_fired) {
    if(events.gun_sound) events.gun_sound(inventory.gun_fired);
    mounts.set_gun_flash(body(), models, inventory.gun_fired);
  }
  gun_fired_last = inventory.gun_fired; inventory.gun_fired = false;
}
void mission_player_weapons::synchronize_countermeasures(bool const receive) {
  auto &controls{weapons.player_countermeasures};
  if(receive) for(size_t j{0}; j != 2; ++j) {
    auto const id{j + 7}; inventory.ammunition[id] = controls.ammunition[j]; inventory.deadlines[id] = controls.deadline[j];
    auto const slot{inventory.selected[id]}; if(slot >= 0 && slot < 9) inventory.pylons[static_cast<size_t>(slot)].ammunition = controls.pylon_ammunition[j];
  }
  else {
    controls.mode = static_cast<int32_t>(control_mode); controls.faults = flight.faults(); controls.infinite = inventory.infinite; controls.automatic = inventory.automatic_chaff;
    for(size_t j{0}; j != 2; ++j) { auto const id{j + 7}; controls.ammunition[j] = inventory.ammunition[id]; controls.deadline[j] = inventory.deadlines[id]; auto const slot{inventory.selected[id]}; controls.pylon_ammunition[j] = slot >= 0 && slot < 9 ? inventory.pylons[static_cast<size_t>(slot)].ammunition : 0; }
  }
}
} // namespace apache::simulation
