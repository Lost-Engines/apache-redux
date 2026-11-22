#include "mission_damage.h"

namespace apache::simulation {
mission_damage::mission_damage(resources::mission_definition const &definition, mission_scenery &ground, mission_actors &units, mission_spatial &locations, mission_targets &queries, mission_combat &decisions, mission_weapons &shots, resources::model_archive const &objects, resources::terrain const &terrain, scenery_collisions const &collisions, render::mobile_scene &scene, math::trigonometry const &trig, combat_random &war, combat_random &weapon, forces_info const &skill, std::function<uint8_t(int32_t, int32_t)> surface, mission_damage_callbacks callbacks)
  : mission{definition}, scenery{ground}, actors{units}, spatial{locations}, targets{queries}, combat{decisions}, weapons{shots}, models{objects}, structures{collisions}, mobiles{scene}, war_random{war}, weapon_random{weapon}, events{std::move(callbacks)}, explosions{objects, terrain, trig, shots.visuals, marks, weapon, std::move(surface)} {
  environment.difficulty_percent = skill.damage_percent; environment.decoy_modifier = skill.decoy_modifier;
}
damage_recipient mission_damage::read(target_identity const &id) const {
  damage_recipient result;
  if(id.kind == target_kind::scenery) {
    auto const &object{(id.layer ? scenery.extra : scenery.base).at(id.index)};
    result.model = object.model; result.flags = object.status; result.position = {object.x, object.y, 0};
  } else {
    auto const &object{mobiles.at(id.index)}; result.model = object.id; result.flags = object.status; result.mobile = true; result.position = object.world.position;
    if(auto i{spatial.actor_index(id.index)}) { result.actor = true; result.player = spatial.player == i; result.network = actors.units[*i].network; result.damage = combat.attention[*i].damage; }
  }
  return result;
}
void mission_damage::write(target_identity const &id, damage_recipient const &state) {
  if(id.kind == target_kind::scenery) { auto &object{(id.layer ? scenery.extra : scenery.base).at(id.index)}; object.status = static_cast<uint16_t>(state.flags); } else { mobiles.at(id.index).status = state.flags; if(auto i{spatial.actor_index(id.index)}) combat.attention[*i].damage = state.damage; }
}
void mission_damage::kill(target_identity const &id, target_identity const &source) {
  if(id.kind == target_kind::scenery) {
    auto &object{(id.layer ? scenery.extra : scenery.base).at(id.index)}; if(object.status & 1) return;
    if(events.killed && source.kind != target_kind::none) events.killed(id, source);
    auto const position{math::vector3{object.x, object.y, 0}}; explosions.ground(object, position, false); object.status |= 1;
    return;
  }
  auto const index{spatial.actor_index(id.index)}; if(!index) return;
  auto &actor{actors.units[*index]}; if(actor.object.status & 1) return;
  if(spatial.player == index) {
    if(invulnerable) return;
    combat.attention[*index].damage = combat.attention[*index].processed_damage = 0;
    if(events.player) events.player(damage_action::kill, source);
    if(events.player_killed) events.player_killed(source);
    return;
  }
  if(events.killed && source.kind != target_kind::none) events.killed(id, source);
  if(actor.object.id == 473 || (environment.mode != 4 && environment.mode != 5)) combat.kill(*index);
  explosions.mobile(actor.object.id, actor.object.world, false);
  for(size_t child{0}; child != actor.object.children.size(); ++child) if(auto &object{actor.object.children[child]}) {
    std::optional<size_t> occupant;
    for(auto const &carrier : actors.carriers) if(carrier.actor == *index) for(auto const &slot : carrier.slots) if(slot.attachment == child) occupant = slot.occupant;
    if(occupant) { object->status |= 1; actors.units[*occupant].object.status |= 1; combat.kill(*occupant); } else { render::maintain_model(actor.object); explosions.mobile(object->id, object->world, false); }
  }
}
void mission_damage::partial(target_identity const &id) {
  if(id.kind == target_kind::scenery) { auto &object{(id.layer ? scenery.extra : scenery.base).at(id.index)}; explosions.ground(object, {object.x, object.y, 0}, true); } else { auto const &object{mobiles.at(id.index)}; explosions.mobile(object.id, object.world, true); }
}
void mission_damage::auxiliary(target_identity const &id, projectile_record const &round) {
  auto const recipient{read(id)}; if(!(models.at(recipient.model)->metadata[7] & 1)) return;
  int32_t const side{static_cast<int32_t>((recipient.flags & 7) >> 1)};
  auto const sound{[&](unsigned int value, bool cockpit){ if(events.sound) events.sound(value, cockpit); }};
  bool const player_source{spatial.player && equivalent_targets(round.source, *spatial.targets[*spatial.player])};
  if(player_source) {
    if(events.score) events.score(static_cast<int32_t>(recipient.model));
    if(side == 1) { sound(friendly_fire < 3 ? 0x4a : 0x4b, true); if(events.message) events.message(friendly_fire < 3 ? "HIT ALLIED TARGET" : "CEASE FIRE"); ++friendly_fire; } else if(weapon_random.next() % 10 < 2) {
      auto const id{round.state.weapon};
      if(id >= 1 && id <= 6) sound(id == 1 ? 0x43 : id <= 3 ? 0x40 : id <= 5 ? 0x41 : 0x42, true);
    } else if(weapon_random.next() % 10 > 8) sound(0x44, true);
  } else {
    auto const choice{weapon_random.next() % 20};
    if(choice >= 1 && choice <= 4) sound(choice == 4 ? 0x61 : static_cast<unsigned int>(0x65 + choice), false);
  }
  if(side != 1 || id.kind != target_kind::mobile) return;
  auto const index{spatial.actor_index(id.index)}; if(!index) return;
  auto const &actor{actors.units[*index]};
  if(actor.classification == 1 && mission.tasks[actor.location.task].user) { sound(0x62, true); return; }
  if(actor.classification != 1 && actor.classification != 2 && actor.classification != 7) return;
  auto const choice{weapon_random.next() % 10};
  if(choice == 1 || choice == 2) sound(actor.classification == 7 ? (choice == 1 ? 0x6b : 0x6a) : (choice == 1 ? 0x62 : 0x65), false);
}
void mission_damage::destroy(target_identity const &target) {
  explosions.listener = combat.player_position; explosions.listener_heading = combat.player_heading;
  weapon_random.global_seed = war_random.global_seed;
  kill(target, {});
  war_random.global_seed = weapon_random.global_seed;
}
void mission_damage::impact(projectile_record &round, projectile_event_info const &event) {
  if(event.kind == projectile_event::notify || event.kind == projectile_event::removed) return;
  explosions.listener = combat.player_position; explosions.listener_heading = combat.player_heading;
  weapon_random.global_seed = war_random.global_seed;
  if(event.kind == projectile_event::expired || event.kind == projectile_event::map_edge) { explosions.weapon(round.state.weapon, round.state.body.pose, false, false); war_random.global_seed = weapon_random.global_seed; return; }
  auto const origin{targets.describe(round.source)};
  damage_source source{round.source.identity.kind, origin.side, -1, origin.actor};
  if(source.actor) if(auto index{spatial.actor_index(round.source.identity.index)}) source.network = actors.units[*index].network;
  damage_random random{weapon_random.value, round.state.random, war_random.global_seed};
  auto const effects{[&](auto const &callback){
    weapon_random.value = random.weapon; weapon_random.global_seed = war_random.global_seed = random.global;
    callback();
    random.weapon = weapon_random.value; random.global = war_random.global_seed = weapon_random.global_seed;
  }};
  blast_callbacks callbacks;
  callbacks.read = [&](auto const &id){ return read(id); }; callbacks.write = [&](auto const &id, auto const &state){ write(id, state); };
  callbacks.action = [&](auto const &id, damage_action action){ effects([&]{ if(action == damage_action::kill) kill(id, round.source.identity); else if(events.player) events.player(action, round.source.identity); }); };
  callbacks.partial_explosion = [&](auto const &id){ effects([&]{ partial(id); }); };
  callbacks.auxiliary = [&](auto const &id){ effects([&]{ auxiliary(id, round); }); };
  callbacks.weapon_explosion = [&](auto const &position){ effects([&]{ explosions.weapon(round.state.weapon, round.state.body.pose, false, false, position); }); };
  blast_damage blast{round.state.weapon, source, environment, random, models, structures, mobiles, callbacks};
  if(event.target) blast.hit(*event.target, event.position); else blast.detonate(event.position);
  weapon_random.value = random.weapon; weapon_random.global_seed = war_random.global_seed = random.global; round.state.random = random.round;
}
} // namespace apache::simulation
