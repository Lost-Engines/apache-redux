#include "mission_runtime.h"
#include "quick_start.h"
#include <stdexcept>

namespace apache::simulation {
namespace {
unsigned int map_number(std::string const &zone) {
  if(zone == "YEMEN") return 1;
  if(zone == "KOREA") return 2;
  if(zone == "CYPRUS") return 3;
  if(zone == "FORT") return 4;
  throw std::runtime_error{"Unknown mission warzone"};
}
resources::mission_definition load_mission(resources::data_store const &data, std::string const &path) {
  auto const bytes{data.read(path)};
  return resources::read_mission_definition(std::string{bytes.begin(), bytes.end()});
}
resources::model_archive load_models(resources::data_store const &data, resources::executable_data const &executable) {
  auto models{resources::read_models(data.read("3DDATA/3DOBJS.BIN"))};
  resources::initialise_model_metadata(models, executable); return models;
}
}
mission_actors mission_runtime::make_actors(unsigned int const map) {
  auto const high{(random.global_seed = random.global_seed * 0x41c64e6du + 0x3039u) & 0x7fff0000u};
  random.global_seed = random.global_seed * 0x41c64e6du + 0x3039u;
  random.value = high | ((random.global_seed >> 16) & 0x7fff);
  mission_actors result{mission, formations, models, trig, environment, map, random.global_seed};
  random.global_seed = result.random_seed; return result;
}
mission_combat_callbacks mission_runtime::combat_callbacks() {
  mission_combat_callbacks result;
  result.visible = [this](auto const &source, auto const &target){ return line_reaches_target(targets.world.position(source), targets.world.position(target), source.identity, target.identity, terrain, structures, mobiles, models, trig); };
  result.fire = [this](size_t i){ if(events.fire) events.fire(i); if(weapons) weapons->fire(i); };
  result.ground_fire = [this](auto const &source, auto const &target, unsigned int id){ if(events.ground_fire) events.ground_fire(source, target, id); return weapons ? weapons->fire_ground(source, target, id) : firing_status::blocked; };
  result.sound = [this](unsigned int id, bool cockpit){ if(events.sound) events.sound(id, cockpit); };
  result.message = [this](std::string_view text){ if(events.message) events.message(text); if(events.notice) events.notice(text, 1, 400); };
  return result;
}
movement_callbacks mission_runtime::movement_events() {
  return {.check_air_target{[this](size_t i){ combat.check_air_lock(i); }}, .target{[this](size_t i){ return combat.movement_aim(i); }}, .kill{[this](size_t i){ combat.kill(i); }}, .fire_at_waypoint{[this](size_t i){ combat.fire_at_waypoint(i); }}, .place{[this](size_t i){ spatial.place(i); }}, .unplace{[this](size_t i){ spatial.unplace(i); }}, .weapon{[this](size_t i){ return combat.weapon(i); }}};
}
mission_runtime::mission_runtime(resources::data_store const &data, std::string const &zone, std::string const &path, bool const armed, bool const hurt, int const difficulty, mission_random_state const seeds, resources::mission_definition const *edited)
  : mission{edited ? *edited : load_mission(data, path)}, executable{data}, terrain{data.read(zone + ".WZ/GRIDPOST.BIN")}, models{load_models(data, executable)},
    scene{mission, resources::read_scenery(data.read(zone + ".WZ/GNDOBJS.BIN")), terrain, models, map_number(zone)}, features{data, zone, false}, super_features{data, zone, true}, trig{data}, formations{resources::read_formations(executable)},
    environment{.height{[this](int32_t x, int32_t y){ return terrain.elevation(x, y); }}, .surface{[this](int32_t x, int32_t y){ return resources::surface_type(features, super_features, x, y); }}, .relocate{}},
    random{1, seeds.global}, actors{make_actors(map_number(zone))}, mobiles{models}, spatial{mission, scene, actors, mobiles, terrain}, structures{models, trig, scene.base, scene.extra},
    navigation{.height{environment.height}, .tallest_object{structures.tallest_object}, .obstacle{[this](auto const &from, auto const &to) -> std::optional<navigation_obstacle> { auto hit{structures.line(from, to)}; if(!hit) return {}; return navigation_obstacle{hit->position, models.at(structures.at(hit->layer, hit->index).model)->height}; }}, .relocate{[this](auto &actor, auto const &position){ spatial.move(actor, position); }}},
    targets{mission, scene, actors, spatial, random, terrain, structures, mobiles, models, trig}, weapon_random{seeds.weapons, random.global_seed}, tables{executable},
    combat{mission, scene, actors, spatial, targets, terrain, trig, tables, formations, environment, random, weapon_random, difficulty, combat_callbacks()}, performance{executable},
    movement{actors, mission, formations, models, trig, environment, navigation, performance, random, movement_events()} {
  environment.relocate = [this](auto &actor, auto const &position){ spatial.move(actor, position); };
  targets.additional_state = [this](uint32_t id){ mobile_target_state state; if(auto i{spatial.actor_index(id)}) state.damage = combat.attention[*i].damage; if(pilot) state.player_airspeed = pilot->readings().airspeed; if(sensors && spatial.player && spatial.mobile_id(*spatial.player) == id) { state.ground_radar = sensors->state.ground; state.air_radar = sensors->state.air; } return state; };
  if(armed) {
    mission_weapon_callbacks callbacks;
    callbacks.grounded = [this](size_t index){ return pilot && timing.player == index ? pilot->landed() : actors.on_ground(index); };
    callbacks.incoming = [this](size_t round, unsigned int weapon, int32_t arrival){ if(player_armament) player_armament->warnings.incoming(round, weapon, arrival); };
    callbacks.launched = [this](size_t i, auto const &round){ if(events.launched) events.launched(i, round); };
    callbacks.event = [this](size_t i, auto &round, auto const &info){ if(events.projectile) events.projectile(i, round, info); if(damage) damage->impact(round, info); if(player_armament && info.kind == projectile_event::removed) player_armament->removed(i, round.state.weapon); };
    weapons = std::make_unique<mission_weapons>(mission, actors, spatial, targets, combat, models, terrain, structures, mobiles, trig, tables, random, weapon_random, difficulty, callbacks);
  }
  if(hurt) {
    if(!weapons) throw std::runtime_error{"Mission damage requires projectiles"};
    mission_damage_callbacks callbacks;
    callbacks.score = [this, difficulty](int32_t model){ quick_score = math::add(quick_score, quick_start_points(*models.at(static_cast<size_t>(model)), difficulty)); };
    callbacks.killed = [this](auto const &target, auto const &source){ if(events.killed) events.killed(target, source); };
    callbacks.player = [this](auto action, auto const &source){ if(player_damage) player_damage->apply(action, source, targets.now); };
    callbacks.player_killed = [this](auto const &source){ if(events.player_killed) events.player_killed(source); };
    callbacks.sound = [this](unsigned int id, bool cockpit){ if(events.sound) events.sound(id, cockpit); };
    callbacks.message = [this](std::string_view text){ if(events.message) events.message(text); if(events.notice) events.notice(text, 2, 200); };
    damage = std::make_unique<mission_damage>(mission, scene, actors, spatial, targets, combat, *weapons, models, terrain, structures, mobiles, trig, random, weapon_random, tables.skill(difficulty), [this](int32_t x, int32_t y){ return static_cast<uint8_t>(resources::surface_type(features, super_features, x, y)); }, callbacks);
    damage->explosions.play_sound = [this](auto const &sound){ if(events.explosion_sound) events.explosion_sound(sound); };
  }
  actors.synchronize_models();
}
void mission_runtime::start_player(size_t const index, mission_flight_settings const &settings) {
  sensors.reset(); route = {}; player_armament.reset(); player_damage.reset(); combat.player_target.reset(); timing.player = index;
  timing.arcade = settings.arcade;
  pilot = std::make_unique<mission_flight>(mission, actors, spatial, models, terrain, trig, environment.surface, index, settings);
  combat.player_position = pilot->readings().position.position; combat.player_heading = pilot->readings().angles[0];
  if(damage) {
    player_damage_callbacks callbacks;
    callbacks.feedback = [this](auto const &event){
      if(event.kind == damage_feedback_kind::message) { if(events.message) events.message(event.text); if(events.notice) events.notice(event.text, 2, 400); }
      else if(event.kind == damage_feedback_kind::cockpit_sound) { if(events.sound) events.sound(event.value, true); }
      else { if(sensors && player_armament) sensors->state.toggle_automatic(player_armament->control_mode, pilot->faults()); if(events.radar_toggle) events.radar_toggle(event.value); }
    };
    callbacks.impact_sound = [this](bool heavy){ if(events.impact_sound) events.impact_sound(heavy); };
    player_damage = std::make_unique<mission_player_damage>(*pilot, actors, spatial, targets, combat, models, callbacks);
  }
}
void mission_runtime::equip_player(std::array<int32_t, 9> const &loadout) {
  if(!pilot || !weapons || !timing.player) throw std::runtime_error{"Player armament requires a running player mission"};
  player_armament_events callbacks;
  callbacks.cockpit_sound = [this](unsigned int id){ if(events.sound) events.sound(id, true); };
  callbacks.launch_sound = [this](unsigned int id){ if(events.launch_sound) events.launch_sound(id); };
  callbacks.incoming_sound = [this](){ if(events.incoming_sound) events.incoming_sound(); };
  callbacks.gun_sound = [this](bool active){ if(events.gun_sound) events.gun_sound(active); };
  callbacks.warning_message = [this](size_t slot, bool added, std::string_view text){ if(events.warning_message) events.warning_message(slot, added, text); };
  callbacks.cycle_target = [this](){ if(sensors && player_armament) sensors->state.cycle_target(true, player_armament->control_mode); if(events.cycle_target) events.cycle_target(); };
  callbacks.missile_camera = [this](size_t i){ if(events.missile_camera) events.missile_camera(i); };
  player_armament = std::make_unique<mission_player_weapons>(*timing.player, actors, spatial, targets, combat, *weapons, *pilot, models, executable, trig, loadout, callbacks);
  player_armament->inventory.infinite = mission.infinite_weapons;
}
void mission_runtime::enable_player_sensors(unsigned int const game_mode) {
  if(!pilot || !player_armament) throw std::runtime_error{"Player sensors require flight and armament"};
  sensors = std::make_unique<player_sensors>(terrain, models, scene, mobiles, targets, trig, game_mode);
  sensors->message = [this](std::string_view text){ if(events.message) events.message(text); if(events.notice) events.notice(text, 0, 200); };
  player_armament->game_mode = game_mode; synchronize_player_target();
}
void mission_runtime::synchronize_player_target() {
  if(!sensors || !player_armament) return;
  *player_armament->target = sensors->state.current;
  player_armament->ground_radar = sensors->state.ground; player_armament->air_radar = sensors->state.air;
  if(player_damage) { player_damage->state.ground_radar = sensors->state.ground; player_damage->state.air_radar = sensors->state.air; }
}
void mission_runtime::look_player(math::pose const &view, render::projection const &projection) {
  if(!sensors || !pilot || !player_armament) return;
  sensors->pick_optical(view, pilot->readings().angles[0], projection, player_armament->control_mode, pilot->faults()); synchronize_player_target();
}
bool mission_runtime::fire_player(int32_t const now, double const dt, uint16_t const key) {
  if(!player_armament) return false;
  synchronize_player_target();
  combat.set_time(now, timing.delta, static_cast<uint32_t>(frame));
  return player_armament->fire(now, dt, key);
}
void mission_runtime::advance(int32_t const now, int32_t const delta, flight_input const &input, std::function<unsigned int()> const &flight_random) {
  int32_t const elapsed{math::subtract(now, previous_time)};
  if(elapsed <= 0 || delta <= 0) throw std::runtime_error{"Mission time must advance"};
  timing.now = now; timing.delta = delta; combat.set_time(now, delta, static_cast<uint32_t>(frame));
  if(weapons) weapons->environment.now = now;
  if(damage) damage->environment.flow = timing.game_flow;
  synchronize_player_target();
  if(move_actors) movement.update(timing);
  double const dt{static_cast<double>(elapsed) * 0.005};
  if(pilot && flight_active) {
    int32_t flight_delta{delta}; double flight_dt{dt}; unsigned int substeps{1};
    if(subdivide_flight) while(flight_delta > 0x3fffffff) { flight_delta /= 2; flight_dt *= 0.5; substeps *= 2; }
    for(unsigned int i{0}; i != substeps; ++i) {
      auto const previous{pilot->readings().position.position};
      if(!player_armament || player_armament->control_mode != 1) {
        auto const result{pilot->advance(flight_dt, flight_delta, input, flight_random && !pilot->uses_arcade() ? flight_random() : 0)};
        if(events.flight) events.flight(result);
      }
      combat.player_position = pilot->readings().position.position; combat.player_heading = pilot->readings().angles[0];
      if(check_collisions) {
        auto const landing{pilot->collide(previous, structures, mobiles, damage ? damage->marks : collision_marks, [this](auto const &id){ if(damage) damage->destroy(id); else if(events.collision) events.collision(id); })};
        if(landing && events.landing) events.landing(*landing);
      }
    }
  }
  if(player_armament) { player_armament->synchronize_countermeasures(false); player_armament->warnings.begin(); }
  if(weapons) weapons->update({now, delta, dt, smoke_radius, smoke_lifetime, smoke_density});
  if(player_armament) { player_armament->synchronize_countermeasures(true); player_armament->finish_update(now); }
  if(events.player_weapon_update) events.player_weapon_update();
  if(move_actors) combat.update();
  actors.synchronize_models(false);
  if(events.instrument_update) events.instrument_update();
  if(sensors && pilot && player_armament) { sensors->update(actors.units[*timing.player].object.world, pilot->readings().angles[0], spatial.mobile_id(*timing.player), player_armament->control_mode, pilot->faults()); synchronize_player_target(); }
  if(sensors && pilot) {
    auto &actor{actors.units[*timing.player]};
    auto const &path{mission.tasks[actor.location.task].actors[actor.location.group].waypoints};
    route.update(path, actor.waypoint, pilot->readings().position.position, {.key_event{}, .achieved{events.waypoint}, .sound{[this](unsigned int id){ if(events.sound) events.sound(id, true); }}, .surface{environment.surface}});
  }
  if(damage) damage->marks.advance();
  if(weapons) weapons->visuals.advance(delta, terrain);
  debris.advance(delta, trig, terrain, [this](int32_t x, int32_t y){ return static_cast<uint8_t>(environment.surface(x, y)); }, damage ? damage->marks : collision_marks, random.global_seed);
  previous_time = now; ++frame;
}
} // namespace apache::simulation
