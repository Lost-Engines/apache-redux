#include "mission_combat.h"
#include <algorithm>
#include "ground_movement.h"

namespace apache::simulation {
namespace {
shared_target fixed_target(math::vector3 const &position) {
  return std::make_shared<target_reference>(target_reference{{target_kind::fixed, 0, 0}, position, 0});
}
}
mission_combat::mission_combat(resources::mission_definition const &definition, mission_scenery &structures, mission_actors &units, mission_spatial &locations, mission_targets &queries, resources::terrain const &land, math::trigonometry const &angles, combat_tables const &performance, std::span<std::vector<resources::formation_member> const> shapes, actor_environment const &environment, combat_random &war, combat_random &weapon_rng, int const skill, mission_combat_callbacks callbacks)
  : mission{definition}, scenery{structures}, actors{units}, spatial{locations}, targets{queries}, terrain{land}, trig{angles}, tables{performance}, formations{shapes}, ground{environment}, war_random{war}, weapon_random{weapon_rng}, previous_shared_seed{weapon_rng.shared_seed}, types{actor_types()}, weapons{weapon_definitions()}, group_indices(units.units.size()), task_order(definition.tasks.size()), patrol_slots(units.units.size(), 255), events{std::move(callbacks)}, difficulty{skill}, armaments(units.units.size()), attention(units.units.size()), alive(definition.tasks.size()), battles{initialise_battles(definition, units.groups)} {
  weapon_random.shared_seed = &war_random.global_seed;
  std::array<unsigned int, 2> cap_count{};
  for(size_t g{0}; g != actors.groups.size(); ++g) {
    auto const &group{actors.groups[g]}; auto const &task{mission.tasks[group.task]};
    for(auto index : group.units) group_indices[index] = g;
    if(task.type == 15 && task.side >= 1 && task.side <= 2) patrol_slots[group.units.front()] = cap_count[static_cast<size_t>(task.side - 1)]++;
  }
  for(size_t task{0}; task != task_order.size(); ++task) { task_order[task] = task_actor_order(actors.groups, task); alive[task] = static_cast<int32_t>(task_order[task].size()); }
  for(size_t i{0}; i != actors.units.size(); ++i) {
    auto &actor{actors.units[i]}; auto const &task{mission.tasks[actor.location.task]};
    refresh_weapon_ranges(actor, armaments[i], types.at(actor.object.id), weapons);
    attention[i].range = attention_range(task.type, task.side, tables.skill(difficulty));
  }
}
mission_combat::~mission_combat() {
  weapon_random.shared_seed = previous_shared_seed;
}
void mission_combat::set_time(int32_t const clock, int32_t const step, uint32_t const frame_number) {
  now = clock;
  delta = step;
  frame = frame_number;
  targets.now = now;
}
actor_targeting_context mission_combat::context(size_t const index) const {
  auto const &actor{actors.units.at(index)}; auto const &group{actors.groups[group_indices[index]]}; auto const &route{group.definition.waypoints};
  actor_targeting_context result{.now{now}, .task{mission.tasks[group.task].type}, .start_time{group.definition.start_time}, .has_formation{index == group.units.front() && group.definition.formation != 0}, .radar_model{radar_model(actor.object.id)}, .path{}, .player_target{player_target}};
  if(actor.waypoint) {
    size_t const waypoint{*actor.waypoint}; result.path.type = route.at(waypoint).type; result.path.current = route[waypoint].position;
    if(waypoint) result.path.previous = route[waypoint - 1].position;
    if(waypoint + 1 < route.size()) result.path.next = route[waypoint + 1].position;
  }
  return result;
}
int32_t mission_combat::weapon(size_t const i) {
  auto &arm{armaments[i]};
  return choose_actor_weapon(actors.units[i], arm, types.at(actors.units[i].object.id), weapons, arm.target ? targets.describe(*arm.target).movement : movement_target{}, mission.tasks[actors.units[i].location.task].type);
}
int32_t mission_combat::deadline(size_t const i) {
  if(!weapon(i)) return INT32_MAX;
  int32_t const slot{actors.units[i].weapon_slot};
  return slot >= 0 && slot < 5 ? armaments[i].deadlines[static_cast<size_t>(slot)] : INT32_MAX;
}
void mission_combat::set_deadline(size_t const i, int32_t const value) {
  if(weapon(i)) { int32_t const slot{actors.units[i].weapon_slot}; if(slot >= 0 && slot < 5) armaments[i].deadlines[static_cast<size_t>(slot)] = value; }
}
void mission_combat::notify(size_t const receiver, shared_target const &source, bool const fired) {
  if(!source) return;
  auto &actor{actors.units[receiver]}; auto const description{targets.describe(*source)};
  actor_reaction_context inputs{.now{now}, .task{mission.tasks[actor.location.task].type}, .attacker_side{description.side}, .attacker_model{description.model}, .view_heading{player_heading}, .player{spatial.player == receiver}, .view_position{player_position}, .attacker{description.movement}, .reference{source}};
  actor_reaction_callbacks callbacks{.visible{[&]{ return !events.visible || events.visible(*spatial.targets[receiver], *source); }}, .target_released{}, .sound{events.sound}};
  if(fired) notify_actor_fired_at(actor, armaments[receiver], types.at(actor.object.id), weapons, warnings, war_random.value, weapon_random, inputs, callbacks);
  else notify_actor_targeted(actor, armaments[receiver], types.at(actor.object.id), weapons, warnings, war_random.value, weapon_random, trig, inputs, callbacks);
}
void mission_combat::notify_fired_at(target_reference const &receiver, shared_target const &source) {
  if(receiver.identity.kind == target_kind::mobile) if(auto i{spatial.actor_index(receiver.identity.index)}) notify(*i, source, true);
}
void mission_combat::acquire(size_t const index, int const mode) {
  auto &actor{actors.units[index]}; auto inputs{context(index)};
  actor_targeting_callbacks callbacks;
  callbacks.describe = [&](auto const &target){ return targets.describe(target).movement; };
  callbacks.actor_class = [&](auto const &target){ auto i{target.identity.kind == target_kind::mobile ? spatial.actor_index(target.identity.index) : std::nullopt}; return i ? static_cast<int32_t>(actors.units[*i].classification) : 0; };
  callbacks.search = [&](auto const &origin, int32_t side, int32_t minimum, int32_t maximum, search_kind kind){ return targets.search(origin, side, minimum, maximum, kind); };
  callbacks.ground_sector = [&](int32_t x, int32_t y, int32_t side){ return targets.in_sector(x, y, side, actor.object.world.position, search_kind::ground); };
  callbacks.visible = [&](auto const &target){ return !events.visible || events.visible(*spatial.targets[index], target); };
  callbacks.height = [&](int32_t x, int32_t y){ return terrain.elevation(x, y); }; callbacks.fixed = fixed_target;
  callbacks.targeted = [&](auto const &target){ if(auto i{spatial.actor_index(target.identity.index)}) notify(*i, spatial.targets[index], false); };
  callbacks.sound = [&](unsigned int sound){ if(events.sound) events.sound(sound, true); }; callbacks.message = events.message;
  actor_targeting local{actor, armaments[index], attention[index], types.at(actor.object.id), weapons, trig, tables.skill(difficulty), tables.skill(9), weapon_random, bombing_message_time, inputs, callbacks};
  callbacks.cap = [&]{ patrol(index, local, inputs); };
  if(mode == 0) local.aircraft(); else if(mode == 1) local.air_defence(); else if(mode == 2) local.artillery(); else local.check_air_lock();
}
void mission_combat::check_air_lock(size_t const actor) {
  acquire(actor, 3);
}
std::optional<movement_target> mission_combat::movement_aim(size_t const actor) const {
  auto const &target{armaments.at(actor).target};
  return target ? std::optional{targets.describe(*target).movement} : std::nullopt;
}
patrol_radar_callbacks mission_combat::radar_callbacks() const {
  return {.position{[&](auto const &t){ return targets.world.position(t); }}, .floor{[&](auto const &p, unsigned int side){ return spatial.grid.floor(p[0], p[1], side); }}, .radar{[&](auto const &p, unsigned int side){ return spatial.grid.radar(p[0], p[1], side); }}, .status{[&](auto const &t){ return targets.describe(t).status; }}};
}
void mission_combat::scan_patrol() {
  scan_patrol_contacts(patrols, now, [&](int32_t x, int32_t y){
    patrol_sector cell; cell.owner = scenery.owner(x, y);
    for(unsigned int side{1}; side <= 2; ++side) if(auto const *s{spatial.grid.sector(x, y, side)}) { cell.present[side - 1] = true; cell.air[side - 1] = s->air.targets; }
    return cell;
  }, [&](int32_t x, int32_t y, unsigned int side){ return spatial.grid.status(x, y, side, [&](auto const &t){ return targets.describe(t).status; }); }, radar_callbacks());
}
void mission_combat::patrol(size_t const index, actor_targeting &local, actor_targeting_context const &inputs) {
  auto &actor{actors.units[index]}; unsigned int const slot{patrol_slots[index]}, side{(actor.object.status & 6) == 2 ? 0u : 1u};
  shared_target const assigned{slot < 20 ? patrols.assigned[side][slot] : nullptr};
  patrol_acquisition_callbacks callbacks{.radar{radar_callbacks()}, .basic{[&]{ local.basic_aircraft(); }}, .released{}, .select{[&]{ return select_patrol_target(patrols, actor, attention[index].range, inputs.path, slot, now, trig, [&]{ scan_patrol(); }, [&](auto const &t){ return targets.world.position(t); }); }}, .weapon_range{[&]{ return weapons.at(static_cast<size_t>(weapon(index))).maximum_range; }}, .actor_class{[&](auto const &t){ auto i{t.identity.kind == target_kind::mobile ? spatial.actor_index(t.identity.index) : std::nullopt}; return i ? static_cast<int32_t>(actors.units[*i].classification) : 0; }}, .fixed{fixed_target}};
  acquire_patrol_target(actor, armaments[index], assigned, now, callbacks);
}
void mission_combat::fire_at_waypoint(size_t const index) {
  auto const &actor{actors.units[index]}; auto const inputs{context(index)}; auto saved{armaments[index].target};
  if(inputs.path.current) armaments[index].target = targets.in_sector((*inputs.path.current)[0] >> 22, (*inputs.path.current)[1] >> 22, (actor.object.status & 6) == 2 ? 2 : 1, actor.object.world.position, search_kind::ground);
  if(events.fire) events.fire(index);
  armaments[index].target = std::move(saved);
}
void mission_combat::kill(size_t const index) {
  auto &actor{actors.units[index]}; auto const &group{actors.groups[group_indices[index]]}; std::vector<actor_state *> followers;
  if(group.units.front() == index) for(size_t i{group.units.size()}; i-- > 1;) followers.push_back(&actors.units[group.units[i]]);
  kill_actor(actor, armaments[index], alive[actor.location.task], followers, [&]{ spatial.unplace(index); }, {});
}
void mission_combat::update_statics() {
  ground_defence_callbacks callbacks;
  callbacks.position = [&](auto const &t){ return targets.world.position(t); }; callbacks.status = [&](auto const &t){ return targets.describe(t).status; }; callbacks.side = [&](auto const &t){ return targets.describe(t).side; };
  callbacks.search = [&](auto const &origin, int32_t side, int32_t minimum, int32_t maximum){ return targets.search(origin, side, minimum, maximum, search_kind::air); };
  callbacks.heading = [&](auto const &t, uint16_t heading){ auto &layer{t.identity.layer ? scenery.extra : scenery.base}; layer.at(t.identity.index).heading = heading; };
  callbacks.shoot = [&](auto const &source, auto const &target, unsigned int id){ return events.ground_fire ? events.ground_fire(source, target, id) : firing_status::blocked; };
  visit_ground_defence_sectors(player_position, [&](int32_t x, int32_t y){ return scenery.owner(x, y); }, [&](int32_t x, int32_t y){
    auto const &sector{scenery.sectors[static_cast<size_t>(x * 64 + y)]};
    for(size_t first : {size_t{2}, size_t{0}}) for(size_t j{first}; j != first + 2; ++j) {
      if(!sector.objects[j]) break;
      auto &object{scenery.objects[*sector.objects[j]].combat}; if(!object.self || targets.describe(*object.self).status == 1) continue;
      auto const &type{types.at(targets.describe(*object.self).model)}; auto const id{static_cast<unsigned int>(type.weapons.at(type.main_weapon))};
      acquire_ground_defence_target(object, weapons.at(id), now, frame, tables.skill(difficulty), tables.skill(9), weapon_random, trig, callbacks);
      fire_ground_defence(object, id, weapons.at(id), now, tables.skill(difficulty), weapon_random, trig, callbacks);
    }
  });
}
void mission_combat::update_ground_battles() {
  battle_callbacks callbacks;
  callbacks.survival = [&](size_t task, int32_t initial){ return task_order[task].empty() ? -1 : battle_survival(actors.units[task_order[task].front()], alive[task], initial); };
  callbacks.release = [&](size_t task){ release_battle_formation(task_order[task], armaments); };
  callbacks.random = [&]{ return war_random.next(); };
  callbacks.finish = [&](size_t task, bool advance){ for(auto const &group : actors.groups) if(group.task == task) { ground_movement movement{actors.units, group, mission.tasks[task], formations[group.definition.formation], trig, ground}; movement.finish_battle(advance, delta); } };
  callbacks.combat = [&](size_t task){
    battle_combat_callbacks combat;
    combat.acquire = [&](size_t index){
      auto &actor{actors.units[index]}; auto const &orders{mission.tasks[task]}; auto const &cell{battles.at(static_cast<size_t>(orders.battle_cell))}; auto const &opponent{cell.sides[static_cast<size_t>(2 - orders.side)]};
      battle_acquisition_callbacks acquire;
      acquire.select = [&]{ if(!opponent.task) return shared_target{}; auto i{random_task_survivor(task_order[*opponent.task], actors.units, alive[*opponent.task], [&]{ return war_random.next(); })}; return i ? spatial.targets[*i] : shared_target{}; };
      acquire.position = [&](auto const &target){ return targets.world.position(target); }; acquire.height = [&](int32_t x, int32_t y){ return terrain.elevation(x, y); }; acquire.fixed = fixed_target;
      acquire_battle_target(actor, armaments[index], types.at(actor.object.id), weapons, trig, war_random.global_seed, {static_cast<int32_t>(frame), actor.serial, cell.active}, acquire);
    };
    combat.fire = [&](size_t i){ if(events.fire) events.fire(i); }; combat.deadline = [&](size_t i){ return deadline(i); }; combat.set_deadline = [&](size_t i, int32_t value){ set_deadline(i, value); }; combat.random = [&]{ return war_random.next(); };
    update_battle_formation(task_order[task], actors.units, armaments, now, combat);
  };
  update_battles(battles, now, callbacks);
}
void mission_combat::update() {
  update_actor_formations(actors.groups, actors.units, mission.tasks, spatial.player, [&](size_t i){
    actor_combat_callbacks callbacks;
    callbacks.status = [&](auto const &t){ return targets.describe(t).status; }; callbacks.position = [&](auto const &t){ return targets.world.position(t); };
    callbacks.air_acquire = [&]{ acquire(i, 0); }; callbacks.ground_acquire = [&]{ acquire(i, 1); }; callbacks.artillery_acquire = [&]{ acquire(i, 2); };
    callbacks.fire = [&]{ if(events.fire) events.fire(i); }; callbacks.weapon = [&]{ return static_cast<unsigned int>(weapon(i)); }; callbacks.behind = [&]{ return armaments[i].target && target_behind_actor(actors.units[i], targets.world.position(*armaments[i].target)); };
    return update_actor_combat(actors.units[i], armaments[i], mission.tasks[actors.units[i].location.task].type, actors.units[i].serial, frame, weapons, callbacks);
  });
  update_statics(); update_ground_battles();
}
} // namespace apache::simulation
