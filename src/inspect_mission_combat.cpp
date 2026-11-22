#include "inspect_mission_combat.h"
#include <fstream>
#include <iomanip>
#include <istream>
#include <ostream>
#include <stdexcept>
#include "render/landscape.h"
#include "simulation/mission_crew.h"
#include "simulation/mission_instruments.h"
#include "simulation/mission_network.h"
#include "simulation/mission_runtime.h"

void inspect_mission_combat(std::istream &input, std::ostream &output, apache::resources::data_store const &data, std::string const &zone, std::string const &path, bool const armed, bool const hurt) {
  namespace math = apache::math;
  namespace multiplayer = apache::multiplayer;
  namespace render = apache::render;
  namespace resources = apache::resources;
  namespace simulation = apache::simulation;
  simulation::mission_runtime runtime{data, zone, path, armed, hurt};
  runtime.check_collisions = runtime.subdivide_flight = false;
  simulation::mission_network network{runtime}; network.enabled = true;
  std::unique_ptr<simulation::mission_crew> crew;
  auto &mission{runtime.mission}; auto &scene{runtime.scene}; auto &terrain{runtime.terrain}; auto &models{runtime.models}; auto &trig{runtime.trig};
  auto &actors{runtime.actors}; auto &spatial{runtime.spatial}; auto &mobiles{runtime.mobiles}; auto &structures{runtime.structures}; auto &targets{runtime.targets};
  auto &random{runtime.random}; auto &weapon_random{runtime.weapon_random}; auto &combat{runtime.combat}; auto &pilot{runtime.pilot}; auto &player_damage{runtime.player_damage};
  auto &launched{runtime.weapons}; auto &damage{runtime.damage}; auto &check_collisions{runtime.check_collisions}; auto &collision_marks{runtime.collision_marks}; auto &timing{runtime.timing}; auto &frame{runtime.frame};
  auto const identity{[&](simulation::target_identity const &id)->int64_t {
    if(id.kind == simulation::target_kind::scenery) return 1 + id.index + id.layer * 100000;
    if(id.kind == simulation::target_kind::mobile) {
      if(auto i{spatial.actor_index(id.index)}) return 200001 + static_cast<int64_t>(*i);
      if(launched) if(auto i{launched->projectile_index(id.index)}) return 300001 + static_cast<int64_t>(*i);
    }
    return 0;
  }};
  auto const decode_identity{[&](uint32_t const value)->simulation::target_identity {
    if(value >= 200001 && value < 300001) return {simulation::target_kind::mobile, 0, spatial.mobile_id(value - 200001)};
    if(value && value < 200001) return {simulation::target_kind::scenery, (value - 1) / 100000, (value - 1) % 100000};
    return {};
  }};
  auto const reference{[&](simulation::target_reference const *target){
    if(!target || target->identity.kind == simulation::target_kind::none) { output << " 0 0 0 0"; return; }
    auto const &t{target->identity};
    if(t.kind == simulation::target_kind::fixed) { output << " -1"; for(auto v : target->position) output << ' ' << v; } else output << ' ' << identity(t) << " 0 0 0";
  }};
  bool drawing{false};
  auto &events{runtime.events};
  events.waypoint = [&](size_t i){ output << "waypoint " << frame << ' ' << i << '\n'; };
  events.fire = [&](size_t i){ output << "fire " << frame << ' ' << i; reference(combat.armaments[i].target.get()); output << '\n'; };
  events.ground_fire = [&](auto const &source, auto const &target, unsigned int id){ output << "groundfire " << frame << ' ' << id; reference(&source); reference(&target); output << '\n'; };
  events.sound = [&](unsigned int sound, bool cockpit){ output << "sound " << (drawing && frame ? frame - 1 : frame) << ' ' << sound << ' ' << cockpit << '\n'; };
  events.message = [&](std::string_view text){ output << "message " << (drawing && frame ? frame - 1 : frame) << ' ' << std::quoted(std::string{text}) << '\n'; };
  events.launched = [&](size_t i, auto const &round){ output << "launch " << frame << ' ' << i << ' ' << round.state.weapon << '\n'; };
  events.projectile = [&](size_t i, auto &, auto const &info){ output << "event " << frame << ' ' << i << ' ' << static_cast<unsigned int>(info.kind); for(auto v : info.position) output << ' ' << v; output << ' ' << (info.target ? identity(*info.target) : 0) << '\n'; };
  events.killed = [&](auto const &target, auto const &source){ output << "kill " << frame << ' ' << identity(target) << ' ' << identity(source) << '\n'; };
  events.explosion_sound = [&](auto const &sound){ output << "explosion_sound " << frame << ' ' << sound.volume << ' ' << sound.pan << '\n'; };
  events.collision = [&](auto const &id){ output << "collisionkill " << frame << ' ' << identity(id) << '\n'; };
  events.impact_sound = [&](bool heavy){ if(heavy) output << "explosion_sound " << frame << " 63 32768\n"; else output << "metal_sound " << frame << '\n'; };
  events.radar_toggle = [&](unsigned int value){ output << "radar-toggle " << frame << ' ' << value << '\n'; };
  events.launch_sound = [&](unsigned int id){ output << "launch-sound " << frame << ' ' << id << '\n'; };
  events.incoming_sound = [&]{ output << "incoming-sound " << frame << '\n'; };
  events.gun_sound = [&](bool active){ output << "gun-sound " << frame << ' ' << active << '\n'; };
  events.warning_message = [&](size_t slot, bool added, std::string_view text){ output << "incoming-message " << frame << ' ' << slot << ' ' << added << ' ' << std::quoted(std::string{text}) << '\n'; };
  events.cycle_target = [&]{ output << "cycle-target " << frame << '\n'; };
  events.missile_camera = [&](size_t index){ output << "missile-camera " << frame << ' ' << index << '\n'; };
  auto const snapshot{[&]{
    if(runtime.player_armament) {
      auto const &state{*runtime.player_armament}; auto const &inventory{state.inventory};
      output << "armament " << inventory.current << ' ' << inventory.helmet_mode << ' ' << inventory.armed << ' ' << inventory.gun_fired;
      for(auto const &p : inventory.pylons) output << ' ' << p.weapon << ' ' << p.ammunition;
      for(auto v : inventory.ammunition) output << ' ' << v; for(auto v : inventory.selected) output << ' ' << v; for(auto v : inventory.deadlines) output << ' ' << v;
      output << ' ' << state.impacts.entries.size(); for(auto const &v : state.impacts.entries) output << ' ' << v.round << ' ' << v.deadline;
      auto const *mount{actors.units[*timing.player].object.children[0].get()}; auto const *barrel{mount ? mount->children[0].get() : nullptr};
      output << ' ' << state.gun_fired_last << ' ' << (barrel && !barrel->children.empty() && barrel->children[0] ? barrel->children[0]->id : 0) << '\n';
    }
    if(runtime.sensors) {
      auto const &state{*runtime.sensors};
      output << "sensors " << state.state.automatic << ' ' << state.state.cycle_direction << ' ' << state.state.laser; reference(&state.state.current);
      for(auto v : state.current_display) output << ' ' << v;
      for(auto const *list : {&state.ground, &state.mobile, &state.missiles}) { output << ' ' << list->size(); for(auto const &entry : *list) { output << ' ' << identity(entry.target.identity); for(auto v : entry.position) output << ' ' << v; } }
      output << ' ' << state.ranked.size(); for(auto const &entry : state.ranked) output << ' ' << identity(entry.target.identity) << ' ' << entry.priority;
      output << '\n';
    }
    if(pilot) {
      if(check_collisions) output << "contact " << pilot->collision_model << ' ' << (pilot->hit_actor ? static_cast<int64_t>(*pilot->hit_actor) : -1) << '\n';
      auto const &readings{pilot->readings()}; output << "flight " << pilot->on_carrier() << ' ' << pilot->landed() << ' ' << pilot->death() << ' ' << (pilot->arcade.power.faults | pilot->realistic.dynamics.power.faults);
      for(auto v : readings.angles) output << ' ' << v;
      output << ' ' << readings.airspeed << ' ' << readings.sidespeed << ' ' << readings.climb << ' ' << readings.torque << ' ' << readings.fuel;
      for(auto v : readings.engines) output << ' ' << static_cast<int32_t>(v); output << '\n';
    }
    if(player_damage) { auto const &state{player_damage->state}; output << "player-damage " << pilot->faults() << ' ' << state.random << ' ' << state.broken_mfds << ' ' << state.warning_at << ' ' << identity(player_damage->killer) << '\n'; }
    output << "snapshot " << (frame ? frame - 1 : 0) << ' ' << random.value << ' ' << weapon_random.value << ' ' << random.global_seed << ' ' << combat.warnings.ships_attacked << ' ' << combat.warnings.radar << ' ' << combat.warnings.bandit << ' ' << combat.bombing_message_time << '\n';
    for(size_t i{0}; i != actors.units.size(); ++i) {
      auto const &a{actors.units[i]}; auto const &arm{combat.armaments[i]}; auto const &attention{combat.attention[i]};
      output << "actor " << i << ' ' << a.serial << ' ' << (a.conditions & ~0x8000u) << ' ' << a.object.status;
      for(auto v : a.object.world.position) output << ' ' << v; for(auto v : a.object.world.rotation) output << ' ' << v; for(auto v : a.angles) output << ' ' << v; for(auto v : a.velocity) output << ' ' << v;
      output << ' ' << a.speed << ' ' << (a.waypoint ? static_cast<int64_t>(*a.waypoint) : -1) << ' ' << a.destroyed << ' ' << a.desired_speed << ' ' << a.speed_ramp_target << ' ' << a.retreat_until << ' ' << a.weapon << ' ' << a.weapon_slot;
      for(auto v : a.ammunition) output << ' ' << v; for(auto v : arm.deadlines) output << ' ' << v;
      output << ' ' << arm.ground_maximum << ' ' << arm.ground_minimum << ' ' << arm.air_maximum << ' ' << arm.air_minimum << ' ' << attention.range << ' ' << static_cast<unsigned int>(attention.damage) << ' ' << static_cast<unsigned int>(attention.processed_damage) << ' ' << a.random;
      reference(arm.target.get()); output << '\n';
    }
    if(launched) {
      for(size_t i{0}; i != actors.units.size(); ++i) { auto const &d{launched->decoys[i]}; output << "decoys " << i << ' ' << d.deadlines[0] << ' ' << d.deadlines[1]; for(auto const &target : d.targets) reference(target.get()); output << ' ' << (combat.armaments[i].round ? static_cast<int64_t>(*combat.armaments[i].round) : -1) << '\n'; }
      output << "rounds " << launched->rounds.active_count() << '\n';
      auto const definitions{simulation::weapon_definitions()};
      for(size_t i{0}; i != launched->rounds.entries().size(); ++i) if(launched->rounds.entries()[i]) {
        auto &round{launched->rounds.at(i)}; auto const &state{round.state}; auto const &body{state.body}; auto const kind{definitions[state.weapon].movement_class};
        output << "round " << i << ' ' << state.weapon << ' ' << state.launched_at << ' ' << state.random << ' ' << body.motion.flags;
        if(kind == 3) { for(auto v : state.impact) output << ' ' << v; output << ' ' << state.impact_at; } else {
          output << ' ' << body.motion.distance << ' ' << body.motion.speed;
          for(auto v : body.pose.position) output << ' ' << v; for(auto v : body.pose.rotation) output << ' ' << v; for(auto v : body.previous) output << ' ' << v; for(auto v : body.angles) output << ' ' << v;
          output << ' ' << (kind == 2 ? body.seeker.flags : 0) << ' ' << (kind == 2 ? body.seeker.retry_time : 0);
          auto const &children{mobiles.at(*round.mobile).children}; output << ' ' << (children.empty() || !children[0] ? 0 : children[0]->id);
          reference(kind == 2 ? simulation::adjusted_projectile_target(round, targets.world, {}) : nullptr);
        }
        output << '\n';
      }
      resources::bytes smoke; size_t count{0};
      auto const word{[&](uint32_t value){ for(unsigned int j{0}; j != 4; ++j) smoke.push_back(static_cast<uint8_t>(value >> (j * 8))); }};
      for(size_t i{0}; i != launched->visuals.smoke_puffs.size(); ++i) {
        auto const &puff{launched->visuals.smoke_puffs[i]}; if(!puff.active) continue; ++count;
        word(static_cast<uint32_t>(i)); word(1); for(auto v : puff.position) word(static_cast<uint32_t>(v)); word(static_cast<uint32_t>(puff.radius)); word(static_cast<uint32_t>(puff.lifetime)); word(static_cast<uint32_t>(puff.age)); word(puff.angle);
      }
      output << "smoke " << count << ' ' << resources::fingerprint(smoke) << '\n';
    }
    if(damage) {
      resources::bytes raw; size_t count{0};
      auto const word{[&](uint32_t value){ for(unsigned int j{0}; j != 4; ++j) raw.push_back(static_cast<uint8_t>(value >> (j * 8))); }};
      auto const vector{[&](math::vector3 const &v){ for(auto x : v) word(static_cast<uint32_t>(x)); }};
      for(size_t i{0}; i != launched->visuals.explosions.size(); ++i) {
        auto const &e{launched->visuals.explosions[i]}; if(!e.kind) continue;
        ++count; word(static_cast<uint32_t>(i)); word(e.kind); vector(e.position); vector(e.velocity); word(static_cast<uint32_t>(e.radius)); word(static_cast<uint32_t>(e.frame));
      }
      for(size_t i{0}; i != 256; ++i) {
        auto const &c{i < 128 ? damage->marks.short_lived[i] : damage->marks.long_lived[i - 128]}; if(c.model < 0) continue;
        ++count; word(static_cast<uint32_t>(i + 128)); vector(c.position); word(static_cast<uint32_t>(c.model)); word(c.heading); word(static_cast<uint32_t>(c.age)); vector(c.scale);
      }
      output << "effects " << count << ' ' << resources::fingerprint(raw) << '\n';
      raw.clear();
      for(auto const &layer : {std::cref(scene.base), std::cref(scene.extra)}) for(auto const &object : layer.get()) { word(object.model); word(object.heading); word(object.status); }
      output << "scenery " << resources::fingerprint(raw) << '\n';
    }
    output << "alive"; for(auto v : combat.alive) output << ' ' << v; output << '\n';
    for(size_t i{0}; i != scene.objects.size(); ++i) { auto const &g{scene.objects[i].combat}; output << "defence " << i << ' ' << g.heading << ' ' << g.deadline << ' ' << g.shots; reference(g.target.get()); output << '\n'; }
    for(size_t i{0}; i != combat.battles.size(); ++i) { output << "battle " << i << ' ' << combat.battles[i].active; for(auto const &side : combat.battles[i].sides) output << ' ' << (side.task ? static_cast<int64_t>(*side.task) : -1) << ' ' << side.initial << ' ' << side.release_time; output << '\n'; }
    output << "patrol " << combat.patrols.last_scan << ' ' << combat.patrols.visible[0].size() << ' ' << combat.patrols.visible[1].size(); for(auto const &list : combat.patrols.visible) for(auto const &t : list) reference(t.get()); for(auto const &list : combat.patrols.assigned) for(auto const &t : list) reference(t.get()); output << '\n';
  }};
  std::unique_ptr<render::landscape> renderer;
  std::array<std::unique_ptr<simulation::mission_instruments>, 2> instruments;
  simulation::flight_input controls; std::string command;
  while(input >> command) {
    if(command == "mode") input >> timing.arcade >> timing.game_flow;
    else if(command == "pilot") {
      size_t index{0}; simulation::mission_flight_settings settings; input >> index >> settings.arcade >> settings.fuel >> settings.flow >> settings.faults; for(auto &id : settings.pods) input >> id;
      runtime.start_player(index, settings);
    } else if(command == "loadout") { std::array<int32_t, 9> loadout; for(auto &v : loadout) input >> v; runtime.equip_player(loadout); } else if(command == "sensors") { unsigned int game_mode{0}; input >> game_mode; runtime.enable_player_sensors(game_mode); } else if(command == "sensor-mode") { auto &state{runtime.sensors->state}; input >> state.tads >> state.automatic >> state.ground >> state.air >> state.frozen >> state.scan_all >> state.threat; } else if(command == "sensor-view") {
      math::pose view; for(auto &v : view.position) input >> v; for(auto &v : view.rotation) input >> v;
      drawing = true; runtime.look_player(view, render::projection{0, 0, 320, 240}); drawing = false;
      auto const &state{*runtime.sensors}; output << "sensor-view " << state.state.locked << ' ' << state.state.tads; reference(&state.state.current); reference(&state.optical); output << ' ' << state.optical_symbol.has_value();
      if(state.optical_symbol) for(auto v : *state.optical_symbol) output << ' ' << v; output << '\n';
    } else if(command == "network-assign" || command == "network-revive") {
      uint32_t connection, model; size_t task; unsigned int mode, map; int flow; bool result;
      input >> connection >> model;
      if(command == "network-assign") {
        input >> task >> mode >> flow >> map >> network.enabled;
        result = network.assign(connection, model, task, static_cast<multiplayer::game_mode>(mode), flow, map);
      } else result = network.revive(connection, static_cast<int32_t>(model));
      output << "assignment " << result;
      if(result) {
        auto const index{*network.actor(connection)}; auto const &unit{actors.units[index]}; auto const &arm{combat.armaments[index]};
        output << ' ' << index << ' ' << network.state.players[connection].flags << ' ' << unit.network << ' ' << unit.object.id << ' ' << unit.object.status << ' ' << unit.classification << ' ' << unit.conditions;
        for(auto v : unit.object.world.position) output << ' ' << v; for(auto v : unit.object.world.rotation) output << ' ' << v;
        for(auto v : unit.angles) output << ' ' << v;
        output << ' ' << unit.speed << ' ' << unit.route_distance << ' ' << unit.weapon << ' ' << unit.weapon_slot;
        for(auto v : unit.ammunition) output << ' ' << v;
        output << ' ' << arm.ground_maximum << ' ' << arm.ground_minimum << ' ' << arm.air_maximum << ' ' << arm.air_minimum;
        output << ' ' << unit.object.children.size(); for(auto const &child : unit.object.children) output << ' ' << (child ? child->id : 0);
        output << ' ' << unit.destroyed << ' ' << static_cast<unsigned int>(combat.attention[index].damage) << ' ' << static_cast<unsigned int>(combat.attention[index].processed_damage);
      }
      output << '\n';
    } else if(command == "network-remove-unassigned") network.remove_unassigned();
    else if(command == "network-spatial") {
      resources::bytes mobile_bytes, war_bytes, actor_bytes;
      auto const word{[](resources::bytes &bytes, uint32_t value){ for(unsigned int j{0}; j < 4; ++j) bytes.push_back(static_cast<uint8_t>(value >> (j * 8))); }};
      auto const contact{[&](simulation::shared_target const &target)->uint32_t {
        if(target && target->identity.kind == simulation::target_kind::mobile) if(auto index{spatial.actor_index(target->identity.index)}) return static_cast<uint32_t>(*index + 1);
        return 0;
      }};
      for(size_t sector{0}; sector < 4096; ++sector) {
        auto const list{mobiles.sector(sector)}; word(mobile_bytes, static_cast<uint32_t>(list.size()));
        for(auto id : list) word(mobile_bytes, static_cast<uint32_t>(*spatial.actor_index(id) + 1));
        for(unsigned int side{1}; side <= 2; ++side) {
          auto const *cell{spatial.grid.sector(static_cast<int32_t>(sector / 64), static_cast<int32_t>(sector % 64), side)};
          word(war_bytes, cell != nullptr);
          if(cell) for(auto const *bucket : {&cell->air, &cell->ground}) {
            word(war_bytes, bucket->count); for(auto const &target : bucket->targets) word(war_bytes, contact(target));
          }
        }
      }
      for(auto const &unit : actors.units) { word(actor_bytes, unit.object.status); word(actor_bytes, static_cast<uint32_t>(unit.destroyed)); }
      output << "network-spatial " << resources::fingerprint(mobile_bytes) << ' ' << resources::fingerprint(war_bytes) << ' ' << resources::fingerprint(actor_bytes);
      for(auto value : spatial.grid.counts()) output << ' ' << value;
      for(auto value : spatial.grid.peak_counts()) output << ' ' << value;
      output << '\n';
    } else if(command == "crew-role") {
      unsigned int role; input >> role; network.state.seat = static_cast<multiplayer::crew_role>(role); runtime.player_armament->control_mode = role;
      if(!crew) {
        crew = std::make_unique<simulation::mission_crew>(runtime, network);
        crew->key = [&](uint16_t key){ output << "crew-key " << key << ' ' << runtime.player_armament->control_mode << '\n'; };
        crew->landing = [&](uint8_t grade){ output << "crew-landing " << static_cast<unsigned int>(grade) << '\n'; };
        crew->radio = [&](std::string_view text, uint8_t sample){ output << "crew-radio " << std::quoted(std::string{text}) << ' ' << static_cast<unsigned int>(sample) << '\n'; };
      }
    } else if(command == "crew-condition") {
      unsigned int death; int32_t destroyed; bool invulnerable; input >> death >> destroyed >> invulnerable;
      pilot->realistic.death = pilot->arcade.death = death; pilot->realistic.invulnerable = pilot->arcade.invulnerable = invulnerable;
      actors.units[*runtime.spatial.player].destroyed = destroyed;
    } else if(command == "crew-receive") {
      std::string hex; input >> hex; resources::bytes bytes;
      for(size_t i{0}; i < hex.size(); i += 2) bytes.push_back(static_cast<uint8_t>(std::stoul(hex.substr(i, 2), nullptr, 16)));
      auto const role{network.state.seat == multiplayer::crew_role::pilot ? multiplayer::crew_role::gunner : multiplayer::crew_role::pilot};
      auto const value{multiplayer::decode_crew(role, bytes)}; if(!value || !crew) throw std::runtime_error{"Invalid shared cockpit record"};
      crew->receive(*value);
    } else if(command == "crew-transmit") {
      unsigned int ticks; input >> ticks; auto const message{crew->transmit(ticks)};
      output << "crew-transmit "; for(auto b : message.bytes().subspan(24, 104)) output << std::hex << std::setfill('0') << std::setw(2) << static_cast<unsigned int>(b);
      output << std::dec << '\n';
    } else if(command == "crew-state") {
      auto const &f{pilot->readings()}; auto const &target{runtime.sensors->state.current};
      output << "crew-state"; for(auto v : f.angles) output << ' ' << v;
      for(auto v : f.position.position) output << ' ' << v; for(auto v : f.position.rotation) output << ' ' << v;
      output << ' ' << f.climb << ' ' << f.fuel << ' ' << f.torque << ' ' << f.sidespeed << ' ' << f.airspeed << ' ' << std::setprecision(17) << pilot->model_weight << ' ' << pilot->power().endurance;
      for(auto v : f.engines) output << ' ' << v;
      output << ' ' << pilot->landed() << ' ' << static_cast<unsigned int>(crew->next_frame_ticks) << ' ' << crew->leaving << ' ' << runtime.player_armament->control_mode << ' ' << pilot->death();
      output << ' ' << static_cast<unsigned int>(target.identity.kind) << ' ' << identity(target.identity);
      for(auto v : target.position) output << ' ' << v;
      output << ' ' << static_cast<uint32_t>(target.classification) << ' ' << runtime.player_armament->mounts.hydra_elevation;
      for(auto v : runtime.sensors->current_display) output << ' ' << v;
      output << '\n';
    } else if(command == "network-maintain") { for(auto &actor : actors.units) { actor.object.relative = actor.object.world; render::maintain_model(actor.object); } } else if(command == "network-context") {
      unsigned int mode;
      input >> network.state.local >> mode >> network.lifecycle.following >> network.lifecycle.other_players;
      network.state.active = true; network.lifecycle.mode = static_cast<multiplayer::game_mode>(mode);
      if(damage) damage->environment.mode = static_cast<int32_t>(mode);
      for(auto &player : network.state.players) player.aircraft.model = 64;
      network.remote_killed = [&](auto const &target, uint8_t source){ output << "remote-kill " << identity(target) << ' ' << static_cast<unsigned int>(source) << '\n'; };
    } else if(command == "network-owner") { size_t index; input >> index; if(index >= actors.units.size()) throw std::runtime_error{"Invalid network owner"}; spatial.player = index; } else if(command == "network-output") {
      input >> network.state.players.at(network.state.local).name;
      network.send = [&](auto const &message){
        output << "network-send ";
        for(auto byte : message.bytes()) output << std::hex << std::setfill('0') << std::setw(2) << static_cast<unsigned int>(byte);
        output << std::dec << std::setfill(' ') << '\n';
      };
    } else if(command == "network-source-flags") {
      uint32_t code, flags; input >> code >> flags; auto const id{decode_identity(code)};
      if(id.kind == simulation::target_kind::mobile) mobiles.at(id.index).status = flags;
      else if(id.kind == simulation::target_kind::scenery) (id.layer ? scene.extra : scene.base).at(id.index).status = static_cast<uint16_t>(flags);
    } else if(command == "network-score-context") {
      input >> network.state.active >> network.state.client_server >> network.state.server;
      for(auto &player : network.state.players) input >> player.score;
      events.notice = [&](auto text, auto priority, auto lifetime){ output << "network-notice " << std::quoted(std::string{text}) << ' ' << priority << ' ' << lifetime << '\n'; };
    } else if(command == "network-record-kill") { uint32_t target, source; input >> target >> source; network.record_kill(decode_identity(target), decode_identity(source)); } else if(command == "network-player-killed") { uint32_t source; input >> source; network.player_killed(decode_identity(source)); } else if(command == "network-send-casualties") network.send_casualties();
    else if(command == "network-receive") {
      int32_t now; std::string hex; input >> now >> hex; resources::bytes bytes;
      for(size_t i{0}; i < hex.size(); i += 2) bytes.push_back(static_cast<uint8_t>(std::stoul(hex.substr(i, 2), nullptr, 16)));
      auto const message{multiplayer::packet::decode(bytes)}; if(!message) throw std::runtime_error{"Invalid mission network packet"};
      if(launched) launched->environment.now = now;
      targets.now = now; network.receive(*message, now);
    } else if(command == "network-fragments") {
      for(size_t i{0}; i < runtime.debris.pieces.size(); ++i) {
        auto const &piece{runtime.debris.pieces[i]}; if(!piece.active) continue;
        output << "network-fragment " << i << " 1 " << piece.model << ' ' << piece.flags;
        for(auto v : piece.position.position) output << ' ' << static_cast<uint32_t>(v);
        for(auto v : piece.position.rotation) output << ' ' << static_cast<uint32_t>(v);
        for(auto v : piece.axis) output << ' ' << static_cast<uint32_t>(v);
        output << ' ' << static_cast<uint32_t>(piece.spin);
        for(auto v : piece.velocity) output << ' ' << static_cast<uint32_t>(v);
        output << '\n';
      }
    } else if(command == "network-summary") {
      resources::bytes actor_data, fragment_data;
      auto const word{[](resources::bytes &bytes, uint32_t value){ for(unsigned int i{0}; i < 4; ++i) bytes.push_back(static_cast<uint8_t>(value >> (8 * i))); }};
      for(auto const &unit : actors.units) {
        for(auto value : {unit.object.id, unit.object.status, static_cast<uint32_t>(unit.destroyed)}) word(actor_data, value);
        for(auto value : unit.object.world.position) word(actor_data, static_cast<uint32_t>(value));
        for(auto value : unit.object.world.rotation) word(actor_data, static_cast<uint32_t>(value));
        for(auto value : unit.velocity) word(actor_data, static_cast<uint32_t>(value)); word(actor_data, static_cast<uint32_t>(unit.speed));
        word(actor_data, static_cast<uint32_t>(unit.object.children.size()));
        for(auto const &child : unit.object.children) word(actor_data, child ? child->id : 0);
      }
      size_t count{0};
      for(size_t i{0}; i < runtime.debris.pieces.size(); ++i) {
        auto const &piece{runtime.debris.pieces[i]}; if(!piece.active) continue; ++count;
        word(fragment_data, static_cast<uint32_t>(i)); word(fragment_data, 1); word(fragment_data, piece.model); word(fragment_data, piece.flags);
        for(auto v : piece.position.position) word(fragment_data, static_cast<uint32_t>(v));
        for(auto v : piece.position.rotation) word(fragment_data, static_cast<uint32_t>(v));
        for(auto v : piece.axis) word(fragment_data, static_cast<uint32_t>(v));
        word(fragment_data, static_cast<uint32_t>(piece.spin));
        for(auto v : piece.velocity) word(fragment_data, static_cast<uint32_t>(v));
      }
      output << "network-summary " << network.state.active << ' ' << network.lifecycle.following << ' ' << network.lifecycle.other_players << ' ' << random.global_seed << ' ' << resources::fingerprint(actor_data) << ' ' << count << ' ' << resources::fingerprint(fragment_data);
      for(auto value : combat.alive) output << ' ' << value; output << '\n';
    } else if(command == "network-target") {
      multiplayer::target_state request; input >> request.kind; for(auto &v : request.data) input >> v;
      auto const result{network.resolve(request)}; output << "network-target " << (result ? identity(result->identity) : 0) << '\n';
    } else if(command == "network-bind") {
      uint32_t connection; size_t index; input >> connection >> index;
      if(!network.bind(connection, index)) throw std::runtime_error{"Invalid remote actor binding"};
    } else if(command == "network-pose") {
      size_t connection; input >> connection; auto &remote{network.state.players.at(connection)};
      input >> remote.aircraft.speed; for(auto &v : remote.aircraft.location.position) input >> v; for(auto &v : remote.aircraft.location.rotation) input >> v;
      input >> remote.weapon >> remote.shot_seed >> remote.shot_target.kind; for(auto &v : remote.shot_target.data) input >> v;
    } else if(command == "network-update") {
      int32_t now; input >> now; launched->environment.now = targets.now = now; network.update();
    } else if(command == "remote-fire") {
      size_t index; unsigned int weapon; uint32_t seed; int32_t now, target_index; math::vector3 direction;
      input >> index >> weapon >> seed >> now >> target_index; for(auto &v : direction) input >> v;
      std::optional<simulation::target_reference> target;
      if(target_index >= 0) target = *spatial.targets.at(static_cast<size_t>(target_index));
      launched->environment.now = targets.now = now;
      launched->fire_remote(index, weapon, seed, direction, target);
    } else if(command == "player-target") { size_t index{0}; input >> index; *runtime.player_armament->target = *spatial.targets.at(index); } else if(command == "player-weapon") { input >> runtime.player_armament->inventory.current >> runtime.player_armament->inventory.armed; } else if(command == "player-fire") { int32_t now{0}; double dt{0}; unsigned int key{0}; input >> now >> dt >> key; bool result{runtime.fire_player(now, dt, static_cast<uint16_t>(key))}; output << "player-fired " << frame << ' ' << result << '\n'; } else if(command == "player-aim") {
      int32_t now{0}, delta{0}, hydra{0}; input >> now >> delta >> hydra; auto &state{*runtime.player_armament}; for(auto &v : state.helmet) input >> v;
      drawing = true; state.prepare(now, delta, hydra); drawing = false; output << "player-aim " << state.mounts.hydra_elevation << ' ' << state.mounts.hydras_locked << ' ' << state.sight.status << '\n';
    } else if(command == "player-damage-state") {
      if(!player_damage) throw std::runtime_error{"Player damage is not active"};
      auto &state{player_damage->state}; input >> state.random >> state.broken_mfds >> state.warning_at >> state.ground_radar >> state.air_radar;
    } else if(command == "player-hit") {
      if(!player_damage) throw std::runtime_error{"Player damage is not active"};
      unsigned int amount{0}; input >> amount; combat.attention[*timing.player].damage = static_cast<uint8_t>(amount); player_damage->apply(simulation::damage_action::player_systems, {}, targets.now);
    } else if(command == "subdivide") input >> runtime.subdivide_flight;
    else if(command == "collisions") input >> check_collisions;
    else if(command == "settle-carrier") {
      if(!pilot) throw std::runtime_error{"A carrier test requires a player"};
      pilot->advance(0.05, 107374182, {}, 0);
      output << "settled " << pilot->on_carrier() << ' ' << pilot->landed() << ' ' << pilot->death();
      for(auto v : pilot->readings().position.position) output << ' ' << v; for(auto v : pilot->readings().position.rotation) output << ' ' << v; for(auto v : pilot->readings().angles) output << ' ' << v; output << '\n';
    } else if(command == "contact-test") {
      if(!pilot) throw std::runtime_error{"A contact test requires a player"};
      math::pose pose; math::vector3 previous; std::array<uint32_t, 3> angles; double forward{0}, vertical{0}; bool landed{false}; unsigned int cause{0};
      for(auto &v : pose.position) input >> v; for(auto &v : pose.rotation) input >> v; for(auto &v : previous) input >> v; for(auto &v : angles) input >> v; input >> forward >> vertical >> landed >> cause;
      auto &actor{actors.units[*timing.player]}; actor.conditions &= ~0x400u;
      pilot->realistic.on_carrier = pilot->arcade.on_carrier = false; pilot->realistic.landed = pilot->arcade.landed = landed; pilot->realistic.death = pilot->arcade.death = cause;
      pilot->realistic.readings.position = pilot->arcade.readings.position = pose; pilot->realistic.readings.angles = pilot->arcade.readings.angles = angles;
      pilot->realistic.dynamics.u = pilot->arcade.forward_velocity = forward; pilot->realistic.dynamics.zdot = vertical; pilot->arcade.delta_vertical_velocity = vertical / 72;
      actor.object.world.rotation = pose.rotation; spatial.move(actor, pose.position); render::maintain_model(actor.object);
      pilot->collide(previous, structures, mobiles, damage ? damage->marks : collision_marks, [&](auto const &id){ if(damage) damage->destroy(id); else output << "collisionkill " << frame << ' ' << identity(id) << '\n'; });
      output << "response " << pilot->collision_model << ' ' << (pilot->hit_actor ? static_cast<int64_t>(*pilot->hit_actor) : -1) << ' ' << pilot->landed() << ' ' << pilot->death() << ' ' << pilot->on_carrier() << ' ' << (actor.carrier ? static_cast<int64_t>(*actor.carrier) : -1) << ' ' << (actor.carrier_slot ? static_cast<int64_t>(*actor.carrier_slot) : -1);
      for(auto v : pilot->readings().position.position) output << ' ' << v; for(auto v : pilot->readings().position.rotation) output << ' ' << v; for(auto v : pilot->readings().angles) output << ' ' << v;
      for(auto const &carrier : actors.carriers) for(auto const &slot : carrier.slots) output << ' ' << (slot.reserved ? static_cast<int64_t>(*slot.reserved) : -1);
      output << '\n';
    } else if(command == "controls") { for(auto &v : controls.controls) input >> v; input >> controls.absolute_throttle >> controls.type; } else if(command == "view") { for(auto &v : combat.player_position) input >> v; input >> combat.player_heading; } else if(command == "kill") { size_t i{0}; input >> i; combat.kill(i); } else if(command == "damage") { size_t i{0}; unsigned int amount{0}; input >> i >> amount; combat.attention.at(i).damage = static_cast<uint8_t>(amount); } else if(command == "instruments") {
      unsigned int mode{0}, view{0}; math::pose camera; input >> mode >> view; for(auto &v : camera.position) input >> v; for(auto &v : camera.rotation) input >> v;
      auto &display{instruments.at(mode)}; if(!display) display = std::make_unique<simulation::mission_instruments>(runtime, mode);
      display->update(view, camera, render::projection{0, 0, 320, 240}); auto const &s{display->state}; auto const &h{s.helmet}; auto const &w{h.weapons};
      output << "instrument-flight " << h.heading << ' ' << h.command_heading << ' ' << h.pitch << ' ' << h.roll << ' ' << h.climb_rate << ' ' << h.radar_altitude << ' ' << h.barometric_altitude << ' ' << h.airspeed << ' ' << h.torque << ' ' << static_cast<unsigned int>(h.waypoint_letter) << ' ' << h.waypoint_distance << ' ' << h.waypoint_time << ' ' << s.clock_time << '\n';
      output << "instrument-weapon " << w.selected << ' ' << w.rounds << ' ' << w.impact_time << ' ' << w.lock_status << ' ' << w.armed << ' ' << w.landed << ' ' << w.target_exists << ' ' << w.target_locked << ' ' << w.target_designated << ' ' << w.target_aircraft << ' ' << w.target_valid << ' ' << (w.target_valid ? w.target.x : 0) << ' ' << (w.target_valid ? w.target.y : 0) << ' ' << w.impact.x << ' ' << w.impact.y << ' ' << w.intercept.x << ' ' << w.intercept.y << ' ' << w.target_range << ' ' << w.target_speed << ' ' << w.box_half_size << ' ' << w.rocket_elevation << ' ' << w.tads_active << ' ' << w.tads_valid << ' ' << (w.tads_valid ? w.tads.x : 0) << ' ' << (w.tads_valid ? w.tads.y : 0) << ' ' << h.faults << ' ' << h.mode << ' ' << std::quoted(w.name) << ' ' << std::quoted(w.target_name) << '\n';
      output << "instrument-stores"; for(auto const &p : s.stores.pairs) output << ' ' << p.type << ' ' << p.rounds[0] << ' ' << p.rounds[1]; for(auto v : s.stores.wingtip) output << ' ' << v;
      output << ' ' << s.stores.gun << ' ' << s.stores.chaff << ' ' << s.stores.flares << ' ' << s.stores.armed << ' ' << s.stores.auto_chaff << ' ' << s.stores.auto_ecm << '\n';
      for(bool tactical : {false, true}) {
        auto const &r{tactical ? s.tactical : s.radar}; auto const &contacts{tactical ? s.tactical_contacts : s.radar_contacts};
        output << "instrument-radar " << static_cast<unsigned int>(r.mode) << ' ' << r.range << ' ' << r.corner_range << ' ' << r.mark_length << ' ' << r.heading << ' ' << r.failed << ' ' << r.frozen << ' ' << r.tads << ' ' << r.locked << ' ' << r.scan_all << ' ' << r.threat << ' ' << r.range_index << ' ' << r.has_target << ' ' << std::quoted(r.target_name) << ' ' << r.target_distance << ' ' << contacts.size();
        for(auto const &c : contacts) output << ' ' << c.x << ' ' << c.y << ' ' << c.symbol << ' ' << c.selected; output << '\n';
      }
      output << "instrument-route " << s.route.size() << ' ' << s.waypoint; for(auto const &p : s.route) output << ' ' << p.x << ' ' << p.y << ' ' << static_cast<unsigned int>(p.label); output << '\n';
    } else if(command == "render-frame") {
      std::string path; math::pose camera; int hidden{-1}; input >> std::quoted(path) >> hidden;
      for(auto &v : camera.position) input >> v; for(auto &v : camera.rotation) input >> v;
      if(!renderer) {
        renderer = std::make_unique<render::landscape>(data, zone); renderer->initialise_model_metadata(runtime.executable);
        renderer->bind_scene(scene.base, scene.extra, mobiles, launched ? launched->visuals : renderer->effects, damage ? damage->marks : renderer->craters, renderer->fragments);
      }
      render::landscape_options options; if(hidden >= 0) options.hidden_mobile = spatial.mobile_id(static_cast<size_t>(hidden));
      render::framebuffer target{320, 240}; target.clear(255); renderer->draw(target, camera, options);
      resources::write_ppm(path, target.image); std::ofstream pixels{path + ".pixels", std::ios::binary}; pixels.write(reinterpret_cast<char const *>(target.image.pixels.data()), static_cast<std::streamsize>(target.image.pixels.size()));
      output << "render " << std::quoted(path) << ' ' << renderer->missing_normals << ' ' << renderer->invalid_texture_samples << '\n';
    } else if(command == "snapshot") snapshot();
    else if(command == "step") {
      bool capture{false}; int32_t now{0}, delta{0}; input >> now >> delta >> capture;
      runtime.advance(now, delta, controls, []{ return 0; });
      if(capture) snapshot();
    } else throw std::runtime_error{"Unknown mission combat command"};
    if(!input) throw std::runtime_error{"Incomplete mission combat command"};
  }
}
