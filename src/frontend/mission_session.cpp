#include "mission_session.h"
#include "render/display_layout.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include "resources/reader.h"

namespace apache::frontend {
namespace {
simulation::mission_random_state weapon_seeds() {
  simulation::mission_random_state result;
  auto const pair{[&](){ auto const high{(result.global = result.global * 0x41c64e6du + 0x3039u) & 0x7fff0000u}; result.global = result.global * 0x41c64e6du + 0x3039u; return high | ((result.global >> 16) & 0x7fff); }};
  result.damage = pair(); result.weapons = pair();
  return result;
}
resources::mission_briefing find_mission(resources::data_store const &data, int const id) {
  auto const missions{resources::read_mission_briefings(data.read("MISSDATA/MISSIONS.BLK"))};
  auto const found{std::ranges::find(missions, id, &resources::mission_briefing::id)};
  if(found == missions.end()) throw std::runtime_error{"Unknown mission " + std::to_string(id)};
  return *found;
}
std::string mission_path(resources::mission_briefing const &briefing) {
  if(briefing.flow == 5) return "MISSDATA/MULTI.DTA";
  if(briefing.flow == 6) return "MISSDATA/CAPTURE.DTA";
  if(briefing.flow != 1) return "MISSDATA/" + std::to_string(briefing.id) + ".DTA";
  if(briefing.zone < 1 || briefing.zone > 3) throw std::runtime_error{"Invalid Quickstart warzone"};
  return "MISSDATA/QS_" + std::string(1, "YKC"[briefing.zone - 1]) + ".DTA";
}
mission_options connection_options(mission_options options) {
  if(!options.connection) return options;
  auto const &connection{*options.connection};
  if(connection.local >= 16 || connection.server >= 16) throw std::runtime_error{"Invalid network connection slot"};
  auto const rules{multiplayer::apply_rules(connection.game)};
  options.difficulty = rules.forces; options.invulnerable = false;
  options.infinite_weapons = rules.infinite_weapons; options.no_crashes = rules.no_crashes;
  options.shared_seat = connection.game.mode == multiplayer::game_mode::pilot_gunner ? std::optional{connection.game.second_seat ? multiplayer::crew_role::gunner : multiplayer::crew_role::pilot} : std::nullopt;
  return options;
}
std::optional<resources::mission_definition> connection_definition(resources::data_store const &data, resources::mission_briefing const &briefing, mission_options const &options, resources::mission_definition const *edited) {
  if(!options.connection) return {};
  auto const bytes{edited ? resources::bytes{} : data.read(mission_path(briefing))};
  auto definition{edited ? *edited : resources::read_mission_definition(std::string{bytes.begin(), bytes.end()})};
  multiplayer::mission_rules input;
  input.wind_strength = definition.wind_strength; input.wind_direction = definition.wind_direction;
  input.weather = definition.weather; input.time_of_day = definition.time_of_day;
  auto const rules{multiplayer::apply_rules(options.connection->game, input)};
  definition.wind_strength = rules.wind_strength; definition.wind_direction = rules.wind_direction;
  definition.weather = rules.weather; definition.time_of_day = rules.time_of_day;
  return definition;
}
std::string warzone(int const id) {
  static std::array<std::string, 4> const names{"YEMEN", "KOREA", "CYPRUS", "FORT"};
  if(id < 1 || id > 4) throw std::runtime_error{"Invalid mission warzone"};
  return names[static_cast<size_t>(id - 1)];
}
}
unsigned int mission_session::next_random() { library_random = library_random * 0x41c64e6du + 0x3039u; return (library_random >> 16) & 0x7fff; }
resources::mission_briefing mission_session::quick_start(resources::data_store const &data, int const zone) {
  if(zone < 1 || zone > 3) throw std::runtime_error{"Quickstart requires Yemen, Korea or Cyprus"};
  auto const bytes{data.read("MISSDATA/QS_" + std::string(1, "YKC"[zone - 1]) + ".TXT")};
  resources::mission_briefing result; result.flow = 1; result.zone = zone;
  result.text = resources::tagged_text({reinterpret_cast<char const*>(bytes.data()), bytes.size()});
  for(auto const &section : result.text) if(section.tag == "Title") for(auto const &line : section.lines) { if(!result.title.empty()) result.title += ' '; result.title += line; }
  return result;
}
mission_session::mission_session(resources::data_store const &data, int const mission_id, mission_options const options)
  : mission_session{data, find_mission(data, mission_id), options} {}
mission_session::mission_session(resources::data_store const &data, resources::mission_briefing definition, mission_options const options, resources::mission_definition const *edited)
  : settings{connection_options(options)}, overlay{options.video_mode ? 640u : 320u, options.video_mode ? 480u : 240u}, initial_seeds{weapon_seeds()}, network_definition{connection_definition(data, definition, settings, edited)}, briefing{std::move(definition)}, zone{warzone(briefing.zone)},
    runtime{data, zone, mission_path(briefing), true, true, settings.shared_seat ? 2 : settings.difficulty, initial_seeds, network_definition ? &*network_definition : edited},
    daylight{runtime.executable, runtime.mission.time_of_day}, world{data, zone}, panels{data, options.video_mode},
    avionics{panels, runtime.trig, runtime.executable}, frame{options.graphics.fixed_resolution ? options.graphics.fixed_resolution->width : overlay.image.width, options.graphics.fixed_resolution ? options.graphics.fixed_resolution->height : overlay.image.height} {
  frame.horizontal_fov = options.graphics.field_of_view;
  if(settings.shared_seat) {
    if(*settings.shared_seat != multiplayer::crew_role::pilot && *settings.shared_seat != multiplayer::crew_role::gunner) throw std::runtime_error{"Invalid shared cockpit seat"};
    settings.difficulty = 2; settings.invulnerable = settings.infinite_weapons = settings.no_crashes = false;
  }
  auto const game_mode{settings.connection ? settings.connection->game.mode : settings.shared_seat ? multiplayer::game_mode::pilot_gunner : multiplayer::game_mode::single_player};
  auto const player{std::ranges::find_if(runtime.actors.units, [this, game_mode](auto const &actor){
    bool const first{game_mode == multiplayer::game_mode::single_player || game_mode == multiplayer::game_mode::pilot_gunner || game_mode == multiplayer::game_mode::apache_vs_hind};
    return runtime.mission.tasks[actor.location.task].user && actor.location.member == 0 && (first ? actor.location.group == 0 : actor.serial == static_cast<int32_t>(settings.connection->local));
  })};
  if(player == runtime.actors.units.end()) throw std::runtime_error{"Mission has no player aircraft"};
  runtime.timing.game_flow = briefing.flow;
  runtime.start_player(static_cast<size_t>(player - runtime.actors.units.begin()), {.arcade{settings.arcade}, .flow{briefing.flow}, .game_mode{static_cast<unsigned int>(game_mode)}});
  runtime.equip_player(runtime.mission.weapons); runtime.enable_player_sensors(static_cast<unsigned int>(game_mode));
  runtime.pilot->arcade.invulnerable = runtime.pilot->realistic.invulnerable = runtime.damage->invulnerable = settings.invulnerable;
  runtime.pilot->arcade.no_crashes = runtime.pilot->realistic.no_crashes = settings.no_crashes;
  runtime.pilot->arcade.infinite_fuel = runtime.pilot->realistic.infinite_fuel = settings.connection ? multiplayer::apply_rules(settings.connection->game).infinite_fuel : briefing.flow == 1 && !settings.shared_seat;
  runtime.player_armament->inventory.infinite |= !settings.shared_seat && (settings.infinite_weapons || briefing.flow == 1);
  if(briefing.flow == 1) { runtime.player_armament->inventory.armed = true; if(!runtime.player_armament->inventory.helmet_mode) runtime.player_armament->inventory.helmet_mode = 1; }
  runtime.player_damage->state.random = initial_seeds.damage;
  runtime.combat.warnings.ships_attacked = -60000;
  runtime.sensors->state.tads = false;
  controls.configuration = settings.controls; controls.configuration.resolve();
  controls.curve = simulation::joystick_curve{settings.deadband}; controls.joystick = settings.axes;
  views = std::make_unique<simulation::player_views>(runtime.executable, runtime.trig, runtime.environment.height, runtime.sensors->state, settings.shared_seat ? static_cast<unsigned int>(*settings.shared_seat) : 2);
  runtime.player_armament->helmet_direction = [this](){
    if(views->modes.mode == 12) return runtime.player_armament->helmet;
    auto const &rotation{runtime.pilot->readings().position.rotation};
    return math::vector3{rotation[3], rotation[4], rotation[5]};
  };
  instruments = std::make_unique<simulation::mission_instruments>(runtime, settings.video_mode, static_cast<unsigned int>(game_mode));
  runtime.events.instrument_update = [this](){ instruments->sample(); };
  instruments->state.damaged_display_seed = 0;
  world.initialise_model_metadata(runtime.executable);
  world.bind_scene(runtime.scene.base, runtime.scene.extra, runtime.mobiles, runtime.weapons->visuals, runtime.damage->marks, runtime.debris);
  panels.install_palette(world.palette.colours);
  night_vision = daylight.night_vision(); instruments->state.helmet.colour = daylight.light.night == 0 ? 208 : 223;
  int const cloud_modifier{static_cast<int>(next_random() / 11)};
  visual.sky.weather = static_cast<unsigned int>(runtime.mission.weather);
  if(runtime.mission.weather == 1) { visual.cloud_height = cloud_modifier + 14000; visual.cloud_bottom = (cloud_modifier + 13000) * 256; visual.cloud_top = (cloud_modifier + 15000) * 256; }
  runtime.weapons->environment.weather = runtime.mission.weather;
  runtime.weapons->environment.cloud_bottom = visual.cloud_bottom; runtime.weapons->environment.cloud_top = visual.cloud_top;
  detail(settings.detail);
  views->context.has_target = [this](size_t i){ auto *r{round(i)}; return r && simulation::projectile_has_target(*r, simulation::weapon_definitions()[r->state.weapon], runtime.targets.world, [this](uint32_t id, auto slot){ return runtime.weapons->target_slot(id, slot); }, runtime.timing.now); };
  views->context.has_lock = [this](size_t i){ auto *r{round(i)}; return r && simulation::weapon_definitions()[r->state.weapon].movement_class == 2 && (r->state.body.seeker.flags & 2); };
  views->crashed = [this](bool airborne){ crash(airborne); };
  runtime.pilot->died = [this](){ select_view(19); };
  runtime.pilot->landing_award = [this](){ landing_award(); };
  runtime.events.projectile = [this](size_t i, auto &, auto const &event){ if(event.kind == simulation::projectile_event::removed) views->removed(i); };
  runtime.events.explosion_sound = [this](simulation::explosion_audio const &sound){
    // The original sound selector shares Watcom rand() with wind gusts, even
    // when no audio device is active. Keep that ordering in the live session.
    auto const sample{next_random() % 3 + 1};
    if(play_sound) play_sound(sample, sound.volume, sound.pan);
  };
  runtime.events.impact_sound = [this](bool heavy){
    if(heavy) { auto const sample{next_random() % 3 + 1}; if(play_sound) play_sound(sample, 63, 32768); }
    else if(play_sound) play_sound(6, 60, 32768);
  };
  resources::reader radio{runtime.executable.at(0x446960, radio_effects.size() * 4), "Radio effect choices"};
  for(auto &id : radio_effects) id = radio.u32();
  runtime.events.player_weapon_update = [this](){
    if(briefing.flow != 2 && accelerated_time == 1 && runtime.timing.now >= next_radio) {
      auto const id{radio_effects[static_cast<size_t>(runtime.weapon_random.next() % 54)]};
      if(runtime.events.sound) runtime.events.sound(id, false);
      next_radio = math::add(runtime.timing.now, runtime.weapon_random.next() % 6000);
    }
  };
  runtime.events.missile_camera = [this](size_t i){ views->context.latest_weapon = i; };
  runtime.events.flight = [this](simulation::flight_step_result const &result){
    if(result.landing) {
      records.add({{12, 0, 0, 0, 0, result.landing->grade}});
      if(crew) crew->outgoing.pilot.landing = static_cast<uint8_t>(result.landing->grade);
    }
    using enum simulation::flight_event;
    for(auto event : result.events) { if(flight_sound) flight_sound(event); }
    for(auto event : result.events) switch(event) {
      case fuel_exhausted: communications.add("OUT OF FUEL", 3, 2000); break;
      case hover_on: communications.add("AUTO HOVER ON", 3, 600); break;
      case hover_off: communications.add("AUTO HOVER OFF", 3, 600); break;
      case engine_overheat: communications.add("ENGINE OVERHEAT", 3, 2000); break;
      case left_engine_failed: communications.add("LHS ENGINE FAILED", 3, 2000); break;
      case right_engine_failed: communications.add("RHS ENGINE FAILED", 3, 2000); break;
      case engines_overheated: communications.add("ENGINE FAILED", 3, 2000); break;
      case overtorque_on: communications.add_changing(4, "ENGINE OVERTORQUE", 3); break;
      case overtorque_off: communications.remove(4); break;
      case overspeed_on: communications.add_changing(5, "VMAX EXCEEDED", 3); break;
      case overspeed_off: communications.remove(5); break;
      case pull_up_on: communications.add_changing(6, "PULL UP PULL UP", 3); break;
      case pull_up_off: communications.remove(6); break;
      default: break;
    }
  };
  runtime.events.landing = [this](auto const &landing){
    if(landing.grade) records.add({{12, 0, 0, 0, 0, landing.grade}});
    if(crew && landing.grade) crew->outgoing.pilot.landing = static_cast<uint8_t>(landing.grade);
  };
  runtime.events.killed = [this](auto const &target, auto const &source){
    if(network) network->record_kill(target, source);
    uint32_t const who{source.kind == simulation::target_kind::mobile ? source.index : UINT32_MAX};
    if(target.kind == simulation::target_kind::mobile) records.add({{8, static_cast<uint32_t>(runtime.timing.now), who, 0, 0, target.index}});
    else if(target.kind == simulation::target_kind::scenery) records.add({{9, static_cast<uint32_t>(runtime.timing.now), who, static_cast<uint32_t>(target.layer), 0, target.index}});
  };
  runtime.events.notice = [this](auto text, auto priority, auto lifetime){ communications.add(text, priority, lifetime); };
  runtime.events.warning_message = [this](size_t slot, bool added, auto text){
    auto const id{static_cast<uint32_t>(slot + 1)};
    if(added) communications.add_changing(id, text, 2); else communications.remove(id);
  };
  if(settings.shared_seat || settings.connection) {
    chat = std::make_unique<multiplayer_chat>(runtime.executable);
    chat->change_terrain = [this](auto const &values){ world.palette.grass_colours(values); };
    chat->send = [this](std::string_view text){
      if(!network) return;
      auto const &state{network->state}; auto const &name{state.players[state.local].name};
      if(network->send) network->send(multiplayer::chat(state.local, name, text));
      chat->add("> " + name + ": " + std::string{text}, runtime.timing.now);
    };
    network = std::make_unique<simulation::mission_network>(runtime);
    network->enabled = network->state.active = true;
    if(settings.connection) {
      auto const &connection{*settings.connection}; network->state.players = connection.players;
      network->state.local = connection.local; network->state.server = connection.server;
      network->state.client_server = multiplayer::apply_rules(connection.game).client_server;
      for(uint32_t i{0}; i < 16; ++i) if(i != connection.local && ((connection.players[i].flags >> 8) & 255) != 255) ++network->lifecycle.other_players;
    } else {
      network->state.local = static_cast<uint32_t>(*settings.shared_seat);
      network->state.players[network->state.local].name = *settings.shared_seat == multiplayer::crew_role::pilot ? "PILOT" : "GUNNER";
    }
    if(settings.shared_seat) network->state.seat = *settings.shared_seat;
    network->lifecycle.mode = game_mode;
    runtime.pilot->realistic.game_mode = static_cast<unsigned int>(game_mode); runtime.damage->environment.mode = static_cast<unsigned int>(game_mode);
    runtime.move_actors = game_mode != multiplayer::game_mode::combat && game_mode != multiplayer::game_mode::multiplayer && game_mode != multiplayer::game_mode::capture_the_flag;
    runtime.events.player_killed = [this](auto const &source){ network->player_killed(source); };
    auto const player_index{*runtime.timing.player};
    runtime.actors.units[player_index].network = static_cast<int32_t>(network->state.local);
    if(!network->bind(network->state.local, player_index)) throw std::runtime_error{"Cannot bind player aircraft"};
    runtime.player_armament->inventory.events.remote_fire = [this](size_t index){
      if(!network->send) return;
      auto const *shot{round(index)}; if(!shot) return;
      auto const &body{runtime.actors.units[*runtime.timing.player].object};
      auto const &rotation{body.children.at(0)->children.at(0)->world.rotation};
      network->send(multiplayer::fired(network->state.local, network->state.players[network->state.local].name,
        shot->state.weapon, shot->state.random, network->identify(*runtime.player_armament->target), {rotation[3], rotation[4], rotation[5]}));
    };
    runtime.weapons->countermeasure_released = [this](unsigned int weapon){
      if(network->send) network->send(multiplayer::event(weapon == 8 ? multiplayer::message_type::flare : multiplayer::message_type::chaff,
        network->state.local, network->state.players[network->state.local].name, 0));
    };
    runtime.pilot->mobile_collision = [this](){
      if(network->send) network->send(multiplayer::event(multiplayer::message_type::crashed, network->state.local,
        network->state.players[network->state.local].name, static_cast<int32_t>(runtime.actors.units[*runtime.timing.player].location.task)));
    };
    if(settings.shared_seat) {
    runtime.player_armament->control_mode = static_cast<unsigned int>(*settings.shared_seat);
    runtime.player_armament->inventory.helmet_mode = *settings.shared_seat == multiplayer::crew_role::pilot ? 2 : 3;
    crew = std::make_unique<simulation::mission_crew>(runtime, *network);
    crew->key = [this](uint16_t code){ command(code); };
    crew->death = [this](unsigned int cause){ request_end(cause); };
    crew->landing = [this](uint8_t grade){ records.add({{12, 0, 0, 0, 0, grade}}); };
    crew->radio = [this](std::string_view text, uint8_t sample){ communications.add(text, 3, 1000); if(play_sound) play_sound(sample, 63, 32768); };
    runtime.player_armament->inventory.events.key_event = runtime.weapons->player_key_event = [this](uint16_t code){ return crew->notify_key(code); };
    }
    network->lifecycle.events.death = [this](unsigned int cause){ request_end(cause); };
    views->context.network = true; views->context.game_mode = static_cast<unsigned int>(game_mode);
    world.before_mobile_scan = [this](){
      network->state.dead = runtime.pilot->death() != 0;
      network->state.active = network->state.active && !finished;
      if(wait_network) wait_network();
      network->state.begin_frame();
      if(crew && crew->leaving) ending.countdown = 1;
    };
    if(settings.connection && !settings.connection->defer_initialisation) initialise_remote_players();
  }
  select_view(views->modes.home);
}
void mission_session::detail(unsigned int const level) {
  if(level > 9) throw std::runtime_error{"Visual detail must be between 0 and 9"};
  settings.detail = level;
  visual.distance = level < 7 ? 1 : (level - 6) * 2 + 1;
  visual.far_object_depth = level == 0 ? 0x200000 : 0x400000;
  visual.terrain_detail = level < 2 ? 0 : level < 5 ? 1 : level == 5 ? 2 : 3;
  visual.models.textures = level >= 3; visual.models.gouraud = level >= 6;
  visual.sky.cloud_flags = level == 0 ? 0 : level < 4 ? 2 : 10;
  runtime.smoke_radius = 256; runtime.smoke_lifetime = level < 5 ? 400 : 600; runtime.smoke_density = level < 2 ? 1 : level < 6 ? 2 : 3;
}
simulation::projectile_record *mission_session::round(size_t const index) {
  if(index >= runtime.weapons->rounds.entries().size() || !runtime.weapons->rounds.entries()[index]) return nullptr;
  return &runtime.weapons->rounds.at(index);
}
std::optional<simulation::view_target> mission_session::missile_target(size_t const index) {
  auto *r{round(index)}; if(!r) return {};
  auto *target{simulation::adjusted_projectile_target(*r, runtime.targets.world, [this](uint32_t id, auto slot){ return runtime.weapons->target_slot(id, slot); })};
  if(!target || target->identity.kind == simulation::target_kind::none) return {};
  auto const description{runtime.targets.describe(*target)};
  return simulation::view_target{description.movement.position, runtime.models.at(description.model) ? runtime.models.at(description.model)->height : 0};
}
simulation::view_sources mission_session::sources() {
  auto const subject{[this](render::model_instance const &object){ return simulation::view_subject{object.world, object.id, runtime.models.at(object.id)->radius}; }};
  simulation::view_sources result; result.player = subject(runtime.actors.units[*runtime.timing.player].object); result.faults = runtime.pilot->faults();
  if(drone && *drone < runtime.actors.units.size()) result.drone = subject(runtime.actors.units[*drone].object);
  auto const &target{runtime.sensors->state.current};
  if(target.identity.kind != simulation::target_kind::none || runtime.sensors->state.locked) result.target = {runtime.targets.world.position(target), 0};
  result.weapon = [this, subject](size_t i) -> std::optional<simulation::view_subject> { auto *r{round(i)}; return r && r->mobile && runtime.mobiles.contains(*r->mobile) ? std::optional{subject(runtime.mobiles.at(*r->mobile))} : std::nullopt; };
  result.weapon_target = [this](size_t i){ return missile_target(i); };
  return result;
}
void mission_session::select_view(unsigned int const mode) {
  views->select(mode, sources(), runtime.random.global_seed);
  runtime.player_armament->in_cockpit = views->modes.in_cockpit;
}
void mission_session::crash(bool const airborne) {
  runtime.flight_active = false;
  runtime.spatial.unplace(*runtime.timing.player);
  auto &object{runtime.actors.units[*runtime.timing.player].object};
  render::fracture_model(object, runtime.debris, runtime.random.global_seed);
  if(network && network->send) {
    auto const cause{runtime.pilot->death()}; auto const local{network->state.local}; auto const &name{network->state.players[local].name};
    if(cause == 2 && runtime.pilot->hit_actor) network->send(multiplayer::event(multiplayer::message_type::collision, local, name, runtime.actors.units[*runtime.pilot->hit_actor].serial, static_cast<int32_t>(runtime.actors.units[*runtime.pilot->hit_actor].location.task)));
    else if(cause == 3 || cause == 11 || cause == 12) network->send(multiplayer::event(multiplayer::message_type::crashed, local, name, static_cast<int32_t>(runtime.actors.units[*runtime.timing.player].location.task)));
  }
  if(airborne) {
    resources::reader bytes{runtime.executable.at(0x461510, 12), "crash velocity"};
    runtime.weapons->visuals.add_air_explosion(object.world.position, 0x3200, {bytes.i32(), bytes.i32(), bytes.i32()});
  } else runtime.weapons->visuals.add_ground_explosion(object.world.position, 0x3200, runtime.terrain);
}
void mission_session::request_end(unsigned int const cause) {
  ending.cause = runtime.pilot->death();
  bool const crash_view{ending.request(cause, settings.invulnerable, settings.no_crashes)};
  runtime.pilot->realistic.death = runtime.pilot->arcade.death = ending.cause;
  finished = finished || !ending.active;
  if(crash_view) select_view(19);
}
void mission_session::key(uint16_t const scan, bool const down, bool const dispatch) {
  controls.key(scan & 127, down); if(down && dispatch) post(scan);
}
void mission_session::command(uint16_t const key) {
  if(chat && chat->writing) {
    chat->key(key, runtime.timing.now);
    if(!chat->writing) { controls.counts[0x48] = controls.counts[0x39] = 0; }
    return;
  }
  if(chat && key == 0x4032) { chat->begin(); return; }
  if(quit_prompt) {
    if((key & 255) == 0x15) request_end(1);
    else if((key & 255) == 0x31) { quit_prompt = false; quit_drawn = false; views->modes.changed = true; }
    return;
  }
  auto &arm{*runtime.player_armament}; auto &sensors{runtime.sensors->state}; auto &displays{instruments->state.displays};
  auto const mode{arm.control_mode}; auto const faults{runtime.pilot->faults()}; bool const gunner{views->modes.crew == 1};
  simulation::targeting_events const target_events{.key_event{crew ? std::function<bool(uint16_t)>{[this](uint16_t code){ return crew->notify_key(code); }} : nullptr}, .cockpit_sound{runtime.events.sound ? std::function<void(unsigned int)>{[this](unsigned int id){ runtime.events.sound(id, true); }} : nullptr}, .show_radar{[&](){ displays.show_radar(gunner, faults); }}};
  if((key & 255) == 0x1c) { arm.select(runtime.timing.now, key); return; }
  if((key & 255) == 0x13) { sensors.cycle_radar(key, mode, target_events); return; }
  if((key & 255) == 0x26) { sensors.toggle_lock(key, mode, faults, target_events); return; }
  if(crew && key >= 0x203b && key <= 0x2044) { crew->outgoing.message = static_cast<uint8_t>(key - 0x203a); return; }
  if(key >= 2 && key <= 11) {
    if(!settings.arcade) runtime.pilot->realistic.dynamics.hover.active = false;
    if(mode == 1 || (mode == 0 && crew && crew->notify_key(key))) return;
    if(settings.arcade) { static std::array<double, 10> constexpr values{-.8, -.6, -.4, -.2, 0, .2, .4, .6, .8, 1}; runtime.pilot->arcade.delta_vertical_velocity = values[key - 2]; }
    else { runtime.pilot->realistic.dynamics.hover.active = false; runtime.pilot->realistic.dynamics.power.collective = static_cast<double>(key - 1) * 10.0; }
    return;
  }
  switch(key) {
    case 0x2001: case 0x2010: case 0x102d:
      if(crew) { crew->notify_key(key); request_end(1); }
      else quit_prompt = true;
      break;
    case 0x3b: if(!network) help_requested = true; break;
    case 1: case 0x200f: if(!network) accelerated_time = 1; break;
    case 0x400f: if(!network) accelerated_time = 8; break;
    case 0xf: if(!network) accelerated_time = accelerated_time == 8 ? 1 : accelerated_time * 2; break;
    case 0x19: if(!network) paused = !paused; break;
    case 0x31: case 0x4031: {
      auto &actor{runtime.actors.units[*runtime.timing.player]};
      runtime.route.cycle(runtime.mission.tasks[actor.location.task].actors[actor.location.group].waypoints, actor.waypoint, key == 0x31, key, mode, {.key_event{target_events.key_event}, .achieved{}, .sound{}, .surface{}}); break;
    }
    case 0x33: case 0x34:
      if(views->modes.in_cockpit) {
        if(views->modes.mode == 13) views->optics.zoom(13, key == 0x34);
        else instruments->state.map_range = key == 0x34 ? std::max(instruments->state.map_range / 2, 0x80000) : std::min(instruments->state.map_range * 2, 0x1000000);
      } break;
    case 0x17: instruments->state.helmet.colour = instruments->state.helmet.colour == 208 ? 223 : 208; break;
    case 0x2f: night_vision = !night_vision; break;
    case 0xe: case 0x400e: case 0xd: sensors.cycle_target(key == 0xe, mode); break;
    case 0x15: sensors.toggle_automatic(mode, faults); break;
    case 0x22: sensors.cycle_ground_range(key, mode, target_events); break;
    case 0x16: sensors.toggle_freeze(key, mode, target_events); break;
    case 0x25: sensors.toggle_tactical_range(); break;
    case 0x12: sensors.toggle_ecm(key, mode, target_events); break;
    case 0x32: case 0x4032:
      if(!sensors.air) sensors.threat = key == 0x32 ? (sensors.threat == 1 ? 4 : sensors.threat - 1) : (sensors.threat == 4 ? 1 : sensors.threat + 1);
      else if(sensors.air) sensors.scan_all = !sensors.scan_all;
      break;
    case 0x3c: case 0x47: select_view(0); break;
    case 0x3d: case 0x49: select_view(5); break;
    case 0x3e: case 0x4f: select_view(views->modes.left_view()); break;
    case 0x3f: case 0x51: select_view(views->modes.right_view()); break;
    case 0x40: select_view(10); break;
    case 0x23: select_view(12); break;
    case 0x14: views->select_optics(sources(), runtime.random.global_seed); break;
    case 0x41: select_view(14); break;
    case 0x42: select_view(15); break;
    case 0x43: select_view(18); break;
    case 0x44: select_view(17); break;
    case 0x57: select_view(16); break;
    case 0x58: case 0x4058: {
      if(runtime.actors.units.empty()) break;
      size_t candidate{drone.value_or(*runtime.timing.player)};
      for(size_t i{0}; i != runtime.actors.units.size(); ++i) {
        candidate = (candidate + (key == 0x58 ? 1 : runtime.actors.units.size() - 1)) % runtime.actors.units.size();
        auto const &actor{runtime.actors.units[candidate]};
        if(candidate != *runtime.timing.player && !actor.destroyed && runtime.mobiles.contains(runtime.spatial.mobile_id(candidate))) { drone = candidate; select_view(11); break; }
      } break;
    }
    case 0x39: runtime.fire_player(runtime.timing.now, frame_time, key); break;
    case 0x2e: case 0x21: arm.release_countermeasure(runtime.timing.now, key == 0x2e ? 7 : 8, key); break;
    case 0x202e: arm.toggle_countermeasures(key); break;
    case 0x1a: case 0x1b: case 0x401a: case 0x401b: displays.cycle(gunner, (key & 255) == 0x1b, (key & 0x4000) == 0); break;
    case 0x201a: case 0x201b: displays.toggle(gunner, (key & 255) == 0x1b, faults); break;
    case 0x18: if(settings.arcade) runtime.pilot->arcade.toggle_hover(); else runtime.pilot->realistic.dynamics.hover.toggle(runtime.pilot->realistic.dynamics.u); break;
    default: break;
  }
}
void mission_session::step(unsigned int const ticks) {
  if(ticks == 0 || ticks > 200) throw std::runtime_error{"Mission frame interval must be 1 to 200 ticks"};
  runtime.frame = ++frame_number;
  frame_time = static_cast<double>(ticks) * 0.005;
  last_delta = static_cast<int32_t>(std::min(frame_time * 2147483648.0, static_cast<double>(INT32_MAX)));
  controls.timer((keyboard_remainder + ticks) / 2); keyboard_remainder = (keyboard_remainder + ticks) % 2;
  daylight.update(runtime.timing.now);
  bool const was_prompt{quit_prompt};
  for(auto key : commands) { command(key); if(quit_prompt != was_prompt || help_requested || finished) break; }
  commands.clear();
  if(paused || (finished && !network) || was_prompt || help_requested) return;
  if(!(chat && chat->writing) && controls.held[0x39]) runtime.fire_player(runtime.timing.now, frame_time, 0x39);
  if(network) {
    network->state.dead = runtime.pilot->death() != 0;
    if(crew) {
      auto const packet{crew->transmit(ticks)};
      if(network->send) { network->send(packet); network->send_casualties(); }
      runtime.random.global_seed = 1;
    } else send_frame();
    if(!crew) { network->state.extrapolate(network_clock ? network_clock() : runtime.timing.now); network->update(); }
  }
  for(unsigned int i{0}; i != accelerated_time; ++i) {
    if(!(chat && chat->writing)) {
      auto const buttons{controls.button_commands()};
      for(size_t j{0}; j != buttons.count; ++j) post(static_cast<uint16_t>(buttons.keys[j]));
      controls.update(views->modes.movement, views->modes.crew == 1, buttons.alternate);
    }
    runtime.pilot->realistic.acceleration = static_cast<int32_t>(accelerated_time);
    runtime.weapons->night = daylight.light.night != 0;
    runtime.frame = frame_number;
    runtime.advance(math::add(runtime.timing.now, static_cast<int32_t>(ticks)), last_delta, controls.flight, [this](){ return next_random(); });
    records.add({{2, ticks, 0, 0, 0, 0}});
    records.position(runtime.pilot->readings().position.position);
    communications.advance(frame_time);
    auto const cause{runtime.pilot->death()};
    if((cause == 2 || cause == 3 || cause == 4 || cause == 10 || cause == 11 || cause == 12) && views->modes.mode != 19 && !views->context.following) select_view(19);
  }
  runtime.player_armament->update_hydra(runtime.timing.now, last_delta, controls.keyboard[4]);
}
bool mission_session::resize(unsigned int const width, unsigned int const height) {
  if(frame.image.width == width && frame.image.height == height) return false;
  frame = render::framebuffer{width, height}; frame.horizontal_fov = settings.graphics.field_of_view;
  views->modes.changed = true; quit_drawn = false;
  return true;
}
void mission_session::draw() {
  if(quit_prompt && quit_drawn) return;
  if(help_requested) return;
  if(network && views->modes.mode == 19 && int64_t{views->context.crash_timer} + (paused ? 0 : last_delta) / 4 > INT32_MAX) network_crash_complete();
  camera = views->update(sources(), {controls.external[3], controls.external[2], controls.keyboard[5], paused ? 0 : last_delta}, runtime.random.global_seed);
  finished = finished || camera.finished;
  visual.lighting = daylight.light; visual.infrared = night_vision || camera.infrared ? 1 : 0;
  visual.hidden_mobile = views->modes.in_cockpit ? std::optional{runtime.spatial.mobile_id(*runtime.timing.player)} : std::nullopt;
  auto const faders{panels.faders(daylight.light.night, night_vision)};
  world.palette.additional_faders.assign(faders.begin(), faders.end());
  world.palette.additional_faders.push_back({true, 10000, 2, 254, night_vision ? 1 : 0, {0, 1, 1}});
  world.palette.additional_faders.push_back({false, INT32_MAX, 0, 256, camera.fade, {0, 0, 0}});
  auto const viewport{views->modes.viewport(settings.video_mode)};
  render::rectangle const original_view{viewport[0], viewport[1], viewport[0] + viewport[2] - 1, viewport[1] + viewport[3] - 1};
  render::display_layout const layout{frame.image.width, frame.image.height, overlay.image.width, overlay.image.height, original_view, settings.graphics.field_of_view};
  bool const scaled{frame.image.width != overlay.image.width || frame.image.height != overlay.image.height};
  if(scaled) frame.clear();
  frame.clip = layout.scene;
  // Sight coordinates stay in the cockpit's authored pixel space. Its FOV
  // describes the same rays as the full-resolution scene beneath the overlay.
  render::projection const projection{viewport[0], viewport[1], viewport[2], viewport[3], layout.instrument_fov};
  world.draw(frame, camera.camera, visual);
  if(camera.sight) runtime.look_player(camera.camera, projection);
  auto &arm{*runtime.player_armament};
  auto const &rotation{views->modes.mode == 12 ? camera.camera.rotation : runtime.pilot->readings().position.rotation};
  arm.helmet = {rotation[3], rotation[4], rotation[5]}; arm.in_cockpit = views->modes.in_cockpit;
  arm.evaluate_sight(runtime.timing.now);
  instruments->update(views->modes.mode, camera.camera, projection);
  auto &state{instruments->state}; state.accelerated_time = accelerated_time; state.delta = last_delta;
  if(network && (network->lifecycle.mode == multiplayer::game_mode::combat || network->lifecycle.mode == multiplayer::game_mode::multiplayer || network->lifecycle.mode == multiplayer::game_mode::capture_the_flag)) {
    if(!state.scores) state.scores.emplace();
    auto &scores{state.scores->players}; scores.clear(); auto const local{network->state.local};
    auto const add{[&](size_t i){ auto const &p{network->state.players[i]}; scores.emplace_back(p.name, p.score); }};
    add(local);
    for(size_t i{0}; i < 16; ++i) if(i != local && ((network->state.players[i].flags >> 8) & 255) != 255) add(i);
  }
  state.helmet.viewing_device = views->optics.name(); state.helmet.magnification = views->optics.magnification().label;
  auto const look{views->modes.mode == 13 ? views->goggles : views->helmet}; state.helmet.sight_heading = look.heading; state.helmet.sight_pitch = look.pitch;
  auto const &power{settings.arcade ? runtime.pilot->arcade.power : runtime.pilot->realistic.dynamics.power};
  auto const endurance{static_cast<int32_t>(std::clamp(power.endurance, 0.0, static_cast<double>(INT32_MAX)))};
  for(size_t i{0}; i != arm.warnings.entries.size(); ++i) communications.update_changing(static_cast<uint32_t>(i + 1), arm.warnings.entries[i].text);
  if(views->modes.mode == 0 || views->modes.mode == 5) state.communications = communications.compose({
    .fuel{runtime.pilot->readings().fuel}, .endurance{endurance}, .flow{runtime.timing.game_flow},
    .score{runtime.quick_score}, .infinite_fuel{settings.arcade ? runtime.pilot->arcade.infinite_fuel : runtime.pilot->realistic.infinite_fuel}, .faults{runtime.pilot->faults()},
    .weapon{simulation::weapon_definitions()[arm.inventory.current].name}});
  auto &hud{scaled ? overlay : frame};
  if(scaled) layout.background(frame, overlay);
  hud.horizontal_fov = 90;
  hud.reset_clip(); panels.draw(hud, views->modes.mode, scaled || views->modes.changed); avionics.draw(hud, state, world, visual.lighting);
  for(size_t i{0}; i != 256; ++i) frame.image.colours[i] = {world.palette.modified[i * 3], world.palette.modified[i * 3 + 1], world.palette.modified[i * 3 + 2]};
  if(chat) chat->draw(hud, runtime.timing.now);
  views->modes.changed = false;
  if(network) {
    ending.finish_frame(); if(crew) crew->outgoing.leaving = ending.quitting;
    finished = finished || !ending.active || !network->state.active;
  }
  if(quit_prompt) {
    auto const &prompt{panels.sprite(settings.video_mode ? "END_YN.640" : "END_YN.320")};
    hud.reset_clip(); hud.blit(prompt, settings.video_mode ? 210 : 86, settings.video_mode ? 116 : 54, true); quit_drawn = true;
  }
  if(scaled) layout.composite(overlay, frame);
  frame.horizontal_fov = settings.graphics.field_of_view;
}
} // namespace apache::frontend
