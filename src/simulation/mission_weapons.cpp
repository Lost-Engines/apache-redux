#include "mission_weapons.h"
#include "remote_weapons.h"
#include <algorithm>

namespace apache::simulation {
mission_weapons::mission_weapons(resources::mission_definition const &definition, mission_actors &units, mission_spatial &locations, mission_targets &queries, mission_combat &decisions, resources::model_archive const &objects, resources::terrain const &land, scenery_collisions const &structures, render::mobile_scene &scene, math::trigonometry const &angles, combat_tables const &performance, combat_random &war, combat_random &weapon, int const skill, mission_weapon_callbacks callbacks)
  : mission{definition}, actors{units}, spatial{locations}, targets{queries}, combat{decisions}, models{objects}, terrain{land}, scenery{structures}, mobiles{scene}, trig{angles}, tables{performance}, war_random{war}, weapon_random{weapon}, difficulty{skill}, types{actor_types()}, weapons{weapon_definitions()}, previous_state{queries.additional_state}, events{std::move(callbacks)}, rounds{objects, angles, performance, land, structures, scene, visuals, war.global_seed, {
      .guidance{[this](projectile_record &round, int32_t now){ environment.now = now; return track_projectile(round, weapons.at(round.state.weapon), targets.world, [this](uint32_t mobile, simulation::target_slot slot){ return target_slot(mobile, slot); }, terrain, environment, war_random.global_seed); }},
      .event{[this](size_t index, projectile_record &round, projectile_event_info const &info){ event(index, round, info); }}
    }}, decoys(units.units.size()) {
  targets.additional_state = [this](uint32_t mobile){ return state(mobile); };
  environment.decoy_modifier = tables.skill(difficulty).decoy_modifier;
}
mission_weapons::~mission_weapons() { rounds.clear(); targets.additional_state = std::move(previous_state); }
std::optional<size_t> mission_weapons::projectile_index(uint32_t const mobile) const {
  for(size_t i{0}; i != rounds.entries().size(); ++i) if(rounds.entries()[i] && rounds.entries()[i]->mobile == mobile) return i;
  return {};
}
mobile_target_state mission_weapons::state(uint32_t const mobile) const {
  auto result{previous_state ? previous_state(mobile) : mobile_target_state{}};
  if(auto i{spatial.actor_index(mobile)}) {
    result.damage = combat.attention[*i].damage; result.chaff_at = decoys[*i].deadlines[0]; result.flare_at = decoys[*i].deadlines[1];
    result.chaff_valid = targets.world.valid(*decoys[*i].targets[0]); result.flare_valid = targets.world.valid(*decoys[*i].targets[1]);
  } else if(auto i{projectile_index(mobile)}) { result.type = 2; result.speed = rounds.entries()[*i]->state.body.motion.speed; }
  else result.type = 0;
  return result;
}
shared_target mission_weapons::target_slot(uint32_t const mobile, simulation::target_slot const slot) const {
  auto const index{spatial.actor_index(mobile)}; if(!index) return {};
  return slot == simulation::target_slot::current ? combat.armaments[*index].target : decoys[*index].targets[slot == simulation::target_slot::chaff ? 0 : 1];
}
std::optional<size_t> mission_weapons::construct(projectile_request const &request) {
  rounds.weapon_random = weapon_random.value;
  auto const index{rounds.construct(request)};
  weapon_random.value = rounds.weapon_random;
  return index;
}
void mission_weapons::launch(size_t const index) {
  rounds.launch(index, environment.now);
  if(events.launched) events.launched(index, rounds.at(index));
}
std::optional<size_t> mission_weapons::launch_player(projectile_request const &request, bool const untracked) {
  auto const index{construct(request)};
  if(index) { launch(*index); if(untracked) rounds.at(*index).state.body.motion.flags |= 4; }
  return index;
}
bool mission_weapons::player_line_of_sight(target_reference const &source, target_reference const &target) const {
  return line_reaches_target(targets.world.position(source), targets.world.position(target), source.identity, target.identity, terrain, scenery, mobiles, models, trig);
}
bool mission_weapons::fire(size_t const index) {
  auto &actor{actors.units.at(index)}; auto &armament{combat.armaments.at(index)};
  if(!armament.target) return false;
  auto const &source{*spatial.targets[index]};
  actor_firing_context context{.now{environment.now}, .weather{environment.weather}, .task{mission.tasks[actor.location.task].type}, .source_height{models.at(actor.object.id)->height}, .night{night}, .source{source}, .player{}, .target{targets.describe(*armament.target).movement}};
  if(spatial.player) context.player = *spatial.targets[*spatial.player];
  actor_firing_callbacks callbacks;
  callbacks.visible = [&](auto const &from, auto const &to){ return line_reaches_target(from, to, source.identity, armament.target->identity, terrain, scenery, mobiles, models, trig); };
  callbacks.construct = [&](auto const &request){ return construct(request); };
  callbacks.launch = [&](size_t round){ launch(round); };
  callbacks.remove_flame = [&](size_t round){ if(auto mobile{rounds.at(round).mobile}) { auto &children{mobiles.at(*mobile).children}; if(!children.empty()) children[0].reset(); } };
  callbacks.bomb_released = events.bomb_released;
  callbacks.fired_at = [&](auto const &receiver, auto const &){ combat.notify_fired_at(receiver, spatial.targets[index]); };
  return fire_actor(actor, armament, types.at(actor.object.id), weapons, trig, tables.skill(difficulty), weapon_random, context, callbacks);
}
bool mission_weapons::fire_remote(size_t const index, unsigned int const weapon, uint32_t const seed, math::vector3 const &gun_direction, std::optional<target_reference> const &target) {
  if(index >= actors.units.size() || !weapon || weapon > 19) return false;
  auto &actor{actors.units[index]}; auto const &source{*spatial.targets[index]};
  remote_fire_input input{weapon, seed, actor.object.world, gun_direction, source, targets.describe(source).movement, target};
  for(auto const &shot : remote_weapon_shots(input, [this](int32_t x, int32_t y){ return terrain.elevation(x, y); })) {
    if(shot.kind == remote_fire_kind::chaff || shot.kind == remote_fire_kind::flare) {
      if(decoy(index, weapon)) {
        auto const interval{weapons[weapon].fire_interval_ticks};
        if(spatial.player != index) {
          // Drone randomness is separate from the shared war-model generator.
          combat_random random{actor.random, war_random.global_seed};
          auto const delay{countermeasure_delay(weapons[weapon], tables.skill(difficulty), random)};
          actor.random = random.value; war_random.global_seed = random.global_seed;
          decoys[index].deadlines[weapon - 7] = math::add(environment.now, delay);
        } else decoys[index].deadlines[weapon - 7] = math::add(environment.now, interval);
      }
    } else if(auto const round{construct(shot.request)}) {
      if(shot.kind != remote_fire_kind::simple) { rounds.at(*round).state.random = shot.seed; launch(*round); }
    }
  }
  return true;
}
firing_status mission_weapons::fire_ground(target_reference const &source, target_reference const &target, unsigned int const id) {
  auto const origin{targets.describe(source)}, destination{targets.describe(target)}; auto const &weapon{weapons.at(id)};
  auto const aim{aim_weapon(weapon, {origin.movement, destination.movement, models.at(origin.model)->height, night}, trig, [&](auto const &from, auto const &to){ return line_reaches_target(from, to, source.identity, target.identity, terrain, scenery, mobiles, models, trig); })};
  if(aim.status != firing_status::ready) return aim.status;
  projectile_request request; request.weapon = id; request.source = source; request.launch.pose = aim.pose; request.launch.source = origin.movement;
  if(weapon.movement_class == 2) request.selected = target;
  auto const index{construct(request)};
  if(!index) return firing_status::blocked;
  launch(*index); return firing_status::ready;
}
bool mission_weapons::grounded(size_t const index) const {
  return events.grounded ? events.grounded(index) : actors.on_ground(index);
}
bool mission_weapons::decoy(size_t const index, unsigned int const id) {
  if(grounded(index)) return false;
  auto const &actor{actors.units[index]}; projectile_request request; request.weapon = id; request.source = *spatial.targets[index]; request.launch.pose = actor.object.world; request.launch.source = targets.describe(request.source).movement; request.launch.simple_value = weapons.at(id).tracking_or_dispersion;
  auto const round{construct(request)}; if(!round) return false;
  launch(*round);
  *decoys[index].targets[id - 7] = target_reference{{target_kind::mobile, 0, *rounds.at(*round).mobile}, {}, 0};
  return true;
}
bool mission_weapons::release_countermeasure(unsigned int const weapon, uint16_t const key) {
  if(!spatial.player || grounded(*spatial.player)) return false;
  size_t const index{*spatial.player}; auto &actor{actors.units[index]}; auto &decoy_state{decoys[index]};
  countermeasure_actor state{static_cast<int32_t>(actor.classification), true, actor.object.world.position, actor.conditions, actor.random, actor.retreat_until, decoy_state.deadlines[0], decoy_state.deadlines[1]};
  countermeasure_callbacks callbacks;
  callbacks.launch = [&](unsigned int id){ return decoy(index, id); };
  callbacks.key_event = player_key_event; callbacks.released = countermeasure_released;
  bool const released{release_player_countermeasure(state, player_countermeasures, weapon, key, environment.now, tables.skill(difficulty), war_random.global_seed, callbacks)};
  actor.conditions = state.conditions; actor.random = state.random; actor.retreat_until = state.retreat_until; decoy_state.deadlines = {state.chaff_at, state.flare_at};
  return released;
}
void mission_weapons::incoming(size_t const round_index, projectile_record &round) {
  auto const *target{adjusted_projectile_target(round, targets.world, {})};
  if(!target || target->identity.kind != target_kind::mobile) return;
  auto const index{spatial.actor_index(target->identity.index)}; if(!index) return;
  // Keep the player's incoming warning, but parked AI neither dispenses decoys
  // nor advances its countermeasure timers/random sequence.
  if(spatial.player != index && grounded(*index)) return;
  auto &actor{actors.units[*index]}; auto &decoys{this->decoys[*index]};
  countermeasure_actor state{static_cast<int32_t>(actor.classification), spatial.player == index, actor.object.world.position, actor.conditions, actor.random, actor.retreat_until, decoys.deadlines[0], decoys.deadlines[1]};
  countermeasure_callbacks callbacks; callbacks.launch = [&](unsigned int id){ return decoy(*index, id); }; callbacks.warning = [&](int32_t arrival){ if(events.incoming) events.incoming(round_index, round.state.weapon, arrival); };
  callbacks.key_event = player_key_event; callbacks.released = countermeasure_released;
  notify_incoming_missile(state, player_countermeasures, weapons.at(round.state.weapon), round.state.body.pose.position, round.state.body.motion.speed, environment.now, tables.skill(difficulty), war_random.global_seed, callbacks);
  actor.conditions = state.conditions; actor.random = state.random; actor.retreat_until = state.retreat_until; decoys.deadlines = {state.chaff_at, state.flare_at};
}
void mission_weapons::event(size_t const index, projectile_record &round, projectile_event_info const &info) {
  if(events.event) events.event(index, round, info);
  if(info.kind == projectile_event::notify) incoming(index, round);
  if(info.kind == projectile_event::removed && round.source.identity.kind == target_kind::mobile) if(auto actor{spatial.actor_index(round.source.identity.index)}) {
    auto &state{decoys[*actor]}; auto const id{round.state.weapon};
    if((id == 7 || id == 8) && round.mobile && state.targets[id - 7]->identity.kind == target_kind::mobile && state.targets[id - 7]->identity.index == *round.mobile) { *state.targets[id - 7] = {}; state.deadlines[id - 7] = 0; }
    else if(combat.armaments[*actor].round == index) combat.armaments[*actor].round.reset();
  }
}
void mission_weapons::update(projectile_time const &time) { environment.now = time.now; rounds.update(time); }
} // namespace apache::simulation
