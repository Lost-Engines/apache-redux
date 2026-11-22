#include "mission_network.h"
#include "render/fragment_renderer.h"

namespace apache::simulation {
mission_network::mission_network(mission_runtime &mission)
  : runtime{mission},
    lifecycle{state} {
  auto const link{[this](size_t index){
    return multiplayer::actor_link{index, runtime.actors.units[index].destroyed, runtime.spatial.player == index};
  }};
  lifecycle.events.player = [this, link](uint32_t connection)->std::optional<multiplayer::actor_link> {
    if(auto index{actor(connection)}) return link(*index);
    return {};
  };
  lifecycle.events.find = [this, link](int32_t serial, int32_t task)->std::optional<multiplayer::actor_link> {
    for(size_t index{0}; index < runtime.actors.units.size(); ++index) {
      auto const &unit{runtime.actors.units[index]};
      if(unit.serial == serial && static_cast<int64_t>(unit.location.task) == task) return link(index);
    }
    return {};
  };
  lifecycle.events.remove = [this](size_t index){
    runtime.combat.kill(index);
    render::fracture_model(runtime.actors.units[index].object, runtime.debris, runtime.random.global_seed);
  };
  lifecycle.events.unplace = [this](size_t index){runtime.spatial.unplace(index);};
  lifecycle.events.leave = [this](size_t index){
    runtime.spatial.unplace(index);
    runtime.actors.units[index].destroyed = 1;
  };
  lifecycle.events.restart = [this](size_t index, math::pose const &position, int32_t model){
    runtime.actors.units[index].object.world = position;
    for(uint32_t connection{0}; connection < assignments.size(); ++connection) {
      if(assignments[connection] == index) {
        revive(connection, model);
        break;
      }
    }
  };
  lifecycle.events.countermeasure = [this](size_t index, bool flare){
    if(runtime.weapons) runtime.weapons->fire_remote(index, flare ? 8 : 7, 0, {}, {});
  };
  lifecycle.events.death = [this](unsigned int cause){
    if(runtime.pilot) runtime.pilot->request_death(cause);
  };
  lifecycle.events.player_killed = [this]{
    if(!runtime.spatial.player) return;
    auto &object{runtime.actors.units[*runtime.spatial.player].object};
    if(object.status & 1) return;
    if(runtime.pilot) runtime.pilot->request_death(4);
    auto const mode{lifecycle.mode};
    if(mode != multiplayer::game_mode::combat && mode != multiplayer::game_mode::multiplayer && mode != multiplayer::game_mode::capture_the_flag) object.status |= 1;
  };
  lifecycle.events.mobile_killed = [this](size_t index, uint8_t source){
    if(runtime.actors.units[index].object.status & 1) return;
    auto const id{runtime.spatial.targets[index]->identity};
    if(remote_killed) remote_killed(id, source);
    if(runtime.damage) runtime.damage->destroy(id);
  };
  lifecycle.events.scenery_killed = [this](math::vector3 const &position, uint8_t source){
    auto const target{resolve({1, position})};
    if(!target) return;
    auto const &id{target->identity};
    auto const &object{(id.layer ? runtime.scene.extra : runtime.scene.base)[id.index]};
    if(object.status & 1) return;
    if(remote_killed) remote_killed(id, source);
    if(runtime.damage) runtime.damage->destroy(id);
  };
  lifecycle.events.flag_won = [this](uint32_t slot){flag_winner = slot;};
  lifecycle.events.notice = [this](auto text, auto priority, auto lifetime){
    if(runtime.events.notice) runtime.events.notice(text, priority, lifetime);
  };
  lifecycle.events.scores = [this]{
    std::array<int32_t, 16> values{};
    for(size_t i{0}; i < values.size(); ++i) values[i] = state.players[i].score;
    if(send) send(multiplayer::scores(state.local, state.players[state.local].name, values));
  };
}
void mission_network::send_player_kill(uint32_t const victim) {
  if(!enabled || !state.active || !send || state.local >= state.players.size() || !runtime.spatial.player) return;
  auto &player{state.players[state.local]};
  if(state.client_server) {
    if(state.server == state.local) {
      player.score = math::add(player.score, 1);
      if(lifecycle.events.scores) lifecycle.events.scores();
    }
    if(runtime.events.notice) runtime.events.notice("Score : " + std::to_string(player.score), 3, 200);
  }
  auto const &unit{runtime.actors.units[*runtime.spatial.player]};
  send(multiplayer::event(multiplayer::message_type::killed_player, state.local, player.name,
    static_cast<int32_t>(victim), static_cast<int32_t>(unit.location.task)));
}
void mission_network::player_killed(target_identity const &source) {
  if(!runtime.spatial.player || runtime.actors.units[*runtime.spatial.player].destroyed) return;
  if(source.kind == target_kind::scenery || (source.kind == target_kind::mobile && runtime.spatial.actor_index(source.index))) {
    send_player_kill(static_cast<uint32_t>(runtime.actors.units[*runtime.spatial.player].serial));
  }
}
void mission_network::record_kill(target_identity const &target, target_identity const &source) {
  if(!enabled || lifecycle.mode == multiplayer::game_mode::single_player) return;
  auto const source_actor{source.kind == target_kind::mobile ? runtime.spatial.actor_index(source.index) : std::nullopt};
  uint32_t allegiance{0};
  if(source_actor) {
    allegiance = runtime.actors.units[*source_actor].object.status;
  } else if(source.kind == target_kind::scenery && runtime.structures.contains(source.layer, source.index)) {
    allegiance = runtime.structures.at(source.layer, source.index).status;
  } else {
    return;
  }
  auto const mode{lifecycle.mode};
  uint8_t cause{static_cast<uint8_t>(((allegiance >> 1) & 3) == 1 ? 1 : 2)};
  if(mode == multiplayer::game_mode::apache_vs_hind) cause = static_cast<uint8_t>(3 - cause);
  if(source_actor && ((state.local < 2 && source_actor == actor(1 - state.local)) || (mode == multiplayer::game_mode::pilot_gunner && source_actor == runtime.spatial.player))) cause = 3;
  bool const cooperative{mode == multiplayer::game_mode::single_missions || mode == multiplayer::game_mode::pilot_gunner || mode == multiplayer::game_mode::apache_vs_hind};
  if(target.kind == target_kind::scenery) {
    if(cooperative && runtime.structures.contains(target.layer, target.index)) {
      auto const &object{runtime.structures.at(target.layer, target.index)};
      if(!(object.status & 1)) casualties.scenery(object.x, object.y, cause);
    }
    return;
  }
  auto const index{target.kind == target_kind::mobile ? runtime.spatial.actor_index(target.index) : std::nullopt};
  if(!index || index == runtime.spatial.player) return;
  auto const &unit{runtime.actors.units[*index]};
  if(unit.object.status & 1) return;
  if(cooperative) {
    auto task{static_cast<int32_t>(unit.location.task)};
    if(mode == multiplayer::game_mode::apache_vs_hind && task < 2) task = 1 - task;
    casualties.mobile(task, unit.serial, cause);
  }
  if(source_actor == runtime.spatial.player && source_actor && !unit.destroyed) {
    for(uint32_t connection{0}; connection < assignments.size(); ++connection) {
      if(assignments[connection] == index) {
        if(lifecycle.events.remove) lifecycle.events.remove(*index);
        send_player_kill(connection);
        break;
      }
    }
  }
}
void mission_network::send_casualties() {
  if(!enabled || !state.active || !send || state.local >= state.players.size()) return;
  auto const &name{state.players[state.local].name};
  if(auto message{casualties.take_mobile(state.local, name)}) send(*message);
  if(auto message{casualties.take_scenery(state.local, name)}) send(*message);
}
std::optional<size_t> mission_network::actor(uint32_t const connection) const {
  if(connection >= state.players.size()) return {};
  return assignments[connection];
}
bool mission_network::bind(uint32_t const connection, size_t const index) {
  if(connection >= state.players.size() || index >= runtime.actors.units.size() || runtime.actors.units[index].location.member) return false;
  if(auto previous{actor(connection)}; previous && *previous != index) return false;
  auto &unit{runtime.actors.units[index]};
  if(unit.network != -1 && unit.network != static_cast<int32_t>(connection)) return false;
  unit.network = static_cast<int32_t>(connection);
  assignments[connection] = index;
  state.players[connection].assigned = true;
  return true;
}
void mission_network::replace_model(size_t const index, uint32_t const model, unsigned int const map) {
  static auto const types{actor_types()};
  static auto const weapons{weapon_definitions()};
  auto &unit{runtime.actors.units[index]};
  auto const &type{types.at(model)};
  auto const &task{runtime.mission.tasks[unit.location.task]};
  std::array<uint32_t, 20> attachments{};
  if(model == 607) {
    attachments[0] = 614;
    attachments[2] = 620;
  } else if(model == 990) attachments[0] = 859;
  auto replacement{render::construct_model(runtime.models, model, unit.object.world, attachments)};
  replacement.alternate_colours = task.side == 2 && map != 2 && (type.flags & 2);
  replacement.status = (unit.object.status & ~0x10u) | (replacement.alternate_colours ? 0x10u : 0);
  unit.object = std::move(replacement);
  unit.classification = type.classification;
  unsigned int count{0};
  int32_t last_weapon{0};
  int32_t last_slot{-1};
  for(size_t slot{0}; slot < type.weapons.size(); ++slot) {
    if(type.weapons[slot]) {
      ++count;
      unit.ammunition[slot] = type.ammunition[slot];
      last_weapon = type.weapons[slot];
      last_slot = static_cast<int32_t>(slot);
    }
  }
  if(task.type == 18) {
    if(!unit.ammunition[1]) ++count;
    unit.ammunition[1] = 10;
  }
  if(count == 0) {
    unit.weapon = 0;
    unit.weapon_slot = -1;
    unit.conditions &= ~0xc0u;
  } else if(count == 1) {
    unit.weapon = last_weapon;
    unit.weapon_slot = last_slot;
    unit.conditions |= 0xc0;
  } else {
    unit.weapon_slot = 0;
    unit.conditions = (unit.conditions & ~0x80u) | 0x40;
  }
  refresh_weapon_ranges(unit, runtime.combat.armaments[index], type, weapons);
}
bool mission_network::assign(uint32_t const connection, uint32_t const model, size_t const task, multiplayer::game_mode const mode, int const flow, unsigned int const map) {
  static auto const types{actor_types()};
  if(!enabled || connection >= state.players.size() || task >= runtime.mission.tasks.size() || model >= runtime.models.size() || model >= types.size() || !runtime.models[model] || !types[model].classification || types[model].classification == 7 || mode == multiplayer::game_mode::single_player) return false;
  auto const serial{flow == 7 ? 0 : static_cast<int32_t>(connection)};
  for(auto const &group : runtime.actors.groups) {
    size_t const index{group.units.front()};
    auto &unit{runtime.actors.units[index]};
    if(group.task != task || unit.serial != serial || (unit.classification != 1 && unit.classification != 2)) continue;
    bool const cooperative{mode == multiplayer::game_mode::single_missions || mode == multiplayer::game_mode::pilot_gunner};
    if(!cooperative || !(unit.conditions & 0x400)) {
      bool const friendly{cooperative || (mode == multiplayer::game_mode::capture_the_flag && task == 0)};
      runtime.spatial.change_status(index, (unit.object.status & ~6u) | (friendly ? 2u : 4u));
    }
    unit.network = static_cast<int32_t>(connection);
    assignments[connection] = index;
    state.players[connection].assigned = true;
    state.players[connection].flags = (state.players[connection].flags & ~0xff00u) | (connection << 8);
    runtime.movement.release_carrier(index, mode == multiplayer::game_mode::single_missions);
    replace_model(index, model, map);
    return true;
  }
  return false;
}
void mission_network::remove_unassigned() {
  for(auto const &group : runtime.actors.groups) {
    size_t const index{group.units.front()};
    auto &unit{runtime.actors.units[index]};
    if(unit.network != -1) continue;
    auto const id{unit.object.id};
    if(id == 473 || id == 994 || id == 995) {
      runtime.spatial.change_status(index, (unit.object.status & ~6u) | 4);
    } else {
      unit.object.status |= 1;
      runtime.spatial.unplace(index);
    }
  }
}
bool mission_network::revive(uint32_t const connection, int32_t const model) {
  auto const index{actor(connection)};
  if(!index) return false;
  auto &unit{runtime.actors.units[*index]};
  bool const apache{model == 64 || model == 103};
  std::array<uint32_t, 20> attachments{};
  if(!apache) attachments[0] = 1009;
  auto replacement{render::construct_model(runtime.models, apache ? 64 : 990, unit.object.world, attachments)};
  replacement.status = unit.object.status & ~0x11u;
  unit.object = std::move(replacement);
  unit.destroyed = 0;
  runtime.combat.attention[*index].damage = runtime.combat.attention[*index].processed_damage = 0;
  runtime.spatial.place(*index);
  return true;
}
std::optional<target_reference> mission_network::resolve(multiplayer::target_state const &target) const {
  if(target.kind == 2) {
    for(size_t i{0}; i < runtime.actors.units.size(); ++i) {
      auto const &unit{runtime.actors.units[i]};
      if(unit.serial == target.data[0] && static_cast<int64_t>(unit.location.task) == target.data[1]) return *runtime.spatial.targets[i];
    }
  } else if(target.kind == 1) {
    auto const &position{target.data};
    if(position[0] < 0 || position[0] >= 0x10000000 || position[1] < 0 || position[1] >= 0x10000000) return {};
    // locate_ground_object uses the stored 256-unit coordinate cell, not the
    // object's collision bounds, altitude, visibility or damage flags.
    for(size_t layer{0}; layer < 2; ++layer) {
      auto const &objects{layer ? runtime.scene.extra : runtime.scene.base};
      for(size_t i{0}; i < objects.size(); ++i) {
        auto const &object{objects[i]};
        if(object.model && object.sector == (position[0] >> 22) * 64 + (position[1] >> 22) && (object.x >> 8) == (position[0] >> 8) && (object.y >> 8) == (position[1] >> 8)) return target_reference{{target_kind::scenery, layer, static_cast<uint32_t>(i)}, position, 0};
      }
    }
  }
  return {};
}
multiplayer::target_state mission_network::identify(target_reference const &target) const {
  auto const &id{target.identity};
  if(id.kind == target_kind::mobile) {
    if(auto index{runtime.spatial.actor_index(id.index)}) {
      auto const &unit{runtime.actors.units[*index]};
      return {2, {unit.serial, static_cast<int32_t>(unit.location.task), 0}};
    }
  } else if(id.kind == target_kind::scenery && runtime.structures.contains(id.layer, id.index)) {
    return {1, runtime.targets.world.position(target)};
  }
  return {};
}
bool mission_network::receive(multiplayer::packet const &message, int32_t const now) {
  if(!enabled) return false;
  if(lifecycle.receive(message, now)) return true;
  if(!state.receive(message, now)) return false;
  if(message.type() == multiplayer::message_type::position) {
    if(auto index{actor(message.sender())}) {
      auto const target{resolve(state.players[message.sender()].target)};
      auto &slot{runtime.combat.armaments[*index].target};
      if(target) {
        if(!slot) slot = std::make_shared<target_reference>();
        *slot = *target;
      } else {
        slot.reset();
      }
    }
  }
  return true;
}
void mission_network::update() {
  if(!enabled) return;
  for(auto const &group : runtime.actors.groups) {
    size_t const index{group.units.front()};
    auto &unit{runtime.actors.units[index]};
    if(runtime.spatial.player == index || unit.network < 0 || unit.network >= static_cast<int32_t>(state.players.size()) || unit.destroyed == 1 || (unit.conditions & 0x400)) continue;
    auto &remote{state.players[static_cast<size_t>(unit.network)]};
    unit.speed = std::bit_cast<int32_t>(static_cast<uint32_t>(remote.aircraft.speed) * 432u);
    unit.object.world.rotation = remote.aircraft.location.rotation;
    runtime.spatial.move(unit, remote.aircraft.location.position);
    auto const &position{remote.aircraft.location.position};
    if(position[0] && position[1] && position[2] && remote.weapon) {
      auto const target{remote.weapon == 1 || remote.weapon == 19 ? std::nullopt : resolve(remote.shot_target)};
      if(runtime.weapons) runtime.weapons->fire_remote(index, remote.weapon, remote.shot_seed, remote.shot_target.data, target);
      remote.weapon = 0;
    }
  }
}
} // namespace apache::simulation
