#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string_view>
#include "math/trigonometry.h"
#include "frontend/presentation.h"
#include "inspect_actor_firing.h"
#include "inspect_actor_reactions.h"
#include "inspect_actor_targeting.h"
#include "inspect_arcade.h"
#include "inspect_audio.h"
#include "inspect_battles.h"
#include "inspect_blasts.h"
#include "inspect_briefing.h"
#include "inspect_cameras.h"
#include "inspect_collisions.h"
#include "inspect_combat_effects.h"
#include "inspect_combat_update.h"
#include "inspect_controls.h"
#include "inspect_countermeasures.h"
#include "inspect_damage.h"
#include "inspect_debrief.h"
#include "inspect_firing.h"
#include "inspect_flight.h"
#include "inspect_logbook.h"
#include "inspect_menu.h"
#include "inspect_mission_combat.h"
#include "inspect_mission_radar.h"
#include "inspect_mission_scenery.h"
#include "inspect_mission_spatial.h"
#include "inspect_mission_targets.h"
#include "inspect_navigation.h"
#include "inspect_network.h"
#include "inspect_patrol.h"
#include "inspect_player_weapons.h"
#include "inspect_projectile_pool.h"
#include "inspect_projectiles.h"
#include "inspect_seekers.h"
#include "inspect_sensors.h"
#include "inspect_session.h"
#include "inspect_target_search.h"
#include "inspect_targeting.h"
#include "inspect_targets.h"
#include "inspect_tracking.h"
#include "inspect_war_grid.h"
#include "inspect_weapons.h"
#include "render/avionics.h"
#include "render/clouds.h"
#include "render/cockpit.h"
#include "render/crater_renderer.h"
#include "render/draw_queue.h"
#include "render/effect_renderer.h"
#include "render/feature_renderer.h"
#include "render/helmet_display.h"
#include "render/horizon.h"
#include "render/instrument_font.h"
#include "render/landscape.h"
#include "render/mfd.h"
#include "render/model_renderer.h"
#include "render/panel_instruments.h"
#include "render/projection.h"
#include "render/projection2.h"
#include "render/rasterizer.h"
#include "render/sector_scan.h"
#include "render/shadows.h"
#include "render/terrain_projection.h"
#include "render/terrain_renderer.h"
#include "render/terrain_shading.h"
#include "resources/animation.h"
#include "resources/data_store.h"
#include "resources/image.h"
#include "resources/missions.h"
#include "resources/models.h"
#include "resources/terrain.h"
#include "resources/terrain_textures.h"
#include "simulation/actor_movement.h"
#include "simulation/actor_types.h"
#include "simulation/battles.h"
#include "simulation/ground_movement.h"
#include "simulation/instrument_state.h"
#include "simulation/mfd_state.h"
#include "simulation/mission_actors.h"

namespace {
bool read_helmet(unsigned int &mode, unsigned int &component, apache::render::helmet_instruments &state, apache::math::trigonometry const &trig, bool const extended) {
  unsigned int colour{223};
  unsigned int letter{65};
  if(!(std::cin >> mode >> component >> state.view >> colour >> state.heading >> state.command_heading >> state.pitch >> state.roll >> state.climb_rate >> state.radar_altitude >> state.barometric_altitude >> state.airspeed >> state.torque >> letter >> state.waypoint_distance >> state.waypoint_time >> state.sight_heading >> state.sight_pitch >> std::quoted(state.viewing_device) >> std::quoted(state.magnification) >> state.layout_grid)) return false;
  if(mode > 1 || component > (extended ? 21u : 10u)) throw std::runtime_error{"Invalid helmet display case"};
  state.colour = static_cast<uint8_t>(colour);
  state.waypoint_letter = static_cast<uint8_t>(letter);
  if(extended) {
    auto &w{state.weapons};
    std::cin >> w.selected >> w.rounds >> w.impact_time >> w.lock_status >> w.armed >> w.landed >> w.target_exists >> w.target_locked >> w.target_designated >> w.target_aircraft >> w.target_valid >> w.target.x >> w.target.y >> w.impact.x >> w.impact.y >> w.intercept.x >> w.intercept.y >> w.target_range >> w.target_speed >> w.box_half_size >> w.rocket_elevation >> w.tads_active >> w.tads_valid >> w.tads.x >> w.tads.y >> state.faults >> state.mode >> std::quoted(w.name) >> std::quoted(w.target_name);
    for(auto *vector : {&state.aircraft.position, &state.camera.position, &w.target_position, &w.impact_position, &w.target_heading}) {
      for(auto &coordinate : *vector) std::cin >> coordinate;
    }
    for(auto *rotation : {&state.aircraft.rotation, &state.camera.rotation}) {
      uint32_t h{0};
      uint32_t p{0};
      uint32_t r{0};
      std::cin >> h >> p >> r;
      *rotation = trig.transformation(h, p, r);
    }
  }
  if(!std::cin) throw std::runtime_error{"Incomplete helmet input"};
  return true;
}
std::unique_ptr<apache::render::model_instance> read_model_instance(apache::resources::model_archive const &models, unsigned int const depth = 0) {
  if(depth > 64) throw std::runtime_error{"Excessive captured model depth"};
  uint32_t id{0};
  std::cin >> id;
  if(!id) return {};
  uint32_t flags{0};
  bool alternate{false};
  apache::math::pose pose;
  apache::math::vector3 scale;
  int count{0};
  std::cin >> flags >> alternate;
  for(auto &v : pose.position) std::cin >> v;
  for(auto &v : pose.rotation) std::cin >> v;
  for(auto &v : scale) std::cin >> v;
  std::cin >> count;
  auto object{std::make_unique<apache::render::model_instance>(apache::render::construct_model(models, id, pose))};
  object->flags = flags;
  object->alternate_colours = alternate;
  object->scale = scale;
  if(count >= 0) {
    if(count > 1000) throw std::runtime_error{"Excessive captured model children"};
    object->children.clear();
    for(int i{0}; i != count; ++i) object->children.push_back(read_model_instance(models, depth + 1));
  }
  return object;
}
void word(apache::resources::bytes &output, uint32_t const value) {
  for(unsigned int shift{0}; shift != 32; shift += 8) output.push_back(static_cast<uint8_t>(value >> shift));
}
uint64_t geometry_fingerprint(apache::resources::model const &m) {
  apache::resources::bytes bytes;
  auto const append{[&](auto const value){word(bytes, static_cast<uint32_t>(value));}};
  append(m.kind);
  append(m.lod_distance);
  append(m.lod_next);
  for(auto const v : m.bounds) append(v);
  append(m.radius);
  append(m.height);
  for(auto const v : m.offsets) append(v);
  append(m.children.size());
  for(auto const &child : m.children) {
    append(child.model);
    for(auto const v : child.translation) append(v);
    for(auto const v : child.rotation) append(v);
  }
  append(m.vertices.size());
  for(auto const &vertex : m.vertices) {
    for(auto const v : vertex) append(v);
  }
  append(m.normals.size());
  append(m.shaded_normals);
  for(auto const &normal : m.normals) {
    for(auto const v : normal) append(v);
  }
  return apache::resources::fingerprint(bytes);
}
void inspect_models(apache::resources::data_store const &data, bool const traces) {
  auto const models{apache::resources::read_models(data.read("3DDATA/3DOBJS.BIN"))};
  for(size_t id{0}; id != models.size(); ++id) {
    if(!models[id]) continue;
    auto const &m{*models[id]};
    if(!traces) {
      std::cout << id << '\t' << m.name << '\t' << m.vertices.size() << '\t' << m.normals.size() << '\t' << m.program.size() << '\t' << std::hex << geometry_fingerprint(m) << std::dec << '\n';
      continue;
    }
    if(!m.offsets[4]) continue;
    std::set<uint32_t> states{0, 1, 2, 4, 8, 15};
    for(auto const &[pc, node] : m.program) {
      if(node.opcode == 5) {
        for(uint32_t i{0}; i != node.branches.size(); ++i) states.insert(i << node.shift);
      }
    }
    for(auto const flags : states) {
      for(int const winding : {1, -1}) {
        apache::resources::bytes events;
        apache::resources::drawing_callbacks const callbacks{
          .face{[&](apache::resources::primitive const &, uint32_t const offset, bool){
            word(events, 0);
            word(events, offset);
            return winding;
          }},
          .winding{[&](std::span<uint8_t const>){return winding;}},
          .child{[&](unsigned int const slot){
            word(events, 1);
            word(events, slot);
          }},
        };
        try {
          apache::resources::execute_model(m, flags, winding, callbacks);
        }
        catch(std::runtime_error const &error) {
          if(std::string_view{error.what()} == "Animation state has no authored case") continue;
          throw;
        }
        std::cout << id << '\t' << flags << '\t' << winding << '\t' << events.size() / 8 << '\t' << std::hex << apache::resources::fingerprint(events) << std::dec << '\n';
      }
    }
  }
}
} // namespace

int main(int const argc, char const *const argv[]) try {
  if(argc < 3) {
    std::cout << "Usage: apache_inspect DATA_DIRECTORY images|terrain|models|model-traces|matrices [RESOURCE [OUTPUT.ppm]]" << std::endl;
    return EXIT_FAILURE;
  }
  apache::resources::data_store const data{argv[1]};
  std::string_view const command{argv[2]};
  if(inspect_briefing(data, command, std::cin, std::cout, argc > 3 ? argv[3] : "")) return EXIT_SUCCESS;
  if(command == "chat") {
    apache::inspect_chat(data, std::cin, std::cout);
    return EXIT_SUCCESS;
  }
  if(command == "network-group") {
    apache::inspect_network_group(data, argc > 3 ? static_cast<unsigned int>(std::stoul(argv[3])) : 3, std::cout);
    return EXIT_SUCCESS;
  }
  if(command == "network-incomplete-flag") {
    apache::inspect_network_live(data, 5, std::cout, true);
    return EXIT_SUCCESS;
  }
  if(command == "network-loopback") {
    apache::inspect_network_live(data, argc > 3 ? static_cast<unsigned int>(std::stoul(argv[3])) : 4, std::cout);
    return EXIT_SUCCESS;
  }
  if(command == "network") {
    apache::inspect_network(std::cin, std::cout);
    return EXIT_SUCCESS;
  }
  if(command == "frontend") {
    inspect_frontend(data, std::cin, std::cout);
    return EXIT_SUCCESS;
  }
  if(command == "menu") {
    inspect_menu(data, std::cin, std::cout);
    return EXIT_SUCCESS;
  } else if(command == "debrief") {
    inspect_debrief(std::cin, std::cout);
    return EXIT_SUCCESS;
  }
  if(command == "logbook") {
    inspect_logbook(data, std::cin, std::cout);
    return EXIT_SUCCESS;
  }
  if(command == "audio") {
    inspect_audio(data, std::cin, std::cout);
    return EXIT_SUCCESS;
  }
  if(command == "communications") {
    inspect_communications(std::cin, std::cout);
    return EXIT_SUCCESS;
  }
  if(command == "session-network") {
    if(argc < 8) throw std::runtime_error{"session-network requires mission, video, flight model, game mode and connection slot"};
    inspect_session(data, std::stoi(argv[3]), static_cast<unsigned int>(std::stoul(argv[4])), std::stoi(argv[5]) == 2, std::cin, std::cout, -1, std::stoi(argv[6]), static_cast<unsigned int>(std::stoul(argv[7])), argc > 8 ? static_cast<unsigned int>(std::stoul(argv[8])) : 2);
    return EXIT_SUCCESS;
  }
  if(command == "session-shared") {
    if(argc < 7) throw std::runtime_error{"session-shared requires mission, video mode, flight model and seat"};
    inspect_session(data, std::stoi(argv[3]), static_cast<unsigned int>(std::stoul(argv[4])), std::stoi(argv[5]) == 2, std::cin, std::cout, std::stoi(argv[6]));
    return EXIT_SUCCESS;
  }
  if(command == "session") {
    if(argc < 6) throw std::runtime_error{"session requires mission, video mode and flight model"};
    inspect_session(data, std::stoi(argv[3]), static_cast<unsigned int>(std::stoul(argv[4])), std::stoi(argv[5]) == 2, std::cin, std::cout);
    return EXIT_SUCCESS;
  }
  if(command == "mission-sensors") {
    if(argc < 5) throw std::runtime_error{"mission-sensors requires a warzone and mission path"};
    inspect_sensors(data, argv[3], argv[4], std::cin, std::cout);
    return EXIT_SUCCESS;
  }
  if(command == "cameras") {
    inspect_cameras(data, std::cin, std::cout);
    return EXIT_SUCCESS;
  }
  if(command == "targeting") {
    inspect_targeting(data, std::cin, std::cout);
    return EXIT_SUCCESS;
  }
  if(command == "player-warnings") {
    inspect_player_warnings(std::cin, std::cout);
    return EXIT_SUCCESS;
  }
  if(command == "player-sight") {
    inspect_player_sight(data, std::cin, std::cout);
    return EXIT_SUCCESS;
  }
  if(command == "player-mounts") {
    inspect_player_mounts(data, std::cin, std::cout);
    return EXIT_SUCCESS;
  }
  if(command == "player-weapons") {
    inspect_player_weapons(data, std::cin, std::cout);
    return EXIT_SUCCESS;
  }
  if(command == "presentation") {
    std::string action;
    while(std::cin >> action) {
      if(action == "intro") {
        bool animations;
        bool skip;
        std::cin >> animations >> skip;
        for(auto const &clip : apache::frontend::intro_clips(data, animations, skip)) std::cout << clip.path << ' ' << clip.movie << ' ' << clip.title << '\n';
      } else if(action == "campaign") {
        int zone;
        int level;
        std::cin >> zone >> level;
        std::cout << apache::frontend::campaign_movie(data, zone, level) << '\n';
      } else {
        throw std::runtime_error{"Unknown presentation inspection command"};
      }
    }
    return EXIT_SUCCESS;
  }
  if(command == "animation") {
    if(argc < 4) throw std::runtime_error{"animation requires an original resource path"};
    apache::resources::animation movie{data.read(argv[3])};
    while(movie.advance()) {
      apache::resources::bytes palette;
      for(auto const &c : movie.image.colours) {
        palette.push_back(c.r);
        palette.push_back(c.g);
        palette.push_back(c.b);
      }
      std::cout << movie.position() - 1 << ' ' << apache::resources::fingerprint(movie.image.pixels) << ' ' << apache::resources::fingerprint(palette) << '\n';
    }
    return EXIT_SUCCESS;
  }
  if(command == "controls") {
    inspect_controls(std::cin, std::cout);
    return EXIT_SUCCESS;
  }
  if(command == "mission-combat" || command == "mission-weapons" || command == "mission-battle") {
    if(argc < 5) throw std::runtime_error{"Expected zone and mission path"};
    inspect_mission_combat(std::cin, std::cout, data, argv[3], argv[4], command != "mission-combat", command == "mission-battle");
    return EXIT_SUCCESS;
  }
  if(command == "mission-targets") {
    if(argc < 5) throw std::runtime_error{"Expected zone and mission path"};
    inspect_mission_targets(std::cin, std::cout, data, argv[3], argv[4]);
    return EXIT_SUCCESS;
  }
  if(command == "mission-spatial") {
    if(argc < 5) throw std::runtime_error{"Expected zone and mission path"};
    inspect_mission_spatial(std::cin, std::cout, data, argv[3], argv[4]);
    return EXIT_SUCCESS;
  }
  if(command == "mission-radar") {
    if(argc < 5) throw std::runtime_error{"Expected zone and mission path"};
    inspect_mission_radar(std::cout, data, argv[3], argv[4]);
    return EXIT_SUCCESS;
  }
  if(command == "mission-scenery") {
    if(argc < 5) throw std::runtime_error{"Expected zone and mission path"};
    inspect_mission_scenery(std::cout, data, argv[3], argv[4]);
    return EXIT_SUCCESS;
  }
  if(command == "combat-update") {
    inspect_combat_update(std::cin, std::cout, data);
    return EXIT_SUCCESS;
  }
  if(command == "war-grid") {
    inspect_war_grid(std::cin, std::cout, data);
    return EXIT_SUCCESS;
  }
  if(command == "battles") {
    inspect_battles(std::cin, std::cout, data);
    return EXIT_SUCCESS;
  }
  if(command == "patrol") {
    inspect_patrol(std::cin, std::cout, data);
    return EXIT_SUCCESS;
  }
  if(command == "actor-targeting") {
    inspect_actor_targeting(std::cin, std::cout, data);
    return EXIT_SUCCESS;
  }
  if(command == "target-search") {
    inspect_target_search(std::cin, std::cout);
    return EXIT_SUCCESS;
  }
  if(command == "actor-reactions") {
    inspect_actor_reactions(std::cin, std::cout, data);
    return EXIT_SUCCESS;
  }
  if(command == "actor-firing") {
    inspect_actor_firing(std::cin, std::cout, data);
    return EXIT_SUCCESS;
  }
  if(command == "firing") {
    inspect_firing(std::cin, std::cout, data);
    return EXIT_SUCCESS;
  }
  if(command == "combat-effects") {
    inspect_combat_effects(std::cin, std::cout, data);
    return EXIT_SUCCESS;
  }
  if(command == "blasts") {
    inspect_blasts(std::cin, std::cout, data);
    return EXIT_SUCCESS;
  }
  if(command == "damage") {
    inspect_damage(std::cin, std::cout, data);
    return EXIT_SUCCESS;
  }
  if(command == "countermeasures") {
    inspect_countermeasures(std::cin, std::cout, data);
    return EXIT_SUCCESS;
  }
  if(command == "tracking") {
    inspect_tracking(std::cin, std::cout, data);
    return EXIT_SUCCESS;
  }
  if(command == "targets") {
    inspect_targets(std::cin, std::cout, data);
  } else if(command == "projectile-pool") {
    inspect_projectile_pool(std::cin, std::cout, data);
  } else if(command == "projectiles") {
    inspect_projectiles(std::cin, std::cout, data);
  } else if(command == "seekers") {
    inspect_seekers(std::cin, std::cout, data);
  } else if(command == "collisions") {
    inspect_collisions(std::cin, std::cout, data);
  } else if(command == "briefings") {
    if(argc != 4) throw std::runtime_error{"briefings requires a bundle resource"};
    for(auto const &mission : apache::resources::read_mission_briefings(data.read(argv[3]))) std::cout << mission.id << ' ' << mission.flow << ' ' << mission.zone << ' ' << mission.level << ' ' << std::quoted(mission.title) << '\n';
  } else if(command == "mission-actors" || command == "mission-actor-state" || command == "mission-battles" || command == "ground-trace" || command == "actor-trace") {
    if(argc != 5) throw std::runtime_error{"mission-actors requires a warzone and mission resource"};
    auto const bytes{data.read(argv[4])};
    auto const mission{apache::resources::read_mission_definition({reinterpret_cast<char const *>(bytes.data()), bytes.size()})};
    auto const models{apache::resources::read_models(data.read("3DDATA/3DOBJS.BIN"))};
    auto const formations{apache::resources::read_formations(apache::resources::executable_data{data})};
    apache::math::trigonometry trig{data};
    std::string const zone{argv[3]};
    apache::resources::terrain heights{data.read(zone + ".WZ/GRIDPOST.BIN")};
    apache::resources::feature_layer features{data, zone, false};
    apache::resources::feature_layer super_features{data, zone, true};
    apache::simulation::actor_environment const environment{
      .height{[&](int32_t x, int32_t y){return heights.elevation(x, y);}},
      .surface{[&](int32_t x, int32_t y){return apache::resources::surface_type(features, super_features, x, y);}},
    };
    apache::simulation::mission_actors actors{mission, formations, models, trig, environment, 0};
    if(command == "mission-battles") {
      auto const battles{apache::simulation::initialise_battles(mission, actors.groups)};
      for(size_t i{0}; i != battles.size(); ++i) {
        std::cout << i << ' ' << battles[i].active;
        for(auto const &side : battles[i].sides) std::cout << ' ' << (side.task ? static_cast<int64_t>(*side.task) : -1) << ' ' << side.initial << ' ' << side.opponent_position[0] << ' ' << side.opponent_position[1] << ' ' << side.release_time;
        std::cout << '\n';
      }
      return EXIT_SUCCESS;
    }
    if(command == "ground-trace" || command == "actor-trace") {
      bool const all{command == "actor-trace"};
      apache::simulation::navigation_environment navigation_world{.height{environment.height}};
      apache::simulation::navigation_tables navigation_tables{apache::resources::executable_data{data}};
      apache::simulation::combat_random movement_random;
      apache::simulation::actor_movement movement{actors, mission, formations, models, trig, environment, navigation_world, navigation_tables, movement_random};
      apache::simulation::movement_time timing;
      if(all) actors.synchronize_models();
      std::string operation;
      size_t frame{0};
      std::vector<bool> ground_units(actors.units.size(), false);
      for(auto const &group : actors.groups) {
        auto const kind{actors.units[group.units.front()].classification};
        if(kind == 3 || kind == 4 || kind == 8) {
          for(auto id : group.units) ground_units[id] = true;
        }
      }
      while(std::cin >> operation) {
        if(operation == "step" || operation == "snapshot") {
          int32_t now{0};
          int32_t delta{0};
          bool snapshot{operation == "snapshot"};
          if(operation == "step") std::cin >> now >> delta >> snapshot;
          if(operation == "step" && all) {
            timing.now = now;
            timing.delta = delta;
            movement.update(timing);
            actors.synchronize_models();
          } else if(operation == "step") {
            for(auto const &group : actors.groups) {
              apache::simulation::ground_movement movement{actors.units, group, mission.tasks[group.task], formations[group.definition.formation], trig, environment};
              movement.update(now, delta);
            }
          }
          if(snapshot) {
            for(size_t id{0}; id != actors.units.size(); ++id) {
              auto const &unit{actors.units[id]};
              if(!all && !ground_units[id]) continue;
              std::cout << frame << ' ' << id << ' ' << unit.conditions;
              for(auto value : unit.object.world.position) std::cout << ' ' << value;
              for(auto value : unit.object.world.rotation) std::cout << ' ' << value;
              for(auto value : unit.angles) std::cout << ' ' << value;
              for(auto value : unit.velocity) std::cout << ' ' << value;
              std::cout << ' ' << unit.speed << ' ' << unit.route_distance << ' ' << (unit.waypoint ? static_cast<int>(*unit.waypoint) : -1) << ' ' << unit.destroyed;
              if(all) std::cout << ' ' << unit.desired_speed << ' ' << unit.speed_ramp_target << ' ' << unit.retreat_until;
              std::cout << '\n';
            }
          }
          ++frame;
        } else if(operation == "battle-end") {
          size_t task{0};
          bool advance{false};
          int32_t delta{0};
          std::cin >> task >> advance >> delta;
          for(auto const &group : actors.groups) {
            if(group.task == task) {
              apache::simulation::ground_movement transition{actors.units, group, mission.tasks[group.task], formations[group.definition.formation], trig, environment};
              transition.finish_battle(advance, delta);
            }
          }
        } else if(operation == "mode") {
          std::cin >> timing.arcade >> timing.game_flow;
        } else if(operation == "flags" || operation == "destroyed" || operation == "network" || operation == "retreat") {
          size_t id{0};
          int32_t value{0};
          std::cin >> id >> value;
          if(operation == "flags") {
            actors.units.at(id).conditions = static_cast<uint32_t>(value);
          } else if(operation == "destroyed") {
            actors.units.at(id).destroyed = value;
          } else if(operation == "retreat") {
            actors.units.at(id).retreat_until = value;
          } else {
            actors.units.at(id).network = value;
          }
        } else {
          throw std::runtime_error{"Unknown ground trace operation"};
        }
        if(!std::cin) throw std::runtime_error{"Incomplete ground trace operation"};
      }
      return EXIT_SUCCESS;
    }
    auto const pose{[](apache::math::pose const &value){
      for(auto v : value.position) std::cout << ' ' << v;
      for(auto v : value.rotation) std::cout << ' ' << v;
    }};
    auto const node{[&](auto const &self, apache::render::model_instance const &object, std::string const &path, bool relative)->void {
      std::cout << "node " << path << ' ' << object.id << ' ' << object.flags << ' ' << relative;
      if(relative) pose(object.relative);
      std::cout << '\n';
      for(size_t i{0}; i != object.children.size(); ++i) {
        if(object.children[i]) self(self, *object.children[i], path + "/" + std::to_string(i), true);
      }
    }};
    for(auto const &unit : actors.units) {
      if(command == "mission-actor-state") {
        std::cout << unit.location.task << ' ' << unit.location.group << ' ' << unit.location.member << ' ' << unit.classification << ' ' << unit.conditions;
        for(auto value : unit.angles) std::cout << ' ' << value;
        for(auto value : unit.velocity) std::cout << ' ' << value;
        std::cout << ' ' << unit.speed << ' ' << unit.desired_speed << ' ' << unit.route_distance;
        for(auto value : unit.ammunition) std::cout << ' ' << value;
        std::cout << ' ' << unit.weapon << ' ' << unit.weapon_slot << ' ' << unit.random << ' ' << (unit.carrier ? static_cast<int>(*unit.carrier) : -1) << ' ' << (unit.carrier_slot ? static_cast<int>(*unit.carrier_slot) : -1) << ' ' << (unit.waypoint ? static_cast<int>(*unit.waypoint) : -1) << '\n';
        continue;
      }
      std::cout << "unit " << unit.location.task << ' ' << unit.location.group << ' ' << unit.location.member << ' ' << unit.classification;
      pose(unit.object.world);
      std::cout << '\n';
      node(node, unit.object, "root", unit.carrier_slot.has_value());
    }
  } else if(command == "navigation") {
    inspect_navigation(std::cin, std::cout, data);
  } else if(command == "weapon-definitions") {
    inspect_weapon_definitions(std::cout);
  } else if(command == "weapon-rules") {
    inspect_weapon_rules(std::cin, std::cout, data);
  } else if(command == "actor-types") {
    auto const types{apache::simulation::actor_types()};
    for(size_t id{0}; id != types.size(); ++id) {
      auto const &type{types[id]};
      std::cout << id << ' ' << type.flags << ' ' << type.classification << ' ' << type.aircraft << ' ' << type.ship;
      for(auto const weapon : type.weapons) std::cout << ' ' << weapon;
      for(auto const load : type.ammunition) std::cout << ' ' << load;
      std::cout << ' ' << type.main_weapon << '\n';
    }
  } else if(command == "formations") {
    auto const all{apache::resources::read_formations(apache::resources::executable_data{data})};
    for(size_t id{0}; id != all.size(); ++id) {
      std::cout << id << ' ' << all[id].size();
      for(auto const &member : all[id]) {
        for(auto const v : member.offset) std::cout << ' ' << v;
        std::cout << ' ' << member.model;
      }
      std::cout << '\n';
    }
  } else if(command == "mission-data") {
    if(argc != 4) throw std::runtime_error{"mission-data requires a mission resource"};
    auto const bytes{data.read(argv[3])};
    auto const mission{apache::resources::read_mission_definition(std::string_view{reinterpret_cast<char const *>(bytes.data()), bytes.size()})};
    for(auto const &task : mission.tasks) {
      std::cout << "task " << task.side << ' ' << task.type << ' ' << task.battle_cell << ' ' << task.user << ' ' << task.actors.size() << '\n';
      for(auto const &actor : task.actors) {
        std::cout << "actor " << actor.model << ' ' << actor.formation << ' ' << actor.start_time << ' ' << actor.start_waypoint << ' ' << actor.conditions << ' ' << actor.waypoints.size() << '\n';
        for(auto const &w : actor.waypoints) std::cout << "waypoint " << w.type << ' ' << w.position[0] << ' ' << w.position[1] << ' ' << w.position[2] << ' ' << w.time << ' ' << w.speed << ' ' << w.interval << ' ' << w.distance << '\n';
      }
    }
    std::cout << "environment " << mission.wind_strength << ' ' << mission.wind_direction << ' ' << mission.weather << ' ' << mission.time_of_day << ' ' << mission.laser_timeout << ' ' << mission.gun << ' ' << mission.failure_count << ' ' << mission.success_count << ' ' << mission.outstanding_count << ' ' << mission.infinite_weapons;
    for(auto const weapon : mission.weapons) std::cout << ' ' << weapon;
    std::cout << '\n';
    for(auto const &[tag, rows] : mission.settings) {
      for(auto const &row : rows) {
        std::cout << "setting " << std::quoted(tag);
        for(auto const v : row) std::cout << ' ' << v;
        std::cout << '\n';
      }
    }
  } else if(command == "arcade-trace") {
    inspect_arcade_trace(std::cin, std::cout, data);
  } else if(command == "arcade-systems") {
    inspect_arcade(std::cin, std::cout);
  } else if(command == "flight-trace") {
    inspect_flight_trace(std::cin, std::cout, data);
  } else if(command == "flight-dynamics") {
    inspect_flight_dynamics(std::cin, std::cout);
  } else if(command == "flight-systems") {
    inspect_flight_systems(std::cin, std::cout);
  } else if(command == "helmet-components" || command == "helmet-weapons") {
    apache::resources::executable_data const executable{data};
    apache::math::trigonometry const trig{data};
    std::array<apache::render::helmet_display, 2> const helmets{apache::render::helmet_display{executable, trig, 0}, apache::render::helmet_display{executable, trig, 1}};
    unsigned int mode{0};
    unsigned int component{0};
    apache::render::helmet_instruments state;
    while(read_helmet(mode, component, state, trig, command == "helmet-weapons")) {
      apache::render::framebuffer frame{mode ? 640u : 320u, mode ? 480u : 240u};
      for(size_t i{0}; i != frame.image.pixels.size(); ++i) frame.image.pixels[i] = static_cast<uint8_t>(i * 7 + i / frame.image.width * 13);
      if(component == 21) {
        helmets[mode].draw(frame, state);
      } else {
        helmets[mode].draw_component(frame, state, static_cast<apache::render::helmet_component>(component));
      }
      std::cout << std::hex << apache::resources::fingerprint(frame.image.pixels) << std::dec << '\n';
      if(argc > 3) {
        std::ofstream output{argv[3], std::ios::binary};
        output.write(reinterpret_cast<char const *>(frame.image.pixels.data()), static_cast<std::streamsize>(frame.image.pixels.size()));
      }
    }
  } else if(command == "model-metadata") {
    apache::resources::executable_data const executable{data};
    auto models{apache::resources::read_models(data.read("3DDATA/3DOBJS.BIN"))};
    apache::resources::initialise_model_metadata(models, executable);
    for(size_t i{0}; i != models.size(); ++i) {
      if(models[i]) {
        auto const &object{*models[i]};
        std::cout << i << ' ' << std::quoted(object.name) << ' ' << std::quoted(object.description);
        for(auto const value : object.metadata) std::cout << ' ' << value;
        std::cout << '\n';
      }
    }
  } else if(command == "avionics") {
    apache::resources::executable_data const executable{data};
    apache::math::trigonometry const trig{data};
    auto const models{apache::resources::read_models(data.read("3DDATA/3DOBJS.BIN"))};
    std::array<apache::render::cockpit, 2> const panels{apache::render::cockpit{data, 0}, apache::render::cockpit{data, 1}};
    std::array<apache::render::avionics_renderer, 2> const displays{apache::render::avionics_renderer{panels[0], trig, executable}, apache::render::avionics_renderer{panels[1], trig, executable}};
    unsigned int mode{0};
    unsigned int component{0};
    apache::render::avionics_instruments state;
    std::optional<apache::render::landscape> world;
    std::string zone;
    std::string loaded;
    std::string background;
    while(read_helmet(mode, component, state.helmet, trig, true)) {
      unsigned int remap{0};
      apache::render::sky_lighting lighting;
      std::cin >> std::quoted(zone) >> std::quoted(background) >> state.clock_time >> state.accelerated_time >> state.damaged_display_seed >> state.map_range >> remap >> lighting.heading >> lighting.pitch >> lighting.night;
      for(auto &seat : state.displays.pages) {
        for(auto &page : seat) {
          unsigned int value{0};
          std::cin >> value;
          if(value > 9) throw std::runtime_error{"Invalid MFD mode"};
          page = static_cast<apache::simulation::mfd_page>(value);
        }
      }
      for(auto &text : state.communications) std::cin >> std::quoted(text);
      auto &e{state.engines};
      std::cin >> e.torque_left >> e.torque_right >> e.temperature_left >> e.temperature_right >> e.rpm_left >> e.rpm_right >> e.rotor_rpm >> e.collective;
      auto &w{state.stores};
      for(auto &pair : w.pairs) std::cin >> pair.type >> pair.rounds[0] >> pair.rounds[1];
      std::cin >> w.wingtip[0] >> w.wingtip[1] >> w.gun >> w.chaff >> w.flares >> w.armed >> w.auto_chaff >> w.auto_ecm;
      for(size_t i{0}; i != 2; ++i) {
        auto &r{i ? state.tactical : state.radar};
        auto &contacts{i ? state.tactical_contacts : state.radar_contacts};
        unsigned int kind{0};
        size_t count{0};
        std::cin >> kind >> r.range >> r.corner_range >> r.mark_length >> r.heading >> r.failed >> r.frozen >> r.tads >> r.locked >> r.scan_all >> r.threat >> r.range_index >> r.has_target >> std::quoted(r.target_name) >> r.target_distance >> count;
        if(kind > 3 || count > 1024) throw std::runtime_error{"Invalid avionics radar state"};
        r.mode = static_cast<apache::render::radar_mode>(kind);
        contacts.resize(count);
        for(auto &c : contacts) std::cin >> c.x >> c.y >> c.symbol >> c.selected;
      }
      size_t count{0};
      std::cin >> count >> state.waypoint;
      if(count > 10000) throw std::runtime_error{"Invalid avionics route"};
      state.route.resize(count);
      for(auto &point : state.route) {
        unsigned int letter{0};
        std::cin >> point.x >> point.y >> letter;
        point.label = static_cast<uint8_t>(letter);
      }
      for(auto *matrix : {&state.helmet.aircraft.rotation, &state.helmet.camera.rotation}) {
        for(auto &coefficient : *matrix) std::cin >> coefficient;
      }
      for(auto &coordinate : state.target_camera_aim) std::cin >> coordinate;
      auto object{read_model_instance(models)};
      state.target_model = object.get();
      if(!std::cin) throw std::runtime_error{"Incomplete avionics input"};
      if(zone != loaded) {
        world.emplace(data, zone);
        world->initialise_model_metadata(executable);
        loaded = zone;
      }
      world->palette.prepare(remap);
      world->missing_normals = world->invalid_texture_samples = 0;
      apache::render::framebuffer frame{mode ? 640u : 320u, mode ? 480u : 240u};
      std::ifstream image{background, std::ios::binary};
      image.read(reinterpret_cast<char *>(frame.image.pixels.data()), static_cast<std::streamsize>(frame.image.pixels.size()));
      if(!image) throw std::runtime_error{"Incomplete avionics background"};
      displays[mode].draw(frame, state, *world, lighting);
      std::cout << std::hex << apache::resources::fingerprint(frame.image.pixels) << std::dec << ' ' << state.damaged_display_seed;
      for(auto const &seat : state.displays.pages) {
        for(auto const page : seat) std::cout << ' ' << static_cast<unsigned int>(page);
      }
      std::cout << ' ' << world->missing_normals << ' ' << world->invalid_texture_samples << '\n';
      if(argc > 3) {
        std::ofstream output{argv[3], std::ios::binary};
        output.write(reinterpret_cast<char const *>(frame.image.pixels.data()), static_cast<std::streamsize>(frame.image.pixels.size()));
      }
    }
  } else if(command == "helmet-projection") {
    apache::resources::executable_data const executable{data};
    apache::math::trigonometry const trig{data};
    std::array<apache::render::helmet_display, 2> const helmets{apache::render::helmet_display{executable, trig, 0}, apache::render::helmet_display{executable, trig, 1}};
    unsigned int mode{0};
    unsigned int view{0};
    int x{0};
    int y{0};
    int width{0};
    int height{0};
    while(std::cin >> mode >> view >> x >> y >> width >> height) {
      apache::math::pose camera;
      apache::math::vector3 position;
      uint32_t h{0};
      uint32_t p{0};
      uint32_t r{0};
      for(auto &coordinate : camera.position) std::cin >> coordinate;
      std::cin >> h >> p >> r;
      camera.rotation = trig.transformation(h, p, r);
      for(auto &coordinate : position) std::cin >> coordinate;
      auto const point{helmets.at(mode).project_point(view, camera, apache::render::projection{x, y, width, height}, position)};
      std::cout << bool(point);
      if(point) std::cout << ' ' << point->x << ' ' << point->y;
      std::cout << '\n';
    }
  } else if(command == "instrument-state") {
    apache::math::trigonometry const trig{data};
    apache::simulation::instrument_inputs input;
    unsigned int letter{0};
    bool has_waypoint{false};
    apache::simulation::instrument_waypoint waypoint;
    while(std::cin >> input.position[0] >> input.position[1] >> input.position[2] >> input.heading >> input.pitch >> input.roll >> input.mission_time >> input.game_time >> input.terrain_height >> input.climb_rate >> input.airspeed >> input.sidespeed >> input.torque >> has_waypoint >> waypoint.x >> waypoint.y >> letter) {
      waypoint.letter = static_cast<uint8_t>(letter);
      input.waypoint = has_waypoint ? std::optional{waypoint} : std::nullopt;
      auto const state{apache::simulation::instrument_values(input, trig)};
      for(int64_t const value : {int64_t{state.heading}, int64_t{state.pitch}, int64_t{state.roll}, int64_t{state.barometric_altitude}, int64_t{state.clock_time}, int64_t{state.radar_altitude}, int64_t{state.climb_rate}, int64_t{state.command_heading}, int64_t{state.waypoint_letter}, int64_t{state.waypoint_distance}, int64_t{state.waypoint_time}, int64_t{state.airspeed}, int64_t{state.sidespeed}, int64_t{state.torque}}) std::cout << static_cast<uint32_t>(value) << ' ';
      std::cout << '\n';
    }
  } else if(command == "scenery-state") {
    std::string zone;
    while(std::cin >> zone) {
      auto placements{apache::resources::read_scenery(data.read(zone + ".WZ/GNDOBJS.BIN"))};
      apache::resources::initialise_scenery(placements);
      apache::resources::bytes state;
      for(auto const &p : placements) {
        for(uint32_t const value : {uint32_t{p.sector}, uint32_t{p.model}, static_cast<uint32_t>(p.x), static_cast<uint32_t>(p.y), uint32_t{p.heading}, uint32_t{p.animation}, uint32_t{p.status}}) word(state, value);
      }
      std::cout << placements.size() << ' ' << std::hex << apache::resources::fingerprint(state) << std::dec << '\n';
    }
  } else if(command == "map" || command == "mfd-map") {
    apache::resources::executable_data const executable{data};
    std::array<apache::render::cockpit, 2> const panels{apache::render::cockpit{data, 0}, apache::render::cockpit{data, 1}};
    std::array<apache::render::mfd_renderer, 2> const mfds{apache::render::mfd_renderer{panels[0], executable}, apache::render::mfd_renderer{panels[1], executable}};
    std::string zone;
    std::string loaded;
    std::unique_ptr<apache::render::landscape> world;
    unsigned int mode{0};
    unsigned int panel{0};
    int32_t x{0};
    int32_t y{0};
    int32_t range{0};
    uint32_t heading{0};
    apache::render::map_configuration config;
    while(std::cin >> zone >> mode >> panel >> x >> y >> range >> heading >> config.flags >> config.contour_interval) {
      if(mode > 1 || panel > 5 || range <= 0 || range > 0x3fffffff) throw std::runtime_error{"Invalid map case"};
      if(loaded != zone) {
        world = std::make_unique<apache::render::landscape>(data, zone);
        loaded = zone;
      }
      apache::render::framebuffer frame{mode ? 640u : 320u, mode ? 480u : 240u};
      for(size_t i{0}; i != frame.image.pixels.size(); ++i) frame.image.pixels[i] = static_cast<uint8_t>(i * 7 + i / frame.image.width * 13);
      auto const area{panel == 5 ? frame.clip : mfds[mode].area(static_cast<apache::render::mfd_panel>(panel))};
      if(command == "mfd-map") {
        mfds[mode].moving_map(frame, area, *world, x, y, heading, range);
      } else {
        world->draw_map(frame, area, {apache::math::subtract(x, range), apache::math::subtract(y, range), apache::math::add(x, range), apache::math::add(y, range)}, heading, config);
      }
      std::cout << std::hex << apache::resources::fingerprint(frame.image.pixels) << std::dec << '\n';
      if(argc > 3) {
        std::ofstream output{argv[3], std::ios::binary};
        output.write(reinterpret_cast<char const *>(frame.image.pixels.data()), static_cast<std::streamsize>(frame.image.pixels.size()));
      }
    }
  } else if(command == "contours") {
    int32_t interval{0};
    apache::render::rectangle area;
    apache::render::window2 window;
    while(std::cin >> interval >> area.left >> area.top >> area.right >> area.bottom >> window.left >> window.bottom >> window.right >> window.top) {
      apache::render::framebuffer frame{320, 240};
      frame.clip = area;
      for(size_t i{0}; i != frame.image.pixels.size(); ++i) frame.image.pixels[i] = static_cast<uint8_t>(i * 7 + i / frame.image.width * 13);
      std::array<apache::render::vertex2, 3> triangle;
      for(auto &point : triangle) std::cin >> point.x >> point.y >> point.intensity;
      apache::render::draw_map_contours(frame, apache::render::projection2{area, window}, triangle, interval);
      std::cout << std::hex << apache::resources::fingerprint(frame.image.pixels) << std::dec << '\n';
      if(argc > 3) {
        std::ofstream output{argv[3], std::ios::binary};
        output.write(reinterpret_cast<char const *>(frame.image.pixels.data()), static_cast<std::streamsize>(frame.image.pixels.size()));
      }
    }
  } else if(command == "landscape" || command == "scene" || command == "atmosphere" || command == "scene-effects" || command == "scene-mobiles" || command == "scene-craters" || (command == "scene-fragments" || command == "scene-rgb")) {
    apache::math::trigonometry const trig{data};
    auto const fragment_models{(command == "scene-fragments" || command == "scene-rgb") ? apache::resources::read_models(data.read("3DDATA/3DOBJS.BIN")) : apache::resources::model_archive{}};
    std::string zone;
    std::string loaded;
    std::unique_ptr<apache::render::landscape> world;
    apache::math::pose camera;
    uint32_t heading{0};
    uint32_t pitch{0};
    uint32_t roll{0};
    apache::render::landscape_options options;
    options.scenery = command == "scene";
    options.celestial_bodies = command == "atmosphere" || command == "scene-effects" || command == "scene-mobiles" || command == "scene-craters" || (command == "scene-fragments" || command == "scene-rgb");
    while(std::cin >> zone >> camera.position[0] >> camera.position[1] >> camera.position[2] >> heading >> pitch >> roll >> options.distance >> options.terrain_detail >> options.features) {
      if(options.celestial_bodies) std::cin >> options.sky.cloud_flags >> options.sky.weather >> options.cloud_height >> options.cloud_bottom >> options.cloud_top >> options.lighting.heading >> options.lighting.pitch >> options.lighting.night;
      if(command == "scene-rgb") std::cin >> options.infrared >> options.cockpit_glass;
      if(!world || loaded != zone) {
        world = std::make_unique<apache::render::landscape>(data, zone);
        loaded = zone;
      }
      if(command == "scene-effects" || command == "scene-mobiles" || command == "scene-craters" || (command == "scene-fragments" || command == "scene-rgb")) {
        world->effects.reset();
        size_t count{0};
        size_t puffs{0};
        size_t blasts{0};
        std::cin >> count;
        for(size_t i{0}; i != count; ++i) {
          unsigned int kind{0};
          apache::math::vector3 position;
          int32_t radius{0};
          int32_t age{0};
          int32_t lifetime{0};
          uint32_t angle{0};
          std::cin >> kind >> position[0] >> position[1] >> position[2] >> radius >> age >> lifetime >> angle;
          if(kind == 3) {
            world->effects.smoke_puffs.at(puffs++) = {true, position, radius, lifetime, age, angle};
          } else {
            world->effects.explosions.at(blasts++) = {kind, position, {0, 0, 0}, radius, age};
          }
        }
      }
      if(command == "scene-mobiles" || command == "scene-craters" || (command == "scene-fragments" || command == "scene-rgb")) {
        world->mobiles.clear();
        size_t count{0};
        int64_t hidden{-1};
        std::cin >> count >> hidden;
        options.hidden_mobile = hidden < 0 ? std::nullopt : std::optional{static_cast<uint32_t>(hidden)};
        for(size_t i{0}; i != count; ++i) {
          uint32_t model{0};
          uint32_t flags{0};
          uint32_t h{0};
          uint32_t p{0};
          uint32_t r{0};
          bool alternate{false};
          apache::math::pose pose;
          std::cin >> model >> flags >> pose.position[0] >> pose.position[1] >> pose.position[2] >> h >> p >> r >> alternate;
          pose.rotation = trig.transformation(h, p, r);
          auto &object{world->mobiles.at(world->mobiles.add(model, pose))};
          object.flags = flags;
          object.alternate_colours = alternate;
        }
      }
      if(command == "scene-craters" || (command == "scene-fragments" || command == "scene-rgb")) {
        world->craters.reset();
        size_t count{0};
        size_t short_count{0};
        size_t long_count{0};
        std::cin >> count;
        for(size_t i{0}; i != count; ++i) {
          bool temporary{false};
          apache::simulation::crater item;
          std::cin >> temporary >> item.model >> item.position[0] >> item.position[1] >> item.position[2] >> item.heading >> item.age >> item.scale[0] >> item.scale[1] >> item.scale[2];
          (temporary ? world->craters.short_lived.at(short_count++) : world->craters.long_lived.at(long_count++)) = item;
        }
      }
      if((command == "scene-fragments" || command == "scene-rgb")) {
        world->fragments.reset();
        uint32_t seed{0};
        int32_t delta{0};
        size_t steps{0};
        size_t count{0};
        std::cin >> seed >> delta >> steps >> count;
        for(size_t i{0}; i != count; ++i) {
          bool tree{false};
          uint32_t model{0};
          uint32_t flags{0};
          uint32_t h{0};
          uint32_t p{0};
          uint32_t r{0};
          apache::math::pose pose;
          std::cin >> tree >> model >> flags >> pose.position[0] >> pose.position[1] >> pose.position[2] >> h >> p >> r;
          pose.rotation = trig.transformation(h, p, r);
          if(tree) {
            auto object{apache::render::construct_model(fragment_models, model, pose)};
            object.flags = flags;
            apache::render::maintain_model(object);
            apache::render::fracture_model(object, world->fragments, seed);
          } else {
            world->fragments.create(model, flags, pose, seed);
          }
        }
        for(size_t i{0}; i != steps; ++i) world->advance(delta, seed);
      }
      apache::render::framebuffer frame{320, 240};
      for(size_t i{0}; i != 256; ++i) frame.image.colours[i] = {world->palette.colours[i * 3], world->palette.colours[i * 3 + 1], world->palette.colours[i * 3 + 2]};
      frame.clear(255);
      camera.rotation = trig.transformation(heading, pitch, roll);
      world->draw(frame, camera, options);
      std::cout << std::hex << apache::resources::fingerprint(frame.image.pixels) << std::dec;
      if(options.celestial_bodies) std::cout << ' ' << world->sun_glare;
      if(command == "scene" || command == "scene-effects" || command == "scene-mobiles" || command == "scene-craters" || (command == "scene-fragments" || command == "scene-rgb")) std::cout << ' ' << world->missing_normals << ' ' << world->invalid_texture_samples;
      if(command == "scene-rgb") {
        apache::resources::bytes rgb;
        rgb.reserve(frame.image.pixels.size() * 3);
        for(auto const pixel : frame.image.pixels) {
          auto const &colour{frame.image.colours[pixel]};
          rgb.push_back(colour.r);
          rgb.push_back(colour.g);
          rgb.push_back(colour.b);
        }
        std::cout << ' ' << std::hex << apache::resources::fingerprint(rgb) << ' ' << apache::resources::fingerprint(world->palette.modified) << std::dec;
      }
      std::cout << '\n';
      if(argc > 3) {
        apache::resources::write_ppm(argv[3], frame.image);
        std::ofstream file{std::string{argv[3]} + ".pixels", std::ios::binary};
        file.write(reinterpret_cast<char const *>(frame.image.pixels.data()), static_cast<std::streamsize>(frame.image.pixels.size()));
      }
    }
  } else if(command == "mfd-state") {
    unsigned int operation{0};
    unsigned int game_mode{0};
    bool gunner{false};
    bool right{false};
    bool forward{false};
    bool requested{false};
    uint32_t faults{0};
    while(std::cin >> operation >> game_mode >> gunner >> right >> forward >> requested >> faults) {
      apache::simulation::mfd_state state;
      state.forwards = forward;
      for(auto &seat : state.pages) {
        for(auto &page : seat) {
          unsigned int value{0};
          std::cin >> value;
          if(value > 9) throw std::runtime_error{"Invalid MFD page"};
          page = static_cast<apache::simulation::mfd_page>(value);
        }
      }
      if(operation == 0) {
        state.initialise(game_mode);
      } else if(operation == 1) {
        state.cycle(gunner, right, requested);
      } else if(operation == 2) {
        state.toggle(gunner, right, faults);
      } else if(operation == 3) {
        state.show_radar(gunner, faults);
      } else if(operation == 4) {
        state.apply_faults(gunner, faults);
      } else if(operation == 5) {
        state.reset();
      } else {
        throw std::runtime_error{"Invalid MFD control operation"};
      }
      for(auto const &seat : state.pages) {
        for(auto const page : seat) std::cout << static_cast<unsigned int>(page) << ' ';
      }
      std::cout << state.forwards << '\n';
    }
  } else if(command == "mfd-flir") {
    apache::resources::executable_data const executable{data};
    apache::math::trigonometry const trig{data};
    auto const models{apache::resources::read_models(data.read("3DDATA/3DOBJS.BIN"))};
    std::array<apache::render::cockpit, 2> const panels{apache::render::cockpit{data, 0}, apache::render::cockpit{data, 1}};
    std::array<apache::render::mfd_renderer, 2> const mfds{apache::render::mfd_renderer{panels[0], executable}, apache::render::mfd_renderer{panels[1], executable}};
    std::string zone;
    std::string loaded;
    std::optional<apache::render::landscape> world;
    unsigned int mode{0};
    unsigned int panel{0};
    unsigned int kind{0};
    unsigned int id{0};
    unsigned int remap{0};
    uint32_t flags{0};
    uint32_t heading{0};
    uint32_t pitch{0};
    uint32_t roll{0};
    bool alternate{false};
    apache::math::pose position;
    apache::math::vector3 aim;
    apache::math::vector3 observer;
    apache::render::sky_lighting lighting;
    while(std::cin >> zone >> mode >> panel >> kind >> id >> flags >> alternate >> remap) {
      if(mode > 1 || panel > 4 || kind > 2) throw std::runtime_error{"Invalid FLIR case"};
      for(auto &axis : position.position) std::cin >> axis;
      std::cin >> heading >> pitch >> roll;
      for(auto &axis : aim) std::cin >> axis;
      for(auto &axis : observer) std::cin >> axis;
      std::cin >> lighting.heading >> lighting.pitch >> lighting.night;
      if(zone != loaded) {
        world.emplace(data, zone);
        world->initialise_model_metadata(executable);
        loaded = zone;
      }
      world->palette.prepare(remap);
      world->missing_normals = world->invalid_texture_samples = 0;
      position.rotation = kind == 1 ? apache::math::matrix3{trig.cosine(heading), apache::math::negate(trig.sine(heading)), 0, trig.sine(heading), trig.cosine(heading), 0, 0, 0, INT32_MAX} : trig.transformation(heading, pitch, roll);
      auto object{apache::render::construct_model(models, id, position)};
      object.flags = flags;
      object.alternate_colours = alternate;
      apache::render::framebuffer frame{mode ? 640u : 320u, mode ? 480u : 240u};
      for(size_t i{0}; i != frame.image.pixels.size(); ++i) frame.image.pixels[i] = static_cast<uint8_t>(i * 7 + i / frame.image.width * 13);
      mfds[mode].flir(frame, mfds[mode].area(static_cast<apache::render::mfd_panel>(panel)), *world, kind ? &object : nullptr, aim, observer, lighting);
      std::cout << std::hex << apache::resources::fingerprint(frame.image.pixels) << std::dec << ' ' << world->missing_normals << ' ' << world->invalid_texture_samples << '\n';
      if(argc > 3) {
        std::ofstream file{argv[3], std::ios::binary};
        file.write(reinterpret_cast<char const *>(frame.image.pixels.data()), static_cast<std::streamsize>(frame.image.pixels.size()));
      }
    }
  } else if(command == "panel-status" || command == "mfd-basic" || command == "mfd-weapons" || command == "mfd-radar" || command == "mfd-flight-plan") {
    apache::resources::executable_data const executable{data};
    apache::math::trigonometry const trig{data};
    std::array<apache::render::cockpit, 2> const panels{apache::render::cockpit{data, 0}, apache::render::cockpit{data, 1}};
    std::array<apache::render::panel_instruments, 2> const gauges{apache::render::panel_instruments{panels[0], trig, executable}, apache::render::panel_instruments{panels[1], trig, executable}};
    std::array<apache::render::mfd_renderer, 2> const mfds{apache::render::mfd_renderer{panels[0], executable}, apache::render::mfd_renderer{panels[1], executable}};
    unsigned int mode{0};
    unsigned int which{0};
    while(std::cin >> mode >> which) {
      if(mode > 1) throw std::runtime_error{"Invalid panel mode"};
      apache::render::framebuffer frame{mode ? 640u : 320u, mode ? 480u : 240u};
      for(size_t i{0}; i != frame.image.pixels.size(); ++i) frame.image.pixels[i] = static_cast<uint8_t>(i * 7 + i / frame.image.width * 13);
      uint32_t seed{0};
      if(command == "mfd-flight-plan") {
        size_t count{0};
        size_t current{0};
        int32_t x{0};
        int32_t y{0};
        int32_t airspeed{0};
        uint32_t heading{0};
        std::cin >> current >> x >> y >> heading >> airspeed >> count;
        if(count > 10000) throw std::runtime_error{"Invalid waypoint count"};
        std::vector<apache::render::waypoint_mark> route(count);
        for(auto &point : route) {
          unsigned int code{0};
          std::cin >> point.x >> point.y >> code;
          point.label = static_cast<uint8_t>(code);
        }
        mfds[mode].flight_plan(frame, mfds[mode].area(static_cast<apache::render::mfd_panel>(which)), route, current, x, y, heading, airspeed, trig);
      } else if(command == "mfd-radar") {
        apache::render::radar_instruments state;
        unsigned int kind{0};
        size_t count{0};
        std::cin >> kind >> state.range >> state.corner_range >> state.mark_length >> state.heading >> state.failed >> state.frozen >> state.tads >> state.locked >> state.scan_all >> state.threat >> state.range_index >> state.has_target >> std::quoted(state.target_name) >> state.target_distance >> count;
        if(kind > 3 || count > 1024) throw std::runtime_error{"Invalid radar case"};
        state.mode = static_cast<apache::render::radar_mode>(kind);
        std::vector<apache::render::radar_contact> contacts(count);
        for(auto &contact : contacts) std::cin >> contact.x >> contact.y >> contact.symbol >> contact.selected;
        mfds[mode].radar(frame, mfds[mode].area(static_cast<apache::render::mfd_panel>(which)), state, contacts, trig);
      } else if(command == "mfd-weapons") {
        apache::render::weapon_instruments state;
        for(auto &pair : state.pairs) std::cin >> pair.type >> pair.rounds[0] >> pair.rounds[1];
        std::cin >> state.wingtip[0] >> state.wingtip[1] >> state.gun >> state.chaff >> state.flares >> state.armed >> state.auto_chaff >> state.auto_ecm;
        mfds[mode].weapons(frame, mfds[mode].area(static_cast<apache::render::mfd_panel>(which)), state);
      } else if(command == "panel-status") {
        bool gunner{false};
        uint32_t faults{0};
        unsigned int acceleration{0};
        std::array<std::string, 3> lines;
        std::cin >> gunner >> faults >> acceleration;
        for(auto &line : lines) std::cin >> std::quoted(line);
        if(which == 0 || which == 3) gauges[mode].warnings(frame, gunner, faults);
        if(which == 1 || which == 3) gauges[mode].communications(frame, gunner, lines);
        if(which == 2 || which == 3) gauges[mode].accelerated_time(frame, acceleration);
      } else {
        unsigned int panel{0};
        uint32_t faults{0};
        bool ground{false};
        bool air{false};
        apache::render::engine_instruments state;
        std::cin >> panel >> faults >> ground >> air >> seed >> state.torque_left >> state.torque_right >> state.temperature_left >> state.temperature_right >> state.rpm_left >> state.rpm_right >> state.rotor_rpm >> state.collective;
        auto const area{mfds[mode].area(static_cast<apache::render::mfd_panel>(panel))};
        if(which == 0) {
          mfds[mode].off(frame, area);
        } else if(which == 1) {
          mfds[mode].damaged(frame, area, seed);
        } else if(which == 8) {
          mfds[mode].engines(frame, area, state);
        } else if(which == 9) {
          mfds[mode].status(frame, area, faults, ground, air);
        } else if(which == 10) {
          mfds[mode].corners(frame, panel != 0);
        } else {
          throw std::runtime_error{"Invalid MFD page"};
        }
      }
      std::cout << std::hex << apache::resources::fingerprint(frame.image.pixels) << std::dec << ' ' << seed << '\n';
      if(argc > 3) {
        std::ofstream file{argv[3], std::ios::binary};
        file.write(reinterpret_cast<char const *>(frame.image.pixels.data()), static_cast<std::streamsize>(frame.image.pixels.size()));
      }
    }
  } else if(command == "instruments") {
    apache::resources::executable_data const executable{data};
    apache::math::trigonometry const trig{data};
    std::array<apache::render::cockpit, 2> const panels{apache::render::cockpit{data, 0}, apache::render::cockpit{data, 1}};
    std::array<apache::render::panel_instruments, 2> const gauges{apache::render::panel_instruments{panels[0], trig, executable}, apache::render::panel_instruments{panels[1], trig, executable}};
    unsigned int mode{0};
    unsigned int which{0};
    int32_t speed{0};
    int32_t altitude{0};
    int32_t seconds{0};
    uint32_t heading{0};
    uint32_t pitch{0};
    uint32_t roll{0};
    while(std::cin >> mode >> which >> speed >> altitude >> seconds >> heading >> pitch >> roll) {
      if(mode > 1 || which > 5) throw std::runtime_error{"Invalid instrument selection"};
      apache::render::framebuffer frame{mode ? 640u : 320u, mode ? 480u : 240u};
      for(size_t i{0}; i != frame.image.pixels.size(); ++i) frame.image.pixels[i] = static_cast<uint8_t>(i * 7 + i / frame.image.width * 13);
      auto const &gauge{gauges[mode]};
      if(which == 0 || which == 5) gauge.airspeed(frame, speed);
      if(which == 1 || which == 5) gauge.altimeter(frame, altitude);
      if(which == 2 || which == 5) gauge.clock(frame, seconds);
      if(which == 3 || which == 5) gauge.compass(frame, heading);
      if(which == 4 || which == 5) gauge.attitude(frame, trig.transformation(heading, pitch, roll));
      std::cout << std::hex << apache::resources::fingerprint(frame.image.pixels) << std::dec << '\n';
      if(argc > 3) {
        std::ofstream file{argv[3], std::ios::binary};
        file.write(reinterpret_cast<char const *>(frame.image.pixels.data()), static_cast<std::streamsize>(frame.image.pixels.size()));
      }
    }
  } else if(command == "cockpits") {
    std::array<apache::render::cockpit, 2> const cockpits{apache::render::cockpit{data, 0}, apache::render::cockpit{data, 1}};
    unsigned int mode{0};
    unsigned int view{0};
    unsigned int pattern{0};
    bool fresh{false};
    bool pnvs{false};
    int32_t night{0};
    while(std::cin >> mode >> view >> fresh >> pattern >> night >> pnvs) {
      if(mode > 1) throw std::runtime_error{"Invalid cockpit mode"};
      apache::render::framebuffer frame{mode ? 640u : 320u, mode ? 480u : 240u};
      for(size_t i{0}; i != frame.image.pixels.size(); ++i) frame.image.pixels[i] = pattern ? static_cast<uint8_t>(i * 7 + i / frame.image.width * 13) : 0;
      cockpits[mode].draw(frame, view, fresh);
      apache::render::palette_rgb colours{};
      cockpits[mode].install_palette(colours);
      colours = apache::render::apply_faders(colours, cockpits[mode].faders(night, pnvs));
      std::cout << std::hex << apache::resources::fingerprint(frame.image.pixels) << ' ' << apache::resources::fingerprint(colours) << std::dec << '\n';
      if(argc > 3) {
        for(size_t i{0}; i != 256; ++i) frame.image.colours[i] = {colours[i * 3], colours[i * 3 + 1], colours[i * 3 + 2]};
        apache::resources::write_ppm(argv[3], frame.image);
      }
    }
  } else if(command == "instrument-fonts") {
    apache::resources::executable_data const executable{data};
    std::array<apache::render::instrument_font, 2> const fonts{apache::render::instrument_font{executable, false}, apache::render::instrument_font{executable, true}};
    unsigned int large{0};
    unsigned int code{0};
    unsigned int colour{0};
    int x{0};
    int y{0};
    apache::render::framebuffer frame{32, 32};
    while(std::cin >> large >> code >> x >> y >> colour) {
      frame.clear(0);
      fonts.at(large).character(frame, static_cast<uint8_t>(code), x, y, static_cast<uint8_t>(colour));
      std::cout << std::hex << apache::resources::fingerprint(frame.image.pixels) << std::dec << '\n';
    }
  } else if(command == "scene-palette") {
    std::string zone;
    std::string loaded;
    bool overcast{false};
    bool previous{false};
    apache::render::palette_conditions conditions;
    std::optional<apache::render::scene_palette> palette;
    while(std::cin >> zone >> overcast >> conditions.cloud_mode >> conditions.cloud_flags >> conditions.weather >> conditions.altitude >> conditions.cloud_bottom >> conditions.cloud_top >> conditions.night >> conditions.infrared >> conditions.sun_glare >> conditions.cockpit_glass) {
      if(!palette || loaded != zone || previous != overcast) {
        palette.emplace(data, zone, overcast);
        loaded = zone;
        previous = overcast;
      }
      palette->prepare(conditions.infrared);
      palette->update(conditions);
      std::cout << std::hex << apache::resources::fingerprint(palette->colours) << ' ' << apache::resources::fingerprint(palette->modified) << ' ' << apache::resources::fingerprint(palette->infrared) << ' ' << apache::resources::fingerprint(palette->mfd_infrared) << ' ' << apache::resources::fingerprint(palette->hud_nvs) << ' ' << apache::resources::fingerprint(palette->visual_remap) << std::dec << '\n';
      if(argc > 3) {
        std::ofstream file{argv[3], std::ios::binary};
        file.write(reinterpret_cast<char const *>(palette->modified.data()), palette->modified.size());
      }
    }
  } else if(command == "faders") {
    uint32_t seed{0};
    size_t count{0};
    while(std::cin >> seed >> count) {
      apache::render::palette_rgb colours;
      for(auto &value : colours) {
        seed = seed * 0x41c64e6du + 0x3039u;
        value = static_cast<uint8_t>(seed >> 16);
      }
      std::vector<apache::render::palette_fader> faders(count);
      for(auto &fader : faders) {
        unsigned int r{0};
        unsigned int g{0};
        unsigned int b{0};
        std::cin >> fader.monochrome >> fader.priority >> fader.first >> fader.count >> fader.level >> r >> g >> b;
        fader.colour = {static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b)};
      }
      auto const result{apache::render::apply_faders(colours, faders)};
      std::cout << std::hex << apache::resources::fingerprint(result) << std::dec << '\n';
    }
  } else if(command == "fragment-state") {
    apache::math::trigonometry const trig{data};
    auto const models{apache::resources::read_models(data.read("3DDATA/3DOBJS.BIN"))};
    std::unique_ptr<apache::render::landscape> world;
    uint32_t seed{1};
    std::string operation;
    while(std::cin >> operation) {
      if(operation == "reset") {
        std::string zone;
        std::cin >> zone >> seed;
        world = std::make_unique<apache::render::landscape>(data, zone);
      } else {
        if(!world) throw std::runtime_error{"Reset the fragment scene first"};
        if(operation == "advance") {
          int32_t delta{0};
          std::cin >> delta;
          world->advance(delta, seed);
        } else if(operation == "state") {
          apache::resources::bytes bytes;
          size_t pieces{0};
          size_t splashes{0};
          for(size_t i{0}; i != world->fragments.pieces.size(); ++i) {
            auto const &piece{world->fragments.pieces[i]};
            if(!piece.active) continue;
            ++pieces;
            word(bytes, static_cast<uint32_t>(i));
            word(bytes, 1);
            word(bytes, piece.model);
            word(bytes, piece.flags);
            for(auto const v : piece.position.position) word(bytes, static_cast<uint32_t>(v));
            for(auto const v : piece.position.rotation) word(bytes, static_cast<uint32_t>(v));
            for(auto const v : piece.axis) word(bytes, static_cast<uint32_t>(v));
            word(bytes, static_cast<uint32_t>(piece.spin));
            for(auto const v : piece.velocity) word(bytes, static_cast<uint32_t>(v));
          }
          for(size_t i{0}; i != world->craters.short_lived.size(); ++i) {
            auto const &item{world->craters.short_lived[i]};
            if(item.model < 0) continue;
            ++splashes;
            word(bytes, 65536u + static_cast<uint32_t>(i));
            for(auto const v : item.position) word(bytes, static_cast<uint32_t>(v));
            word(bytes, static_cast<uint32_t>(item.model));
            word(bytes, item.heading);
            word(bytes, static_cast<uint32_t>(item.age));
            for(auto const v : item.scale) word(bytes, static_cast<uint32_t>(v));
          }
          std::cout << seed << ' ' << pieces << ' ' << splashes << ' ' << std::hex << apache::resources::fingerprint(bytes) << std::dec << '\n';
        } else if(operation == "single" || operation == "tree") {
          uint32_t id{0};
          uint32_t flags{0};
          uint32_t h{0};
          uint32_t p{0};
          uint32_t r{0};
          apache::math::pose pose;
          std::cin >> id >> flags >> pose.position[0] >> pose.position[1] >> pose.position[2] >> h >> p >> r;
          pose.rotation = trig.transformation(h, p, r);
          if(operation == "single") {
            world->fragments.create(id, flags, pose, seed);
          } else {
            auto object{apache::render::construct_model(models, id, pose)};
            object.flags = flags;
            apache::render::maintain_model(object);
            apache::render::fracture_model(object, world->fragments, seed);
          }
        } else {
          throw std::runtime_error{"Unknown fragment-state operation"};
        }
      }
    }
  } else if(command == "integer-sqrt") {
    uint64_t value{0};
    while(std::cin >> value) std::cout << apache::math::integer_sqrt(value) << '\n';
  } else if(command == "vector-math" || command == "direction-math") {
    apache::math::trigonometry const trig{data};
    apache::math::vector3 vector;
    uint32_t angle{0};
    while(std::cin >> vector[0] >> vector[1] >> vector[2] >> angle) {
      auto const axis{apache::math::normalise_q31(vector)};
      for(auto const v : axis) std::cout << v << ' ';
      auto const matrix{command == "direction-math" ? apache::math::direction_matrix(axis) : trig.rotation(axis, angle)};
      for(size_t i{0}; i != matrix.size(); ++i) std::cout << matrix[i] << (i + 1 == matrix.size() ? '\n' : ' ');
    }
  } else if(command == "surface-types") {
    std::string zone;
    std::string loaded;
    int32_t x{0};
    int32_t y{0};
    std::optional<apache::resources::feature_layer> normal;
    std::optional<apache::resources::feature_layer> super;
    while(std::cin >> zone >> x >> y) {
      if(loaded != zone) {
        normal.emplace(data, zone, false);
        super.emplace(data, zone, true);
        loaded = zone;
      }
      std::cout << static_cast<unsigned int>(apache::resources::surface_type(*normal, *super, x, y)) << '\n';
    }
  } else if(command == "crater-state") {
    apache::simulation::craters craters;
    std::optional<apache::resources::terrain> terrain;
    uint32_t seed{1};
    std::string operation;
    while(std::cin >> operation) {
      if(operation == "reset") {
        std::string zone;
        std::cin >> zone >> seed;
        terrain.emplace(data.read(zone + ".WZ/GRIDPOST.BIN"));
        craters = {};
        craters.reset();
      } else if(operation == "clear") {
        craters.reset();
      } else if(operation == "advance") {
        craters.advance();
      } else if(operation == "state") {
        apache::resources::bytes bytes;
        size_t count{0};
        for(size_t i{0}; i != 256; ++i) {
          auto const &item{i < 128 ? craters.short_lived[i] : craters.long_lived[i - 128]};
          if(item.model < 0) continue;
          ++count;
          word(bytes, static_cast<uint32_t>(i));
          for(auto const v : item.position) word(bytes, static_cast<uint32_t>(v));
          word(bytes, static_cast<uint32_t>(item.model));
          word(bytes, item.heading);
          word(bytes, static_cast<uint32_t>(item.age));
          for(auto const v : item.scale) word(bytes, static_cast<uint32_t>(v));
        }
        std::cout << seed << ' ' << count << ' ' << std::hex << apache::resources::fingerprint(bytes) << std::dec << '\n';
      } else {
        if(!terrain) throw std::runtime_error{"Reset the crater scene first"};
        apache::math::vector3 position;
        for(auto &v : position) std::cin >> v;
        int32_t model{0};
        bool temporary{false};
        if(operation == "scale") {
          int32_t radius{0};
          bool heave{false};
          std::cin >> radius >> heave;
          craters.add_scalable(position, radius, heave, *terrain, seed);
        } else if(operation == "random") {
          std::cin >> model >> temporary;
          craters.add_random(position, model, temporary, *terrain, seed);
        } else if(operation == "add") {
          uint32_t heading{0};
          std::cin >> model >> heading >> temporary;
          craters.add(position, model, heading, temporary, *terrain);
        } else {
          throw std::runtime_error{"Unknown crater-state operation"};
        }
      }
    }
  } else if(command == "craters") {
    apache::math::trigonometry const trig{data};
    auto const models{apache::resources::read_models(data.read("3DDATA/3DOBJS.BIN"))};
    auto const textures{apache::resources::read_object_textures(data.read("3DDATA/OBJTEXT.BIN"))};
    std::string zone;
    std::string loaded;
    std::optional<apache::resources::terrain> terrain;
    std::optional<apache::render::scene_palette> palette;
    apache::simulation::crater crater;
    apache::math::pose camera;
    apache::render::sky_lighting light;
    uint32_t h{0};
    uint32_t p{0};
    uint32_t r{0};
    int32_t distance{0};
    while(std::cin >> zone >> crater.model >> crater.position[0] >> crater.position[1] >> crater.position[2] >> crater.heading >> crater.age >> crater.scale[0] >> crater.scale[1] >> crater.scale[2] >> camera.position[0] >> camera.position[1] >> camera.position[2] >> h >> p >> r >> light.heading >> light.pitch >> distance) {
      if(loaded != zone) {
        terrain.emplace(data.read(zone + ".WZ/GRIDPOST.BIN"));
        palette.emplace(data, zone);
        loaded = zone;
      }
      camera.position[2] = std::max(camera.position[2], terrain->elevation(camera.position[0], camera.position[1]) + 256);
      camera.rotation = trig.transformation(h, p, r);
      apache::render::framebuffer frame{320, 240};
      for(size_t i{0}; i != frame.image.pixels.size(); ++i) frame.image.pixels[i] = static_cast<uint8_t>((i * 7 + 19) & 255);
      auto instance{apache::render::crater_model(crater, trig, distance)};
      apache::render::model_renderer renderer{frame, models, textures, *palette};
      renderer.style.light = apache::render::light_vector(trig, light);
      renderer.draw(instance, camera);
      std::cout << std::hex << apache::resources::fingerprint(frame.image.pixels) << std::dec << ' ' << renderer.missing_normals << ' ' << renderer.invalid_texture_samples << '\n';
      if(argc > 3) {
        std::ofstream raw{argv[3], std::ios::binary};
        raw.write(reinterpret_cast<char const *>(frame.image.pixels.data()), static_cast<std::streamsize>(frame.image.pixels.size()));
      }
    }
  } else if(command == "mobile-state") {
    apache::math::trigonometry const trig{data};
    auto const models{apache::resources::read_models(data.read("3DDATA/3DOBJS.BIN"))};
    apache::render::mobile_scene mobiles{models};
    std::optional<apache::resources::terrain> terrain;
    std::string operation;
    auto const pose_input{[&]{
      apache::math::pose pose;
      uint32_t h{0};
      uint32_t p{0};
      uint32_t r{0};
      std::cin >> pose.position[0] >> pose.position[1] >> pose.position[2] >> h >> p >> r;
      pose.rotation = trig.transformation(h, p, r);
      return pose;
    }};
    while(std::cin >> operation) {
      if(operation == "reset") {
        std::string zone;
        std::cin >> zone;
        terrain.emplace(data.read(zone + ".WZ/GRIDPOST.BIN"));
        mobiles.clear();
      } else if(operation == "add") {
        uint32_t model{0};
        std::cin >> model;
        mobiles.add(model, pose_input());
      } else if(operation == "move") {
        uint32_t id{0};
        std::cin >> id;
        mobiles.move(id, pose_input());
      } else if(operation == "remove") {
        uint32_t id{0};
        std::cin >> id;
        mobiles.remove(id);
      } else if(operation == "state") {
        apache::resources::bytes trace;
        for(size_t sector{0}; sector != 4096; ++sector) {
          auto const ids{mobiles.sector(sector)};
          if(ids.empty()) continue;
          word(trace, static_cast<uint32_t>(sector));
          word(trace, static_cast<uint32_t>(ids.size()));
          for(auto const id : ids) word(trace, id);
        }
        std::cout << std::hex << apache::resources::fingerprint(trace) << std::dec << '\n';
      } else if(operation == "receiver") {
        apache::math::vector3 position;
        for(auto &v : position) std::cin >> v;
        auto const height{mobiles.shadow_receiver(position)};
        std::cout << (height ? *height : -1) << '\n';
      } else if(operation == "queue") {
        auto camera{pose_input()};
        apache::render::mobile_visibility visibility;
        apache::render::sky_lighting light;
        size_t prefill{0};
        std::cin >> visibility.cloud_mode >> visibility.cloud_flags >> visibility.weather >> visibility.cloud_bottom >> visibility.cloud_top >> visibility.near_depth >> visibility.far_depth >> light.heading >> light.pitch >> visibility.shadows >> prefill;
        if(!terrain) throw std::runtime_error{"Reset the mobile scene before drawing it"};
        camera.position[2] = std::max(camera.position[2], terrain->elevation(camera.position[0], camera.position[1]) + 256);
        apache::render::shadow_projection casting{*terrain, trig, light};
        casting.cloud_mode = visibility.cloud_mode;
        casting.cloud_top = visibility.cloud_top;
        apache::render::draw_queue queue;
        for(size_t i{0}; i != prefill; ++i) {
          if(!queue.consume_unused_slot()) break;
        }
        mobiles.submit(queue, camera, visibility, casting);
        apache::resources::bytes trace;
        for(auto const &draw : queue.ordered()) {
          word(trace, static_cast<uint32_t>(draw.kind));
          word(trace, draw.payload);
          word(trace, static_cast<uint32_t>(draw.depth));
        }
        std::cout << 7200 - queue.available() << ' ' << trace.size() / 12 << ' ' << std::hex << apache::resources::fingerprint(trace) << std::dec << '\n';
      } else {
        throw std::runtime_error{"Unknown mobile-state operation"};
      }
    }
  } else if(command == "shadows") {
    apache::math::trigonometry const trig{data};
    auto const models{apache::resources::read_models(data.read("3DDATA/3DOBJS.BIN"))};
    std::string zone;
    std::string loaded;
    std::optional<apache::resources::terrain> terrain;
    std::optional<apache::render::scene_palette> palette;
    uint32_t id{0};
    uint32_t flags{0};
    uint32_t heading{0};
    uint32_t pitch{0};
    uint32_t roll{0};
    uint32_t ch{0};
    uint32_t cp{0};
    uint32_t cr{0};
    int32_t distance{0};
    unsigned int mode{0};
    apache::math::pose object;
    apache::math::pose camera;
    apache::render::sky_lighting light;
    while(std::cin >> zone >> id >> flags >> object.position[0] >> object.position[1] >> object.position[2] >> heading >> pitch >> roll >> camera.position[0] >> camera.position[1] >> camera.position[2] >> ch >> cp >> cr >> light.heading >> light.pitch >> mode >> distance) {
      if(loaded != zone) {
        terrain.emplace(data.read(zone + ".WZ/GRIDPOST.BIN"));
        palette.emplace(data, zone);
        loaded = zone;
      }
      object.rotation = trig.transformation(heading, pitch, roll);
      camera.rotation = trig.transformation(ch, cp, cr);
      camera.position[2] = std::max(camera.position[2], terrain->elevation(camera.position[0], camera.position[1]) + 256);
      apache::render::shadow_projection casting{*terrain, trig, light};
      casting.cloud_mode = mode;
      apache::render::framebuffer frame{320, 240};
      for(size_t i{0}; i != frame.image.pixels.size(); ++i) frame.image.pixels[i] = static_cast<uint8_t>((i * 7 + 19) & 255);
      auto instance{apache::render::construct_model(models, id, object)};
      instance.flags = flags;
      instance.distance = distance;
      apache::render::shadow_renderer renderer{frame, models, casting, *palette};
      renderer.draw(instance, camera);
      std::cout << std::hex << apache::resources::fingerprint(frame.image.pixels) << std::dec << ' ' << casting.cast[0] << ' ' << casting.cast[1] << '\n';
      if(argc > 3) {
        std::ofstream raw{argv[3], std::ios::binary};
        raw.write(reinterpret_cast<char const *>(frame.image.pixels.data()), static_cast<std::streamsize>(frame.image.pixels.size()));
      }
    }
  } else if(command == "effect-state") {
    apache::simulation::effects effects;
    std::optional<apache::resources::terrain> terrain;
    uint32_t seed{1};
    std::string operation;
    while(std::cin >> operation) {
      if(operation == "reset") {
        std::string zone;
        std::cin >> zone >> seed;
        terrain.emplace(data.read(zone + ".WZ/GRIDPOST.BIN"));
        effects.reset();
      } else if(operation == "advance") {
        if(!terrain) throw std::runtime_error{"Reset the effect scene before advancing it"};
        int32_t delta{0};
        std::cin >> delta;
        effects.advance(delta, *terrain);
      } else if(operation == "state") {
        apache::resources::bytes bytes;
        size_t puffs{0};
        size_t blasts{0};
        for(size_t i{0}; i != effects.smoke_puffs.size(); ++i) {
          auto const &puff{effects.smoke_puffs[i]};
          if(!puff.active) continue;
          ++puffs;
          word(bytes, static_cast<uint32_t>(i));
          word(bytes, 1);
          for(auto const value : puff.position) word(bytes, static_cast<uint32_t>(value));
          word(bytes, static_cast<uint32_t>(puff.radius));
          word(bytes, static_cast<uint32_t>(puff.lifetime));
          word(bytes, static_cast<uint32_t>(puff.age));
          word(bytes, puff.angle);
        }
        for(size_t i{0}; i != effects.explosions.size(); ++i) {
          auto const &blast{effects.explosions[i]};
          if(!blast.kind) continue;
          ++blasts;
          word(bytes, static_cast<uint32_t>(i));
          word(bytes, blast.kind);
          for(auto const value : blast.position) word(bytes, static_cast<uint32_t>(value));
          for(auto const value : blast.velocity) word(bytes, static_cast<uint32_t>(value));
          word(bytes, static_cast<uint32_t>(blast.radius));
          word(bytes, static_cast<uint32_t>(blast.frame));
        }
        std::cout << seed << ' ' << puffs << ' ' << blasts << ' ' << std::hex << apache::resources::fingerprint(bytes) << std::dec << '\n';
      } else {
        apache::math::vector3 position;
        int32_t radius{0};
        std::cin >> position[0] >> position[1] >> position[2] >> radius;
        if(operation == "smoke") {
          int32_t lifetime{0};
          std::cin >> lifetime;
          effects.add_smoke(position, radius, lifetime, seed);
        } else if(operation == "ground") {
          if(!terrain) throw std::runtime_error{"Reset the effect scene before ground placement"};
          effects.add_ground_explosion(position, radius, *terrain);
        } else if(operation == "air") {
          apache::math::vector3 velocity;
          for(auto &value : velocity) std::cin >> value;
          effects.add_air_explosion(position, radius, velocity);
        } else {
          throw std::runtime_error{"Unknown effect-state operation"};
        }
      }
    }
  } else if(command == "billboards") {
    apache::math::trigonometry const trig{data};
    apache::render::effect_renderer const effects{data};
    int kind{0};
    apache::math::vector3 position;
    int32_t radius{0};
    int32_t age{0};
    int32_t lifetime{0};
    uint32_t angle{0};
    uint32_t heading{0};
    uint32_t pitch{0};
    uint32_t roll{0};
    apache::math::pose camera;
    while(std::cin >> kind >> position[0] >> position[1] >> position[2] >> radius >> age >> lifetime >> angle >> camera.position[0] >> camera.position[1] >> camera.position[2] >> heading >> pitch >> roll) {
      camera.rotation = trig.transformation(heading, pitch, roll);
      auto const angles{trig.angles(camera.rotation)};
      apache::render::framebuffer frame{320, 240};
      for(size_t i{0}; i != frame.image.pixels.size(); ++i) frame.image.pixels[i] = static_cast<uint8_t>((i * 7 + 19) & 255);
      apache::render::projection view{0, 0, 320, 240};
      apache::render::rasterizer raster{frame};
      if(kind == 3) {
        effects.draw_smoke({true, position, radius, lifetime, age, angle}, camera, angles[2], trig, view, raster);
      } else {
        effects.draw_explosion({static_cast<unsigned int>(kind), position, {0, 0, 0}, radius, age}, camera, angles[2], trig, view, raster);
      }
      std::cout << std::hex << apache::resources::fingerprint(frame.image.pixels) << std::dec << ' ' << raster.invalid_texture_samples << '\n';
      if(argc > 3) {
        std::ofstream raw{argv[3], std::ios::binary};
        raw.write(reinterpret_cast<char const *>(frame.image.pixels.data()), static_cast<std::streamsize>(frame.image.pixels.size()));
      }
    }
  } else if(command == "cloud-vertices" || command == "clouds") {
    apache::math::trigonometry const trig{data};
    std::string zone;
    apache::math::pose camera;
    uint32_t heading{0};
    uint32_t pitch{0};
    uint32_t roll{0};
    unsigned int mode{0};
    unsigned int weather{0};
    int height{0};
    while(std::cin >> zone >> camera.position[0] >> camera.position[1] >> camera.position[2] >> heading >> pitch >> roll >> mode >> weather >> height) {
      apache::resources::terrain const terrain{data.read(zone + ".WZ/GRIDPOST.BIN")};
      auto const elevation{terrain.elevation(camera.position[0], camera.position[1])};
      if(camera.position[2] <= elevation) camera.position[2] = elevation + 256;
      camera.rotation = trig.transformation(heading, pitch, roll);
      auto const angles{trig.angles(camera.rotation)};
      auto const bounds{apache::render::scan_cloud_sectors(camera, trig, angles[0], angles[1], mode)};
      apache::render::cloud_mesh mesh{terrain, camera, bounds, height, mode, weather};
      if(command == "clouds") {
        apache::render::scene_palette const palette{data, zone};
        apache::render::framebuffer frame{320, 240};
        frame.clear(155);
        apache::render::projection view{0, 0, 320, 240};
        apache::render::rasterizer raster{frame};
        apache::render::draw_queue queue;
        mesh.submit(queue, camera);
        auto const entries{queue.ordered()};
        for(auto const &entry : entries) mesh.draw(raster, view, palette, entry.payload);
        std::cout << entries.size() << ' ' << std::hex << apache::resources::fingerprint(frame.image.pixels) << std::dec << '\n';
        if(argc > 3) {
          std::ofstream raw{argv[3], std::ios::binary};
          raw.write(reinterpret_cast<char const *>(frame.image.pixels.data()), static_cast<std::streamsize>(frame.image.pixels.size()));
        }
        continue;
      }
      apache::resources::bytes bytes;
      for(auto const &point : mesh.vertices) {
        for(auto const value : point.eye) word(bytes, static_cast<uint32_t>(value));
        word(bytes, static_cast<uint32_t>(point.intensity));
        if(argc > 3) std::cerr << point.eye[0] << ' ' << point.eye[1] << ' ' << point.eye[2] << ' ' << point.intensity << '\n';
      }
      std::cout << bounds.left << ' ' << bounds.bottom << ' ' << bounds.right << ' ' << bounds.top << ' ' << mesh.vertices.size() << ' ' << std::hex << apache::resources::fingerprint(bytes) << std::dec << '\n';
    }
  } else if(command == "cloud-pattern") {
    for(int x{0}; x != 36; ++x) {
      for(int y{0}; y != 36; ++y) std::cout << apache::render::cloud_patch_type(x, y) << '\n';
    }
  } else if(command == "celestial") {
    apache::math::trigonometry const trig{data};
    apache::render::scene_palette const palette{data, "FORT"};
    apache::render::celestial const sky{data, "FORT"};
    int w{0};
    int h{0};
    uint32_t heading{0};
    uint32_t pitch{0};
    uint32_t roll{0};
    apache::render::sky_conditions conditions;
    apache::render::sky_lighting lighting;
    while(std::cin >> w >> h >> heading >> pitch >> roll >> conditions.cloud_mode >> conditions.cloud_flags >> lighting.heading >> lighting.pitch >> lighting.night) {
      apache::render::framebuffer frame{static_cast<unsigned int>(w), static_cast<unsigned int>(h)};
      frame.clear(155);
      auto const glare{sky.draw(frame, trig.transformation(heading, pitch, roll), trig, palette, conditions, lighting)};
      std::cout << std::hex << apache::resources::fingerprint(frame.image.pixels) << std::dec << ' ' << glare << '\n';
      if(argc > 3) {
        std::ofstream raw{argv[3], std::ios::binary};
        raw.write(reinterpret_cast<char const *>(frame.image.pixels.data()), static_cast<std::streamsize>(frame.image.pixels.size()));
      }
    }
  } else if(command == "horizon") {
    apache::math::trigonometry const trig{data};
    apache::render::scene_palette const palette{data, "FORT"};
    int w{0};
    int h{0};
    uint32_t pitch{0};
    uint32_t roll{0};
    unsigned int ground{0};
    apache::render::sky_conditions conditions;
    while(std::cin >> w >> h >> pitch >> roll >> conditions.cloud_mode >> conditions.cloud_flags >> conditions.weather >> ground) {
      apache::render::framebuffer frame{static_cast<unsigned int>(w), static_cast<unsigned int>(h)};
      frame.clear(255);
      conditions.ground_colour = static_cast<uint8_t>(ground);
      apache::render::horizon horizon{frame, frame.clip, palette, trig};
      horizon.ground(pitch, roll, conditions);
      horizon.sky(conditions);
      std::cout << apache::resources::fingerprint(frame.image.pixels) << '\n';
      if(argc > 3) {
        std::ofstream file{argv[3], std::ios::binary};
        file.write(reinterpret_cast<char const *>(frame.image.pixels.data()), static_cast<std::streamsize>(frame.image.pixels.size()));
      }
    }
  } else if(command == "projection2" || command == "projection2-line" || command == "projection2-inverse") {
    apache::math::trigonometry const trig{data};
    apache::render::window2 window;
    apache::render::rectangle viewport;
    int32_t origin_x{0};
    int32_t origin_y{0};
    uint32_t angle{0};
    size_t count{0};
    while(std::cin >> viewport.left >> viewport.top >> viewport.right >> viewport.bottom >> window.left >> window.bottom >> window.right >> window.top >> origin_x >> origin_y >> angle >> count) {
      apache::render::projection2 view{viewport, window};
      view.origin = {origin_x, origin_y};
      view.cosine = trig.cosine(angle);
      view.sine = trig.sine(angle);
      std::vector<apache::render::vertex2> polygon(count);
      for(auto &point : polygon) std::cin >> point.x >> point.y >> point.intensity;
      if(command == "projection2-inverse") {
        for(auto const &point : polygon) {
          auto const world{view.unproject(point.x, point.y)};
          std::cout << world.x << ' ' << world.y << ' ';
        }
        std::cout << '\n';
      } else if(command == "projection2-line") {
        if(count != 2) throw std::runtime_error{"A line needs two endpoints"};
        auto const line{view.clip_line(polygon[0], polygon[1])};
        std::cout << static_cast<bool>(line);
        if(line) {
          for(auto const &point : *line) std::cout << ' ' << point.x << ' ' << point.y;
        }
        for(auto const &point : polygon) {
          auto const text{view.project(point)};
          std::cout << ' ' << text.x << ' ' << text.y;
        }
        std::cout << '\n';
      } else {
        auto const result{view.clip(polygon)};
        std::cout << result.size();
        for(auto const &point : result) std::cout << ' ' << point.x << ' ' << point.y << ' ' << point.intensity;
        std::cout << '\n';
      }
    }
  } else if(command == "sectors") {
    apache::math::trigonometry const trig{data};
    apache::render::projection const view{0, 0, 320, 240};
    std::string zone;
    apache::math::pose camera;
    uint32_t heading{0};
    uint32_t pitch{0};
    uint32_t roll{0};
    uint32_t distance{0};
    int sector_x{0};
    int sector_y{0};
    bool super{false};
    while(std::cin >> zone >> camera.position[0] >> camera.position[1] >> camera.position[2] >> heading >> pitch >> roll >> distance >> sector_x >> sector_y >> super) {
      apache::resources::terrain const terrain{data.read(zone + ".WZ/GRIDPOST.BIN")};
      auto const ground{terrain.elevation(camera.position[0], camera.position[1])};
      if(camera.position[2] <= ground) camera.position[2] = ground + 256;
      camera.rotation = trig.transformation(heading, pitch, roll);
      for(bool const layer : {false, true}) {
        auto const bounds{apache::render::scan_sectors(camera, view, trig.angles(camera.rotation)[0], distance, layer)};
        std::cout << bounds.left << ' ' << bounds.bottom << ' ' << bounds.right << ' ' << bounds.top << ' ';
      }
      auto const origin{apache::render::feature_origin(camera, sector_x, sector_y, super)};
      std::cout << apache::render::sector_visibility(camera.rotation, origin, view, super);
      for(auto const value : origin) std::cout << ' ' << value;
      std::cout << '\n';
    }
  } else if(command == "inverse-trig") {
    apache::math::trigonometry const trig{data};
    uint32_t heading{0};
    uint32_t pitch{0};
    uint32_t roll{0};
    int32_t x{0};
    int32_t y{0};
    while(std::cin >> heading >> pitch >> roll >> x >> y) {
      auto const angles{trig.angles(trig.transformation(heading, pitch, roll))};
      for(auto const angle : angles) std::cout << angle << ' ';
      std::cout << trig.arctangent(x, y) << '\n';
    }
  } else if(command == "features") {
    for(std::string_view const zone : {"FORT", "CYPRUS", "KOREA", "YEMEN"}) {
      for(bool const super : {false, true}) {
        apache::resources::feature_layer const layer{data, zone, super};
        for(size_t id{1}; id != layer.records.size(); ++id) {
          if(layer.records[id]) {
            auto const &object{*layer.records[id]};
            apache::resources::bytes record;
            word(record, object.vertices.size());
            for(auto const &v : object.vertices) {
              word(record, v.flags);
              std::array<int32_t, 2> position{0, 0};
              std::array<int32_t, 4> constexpr units{1 << 26, 1 << 18, 1 << 12, 1 << 6};
              for(size_t level{0}; level != 4; ++level) {
                for(size_t axis{0}; axis != 2; ++axis) position[axis] += v.components[level][axis] * units[level];
              }
              for(auto const value : position) word(record, static_cast<uint32_t>(value));
            }
            for(auto const *list : {&object.faces, &object.distant}) {
              word(record, list->size());
              for(auto const &face : *list) {
                word(record, face.type);
                word(record, face.surface);
                word(record, face.colours[0]);
                word(record, face.colours[1]);
                for(auto const *values : {&face.indices, &face.intensities, &face.render_indices}) {
                  word(record, values->size());
                  for(auto const value : *values) word(record, value);
                }
              }
            }
            std::cout << zone << ' ' << super << ' ' << id << ' ' << std::hex << apache::resources::fingerprint(record) << std::dec << '\n';
          }
        }
      }
    }
  } else if(command == "feature-frames") {
    std::string zone;
    bool super{false};
    bool distant{false};
    uint32_t id{0};
    uint32_t heading{0};
    uint32_t pitch{0};
    uint32_t roll{0};
    uint32_t detail{0};
    apache::math::vector3 origin;
    apache::math::trigonometry const trig{data};
    apache::render::projection const view{0, 0, 320, 240};
    std::string loaded;
    std::optional<apache::resources::feature_layer> layer;
    std::optional<apache::render::scene_palette> palette;
    apache::render::framebuffer frame{320, 240};
    while(std::cin >> zone >> super >> id >> distant >> detail >> heading >> pitch >> roll >> origin[0] >> origin[1] >> origin[2]) {
      std::string const key{zone + (super ? ":super" : ":normal")};
      if(key != loaded) {
        layer.emplace(data, zone, super);
        palette.emplace(data, zone);
        loaded = key;
      }
      if(id >= layer->records.size() || !layer->records[id]) throw std::runtime_error{"Invalid feature record"};
      auto const &object{*layer->records[id]};
      auto const points{apache::render::project_feature(object, trig.transformation(heading, pitch, roll), origin, view)};
      apache::render::feature_renderer renderer{frame, view, *palette};
      frame.clear(155);
      renderer.draw(distant && object.has_distant ? object.distant : object.faces, points, apache::render::terrain_fog{detail});
      apache::resources::bytes coordinates;
      for(auto const &point : points) {
        for(int32_t const value : {point.eye[0], point.eye[1], point.eye[2], point.screen.x, point.screen.y, int32_t{point.outcode}}) word(coordinates, static_cast<uint32_t>(value));
      }
      std::cout << std::hex << apache::resources::fingerprint(coordinates) << ' ' << apache::resources::fingerprint(frame.image.pixels) << std::dec << '\n';
      if(argc >= 4) {
        std::ofstream raw{argv[3], std::ios::binary};
        raw.write(reinterpret_cast<char const *>(frame.image.pixels.data()), static_cast<std::streamsize>(frame.image.pixels.size()));
        for(auto const &point : points) std::cerr << point.eye[0] << ' ' << point.eye[1] << ' ' << point.eye[2] << ' ' << point.screen.x << ' ' << point.screen.y << ' ' << +point.outcode << '\n';
      }
    }
  } else if(command == "terrain-polygons") {
    apache::render::scene_palette const palette{data, "FORT"};
    apache::resources::terrain_textures const textures{data, "FORT"};
    apache::render::projection const view{0, 0, 320, 240};
    apache::render::framebuffer frame{320, 240};
    apache::render::terrain_renderer renderer{frame, view, palette, textures};
    unsigned int method{0};
    unsigned int detail{0};
    unsigned int flags{0};
    unsigned int colour{0};
    unsigned int selector{0};
    unsigned int half{0};
    unsigned int two_sided{0};
    while(std::cin >> method >> detail >> flags >> colour >> selector >> half >> two_sided) {
      std::array<apache::render::vertex3, 4> points;
      for(auto &point : points) {
        apache::math::vector3 eye;
        int32_t light{0};
        std::cin >> eye[0] >> eye[1] >> eye[2] >> light;
        point = view.prepare_scaled(eye, light);
      }
      apache::resources::terrain_post post;
      post.flags = static_cast<uint8_t>(flags);
      post.colours.fill(static_cast<uint8_t>(colour));
      post.textures.fill(static_cast<uint8_t>(selector));
      frame.clear(155);
      renderer.draw(post, points, half, static_cast<apache::render::terrain_method>(method), apache::render::terrain_fog{detail}, two_sided != 0);
      std::cout << std::hex << apache::resources::fingerprint(frame.image.pixels) << std::dec << ' ' << renderer.invalid_texture_samples() << '\n';
      if(argc >= 4) {
        std::ofstream raw{argv[3], std::ios::binary};
        raw.write(reinterpret_cast<char const *>(frame.image.pixels.data()), static_cast<std::streamsize>(frame.image.pixels.size()));
      }
    }
  } else if(command == "terrain-shades") {
    unsigned int detail{0};
    unsigned int colour{0};
    unsigned int kind{0};
    unsigned int salt{0};
    uint32_t range{0};
    while(std::cin >> detail >> range >> colour >> kind >> salt) {
      apache::render::terrain_fog const fog{detail};
      std::array<uint8_t, 256> remap;
      for(unsigned int i{0}; i != 256; ++i) remap[i] = static_cast<uint8_t>(salt ? i * 73 + salt : i);
      std::cout << fog.constant_colour(range, static_cast<uint8_t>(colour), remap) << ' ' << fog.shaded_colours(range, kind, remap) << ' ' << fog.texture_bank(range, 3) << '\n';
    }
  } else if(command == "terrain-textures") {
    for(std::string_view const zone : {"FORT", "CYPRUS", "KOREA", "YEMEN"}) {
      apache::resources::terrain_textures const textures{data, zone};
      for(bool const detailed : {false, true}) {
        for(bool const forward : {false, true}) {
          for(unsigned int selector{0}; selector != 128; ++selector) {
            auto const &texture{textures.get(forward, selector, detailed)};
            std::cout << zone << ' ' << detailed << ' ' << forward << ' ' << selector << ' ' << std::hex << apache::resources::fingerprint(texture.pixels) << std::dec << '\n';
          }
        }
      }
    }
  } else if(command == "fog-tables") {
    apache::render::scene_palette const palette{data, "FORT"};
    for(auto const &table : palette.fog_intensity) {
      std::cout << std::hex << apache::resources::fingerprint(table) << std::dec << '\n';
      if(argc >= 4) {
        std::ofstream out{std::string{argv[3]} + std::to_string(&table - palette.fog_intensity.data()), std::ios::binary};
        out.write(reinterpret_cast<char const *>(table.data()), static_cast<std::streamsize>(table.size()));
      }
    }
  } else if(command == "terrain-vertices") {
    apache::math::trigonometry const trig{data};
    apache::render::projection const view{0, 0, 320, 240};
    std::string zone;
    apache::math::pose camera;
    uint32_t heading{0};
    uint32_t pitch{0};
    uint32_t roll{0};
    int left{0};
    int bottom{0};
    unsigned int columns{0};
    unsigned int rows{0};
    while(std::cin >> zone >> camera.position[0] >> camera.position[1] >> camera.position[2] >> heading >> pitch >> roll >> left >> bottom >> columns >> rows) {
      apache::resources::terrain const terrain{data.read(zone + ".WZ/GRIDPOST.BIN")};
      camera.rotation = trig.transformation(heading, pitch, roll);
      auto const ground{terrain.elevation(camera.position[0], camera.position[1])};
      if(camera.position[2] <= ground) camera.position[2] = ground + 256;
      auto const vertices{apache::render::project_terrain(terrain, camera, view, left, bottom, columns, rows)};
      apache::resources::bytes output;
      for(auto const &v : vertices) {
        for(int32_t const field : {v.eye[0], v.eye[1], v.eye[2], v.screen.x, v.screen.y, v.screen.intensity, int32_t{v.outcode}}) {
          for(unsigned int shift{0}; shift != 32; shift += 8) output.push_back(static_cast<uint8_t>(static_cast<uint32_t>(field) >> shift));
          if(argc >= 4) std::cerr << field << ' ';
        }
      }
      std::cout << std::hex << apache::resources::fingerprint(output) << std::dec << '\n';
    }
  } else if(command == "draw-order") {
    unsigned int count{0};
    while(std::cin >> count) {
      apache::render::draw_queue queue;
      for(unsigned int i{0}; i != count; ++i) {
        apache::render::queued_draw draw;
        unsigned int kind{0};
        std::cin >> kind >> draw.depth >> draw.terrain_depth_sum >> draw.flat;
        draw.kind = static_cast<apache::render::draw_kind>(kind);
        draw.payload = i;
        queue.push(draw);
      }
      auto const ordered{queue.ordered()};
      std::cout << ordered.size();
      for(auto const &draw : ordered) std::cout << ' ' << draw.payload;
      std::cout << '\n';
    }
  } else if(command == "palette") {
    for(std::string_view const zone : {"FORT", "CYPRUS", "KOREA", "YEMEN"}) {
      apache::render::scene_palette const palette{data, zone};
      std::cout << zone << ' ' << std::hex << apache::resources::fingerprint(palette.colours) << ' ' << apache::resources::fingerprint(palette.intensity) << ' ' << apache::resources::fingerprint(palette.glass) << std::dec << '\n';
    }
  } else if(command == "geometry") {
    apache::math::matrix3 a;
    apache::math::matrix3 b;
    apache::math::vector3 v;
    while(std::cin >> a[0]) {
      for(size_t i{1}; i != 9; ++i) std::cin >> a[i];
      for(auto &value : b) std::cin >> value;
      for(auto &value : v) std::cin >> value;
      for(auto const value : apache::math::multiply(a, b)) std::cout << value << ' ';
      for(auto const value : apache::math::transform(a, v)) std::cout << value << ' ';
      for(auto const shift : {19u, 25u}) {
        for(auto const value : apache::math::subdivide_grid(a[0], shift)) std::cout << value << ' ';
      }
      std::cout << '\n';
    }
  } else if(command == "model-renders" || command == "model-renders-alternate" || command == "model-renders-scenery") {
    auto const models{apache::resources::read_models(data.read("3DDATA/3DOBJS.BIN"))};
    auto const textures{apache::resources::read_object_textures(data.read("3DDATA/OBJTEXT.BIN"))};
    apache::render::scene_palette const palette{data, "FORT"};
    apache::math::trigonometry const trig{data};
    apache::render::framebuffer frame{320, 240};
    for(size_t i{0}; i != 256; ++i) frame.image.colours[i] = {palette.colours[i * 3], palette.colours[i * 3 + 1], palette.colours[i * 3 + 2]};
    apache::render::model_renderer renderer{frame, models, textures, palette};
    if(argc >= 5 && std::string_view{argv[4]} == "trace") {
      renderer.face_observer = [&](uint32_t const model, uint32_t const pc){
        std::cerr << model << ' ' << pc << ' ' << std::hex << apache::resources::fingerprint(frame.image.pixels) << std::dec << '\n';
      };
    }
    uint32_t id{0};
    uint32_t flags{0};
    uint32_t heading{0};
    uint32_t pitch{0};
    uint32_t roll{0};
    uint32_t camera_heading{0};
    uint32_t camera_pitch{0};
    uint32_t camera_roll{0};
    uint32_t quality{0};
    apache::math::vector3 position;
    while(std::cin >> id >> flags >> position[0] >> position[1] >> position[2] >> heading >> pitch >> roll >> camera_heading >> camera_pitch >> camera_roll >> quality) {
      renderer.style.textures = (quality & 1) != 0;
      renderer.style.gouraud = (quality & 2) != 0;
      auto object{command == "model-renders-scenery" ? apache::render::construct_scenery_model(models, id, {position, trig.transformation(heading, pitch, roll)}) : apache::render::construct_model(models, id, {position, trig.transformation(heading, pitch, roll)})};
      object.flags = flags;
      object.alternate_colours = command == "model-renders-alternate";
      apache::math::pose const camera{{0, 0, 1000000}, trig.transformation(camera_heading, camera_pitch, camera_roll)};
      frame.clear(155);
      try {
        renderer.draw(object, camera);
        std::cout << std::hex << apache::resources::fingerprint(frame.image.pixels) << std::dec << ' ' << renderer.missing_normals << ' ' << renderer.invalid_texture_samples << '\n';
      } catch(std::runtime_error const &error) {
        std::cout << "error " << error.what() << '\n';
      }
      if(argc >= 4) {
        apache::resources::write_ppm(argv[3], frame.image);
        std::ofstream raw{std::string{argv[3]} + ".pixels", std::ios::binary};
        raw.write(reinterpret_cast<char const *>(frame.image.pixels.data()), static_cast<std::streamsize>(frame.image.pixels.size()));
      }
    }
  } else if(command == "lines") {
    int x{0};
    int y{0};
    int w{0};
    int h{0};
    while(std::cin >> x >> y >> w >> h) {
      apache::math::vector3 a;
      apache::math::vector3 b;
      for(auto &value : a) std::cin >> value;
      for(auto &value : b) std::cin >> value;
      apache::render::projection const view{x, y, w, h};
      auto const result{view.clip_line(view.prepare(a), view.prepare(b))};
      std::cout << bool(result);
      if(result) {
        for(auto const &v : *result) std::cout << ' ' << v.eye[0] << ' ' << v.eye[1] << ' ' << v.eye[2] << ' ' << v.screen.x << ' ' << v.screen.y;
      }
      std::cout << '\n';
    }
  } else if(command == "projection") {
    int x{0};
    int y{0};
    int w{0};
    int h{0};
    int count{0};
    while(std::cin >> x >> y >> w >> h >> count) {
      apache::render::projection const view{x, y, w, h};
      std::vector<apache::render::vertex3> input;
      for(int i{0}; i != count; ++i) {
        apache::math::vector3 eye;
        int32_t u{0};
        int32_t v{0};
        int32_t light{0};
        std::cin >> eye[0] >> eye[1] >> eye[2] >> u >> v >> light;
        input.push_back(view.prepare(eye, u, v, light));
      }
      auto const output{view.clip(input)};
      std::cout << output.size();
      for(auto const &v : output) std::cout << ' ' << v.eye[0] << ' ' << v.eye[1] << ' ' << v.eye[2] << ' ' << v.screen.x << ' ' << v.screen.y << ' ' << v.screen.u << ' ' << v.screen.v << ' ' << v.screen.intensity;
      std::cout << '\n';
    }
  } else if(command == "circles") {
    int w{0};
    int h{0};
    int x{0};
    int y{0};
    int radius{0};
    int mode{0};
    std::array<uint8_t, 256> remapping;
    for(size_t i{0}; i != remapping.size(); ++i) remapping[i] = static_cast<uint8_t>((i * 13 + 7) & 255);
    while(std::cin >> w >> h >> x >> y >> radius >> mode) {
      apache::render::framebuffer frame{static_cast<unsigned int>(w), static_cast<unsigned int>(h)};
      for(size_t i{0}; i != frame.image.pixels.size(); ++i) frame.image.pixels[i] = static_cast<uint8_t>((i * 7 + 19) & 255);
      apache::render::rasterizer raster{frame};
      apache::render::material paint;
      paint.colours = 0x8b8b8b8b;
      paint.pattern = static_cast<apache::render::fill_pattern>(mode);
      paint.remapping = remapping;
      if(mode == 5) {
        raster.circle_outline(x, y, radius, 139);
      } else {
        raster.circle(x, y, radius, paint);
      }
      std::cout << std::hex << apache::resources::fingerprint(frame.image.pixels) << std::dec << '\n';
      if(argc > 3) {
        std::ofstream raw{argv[3], std::ios::binary};
        raw.write(reinterpret_cast<char const *>(frame.image.pixels.data()), static_cast<std::streamsize>(frame.image.pixels.size()));
      }
    }
  } else if(command == "raster") {
    apache::render::framebuffer frame{64, 48};
    apache::render::rasterizer raster{frame};
    std::array<uint8_t, 256> remapping;
    for(size_t i{0}; i != remapping.size(); ++i) remapping[i] = static_cast<uint8_t>((i * 13 + 7) & 255);
    apache::resources::texture texture{64, apache::resources::bytes(64 * 64)};
    for(size_t i{0}; i != texture.pixels.size(); ++i) texture.pixels[i] = static_cast<uint8_t>((i * 17 + i / 64 * 3) & 255);
    apache::resources::bytes intensity_map(65536);
    for(size_t i{0}; i != intensity_map.size(); ++i) intensity_map[i] = static_cast<uint8_t>((i * 11 + (i >> 8) * 7) & 255);
    int mode{0};
    int count{0};
    while(std::cin >> mode >> count) {
      std::vector<apache::render::screen_vertex> vertices(static_cast<size_t>(count));
      for(auto &v : vertices) {
        std::cin >> v.x >> v.y;
        if(mode >= 6 && mode <= 12) std::cin >> v.u >> v.v >> v.intensity;
      }
      for(size_t i{0}; i != frame.image.pixels.size(); ++i) frame.image.pixels[i] = static_cast<uint8_t>((i * 7 + 19) & 255);
      if(mode == 5) {
        raster.line(vertices.at(0), vertices.at(1), 139);
      } else if(mode >= 13) {
        apache::render::material paint;
        paint.pattern = apache::render::fill_pattern::heat_haze;
        paint.heat_phase = static_cast<uint8_t>(mode - 13);
        raster.fill(vertices, paint);
      } else if(mode >= 6) {
        apache::render::material paint;
        paint.colours = 0x81468146;
        paint.image = mode == 6 ? nullptr : &texture;
        paint.light = mode == 6 || mode >= 11 ? apache::render::shading::gouraud : mode == 9 || mode == 10 ? apache::render::shading::constant : apache::render::shading::none;
        paint.intensity = 0x58312345;
        paint.intensity_map = intensity_map;
        paint.masked = mode == 8 || mode == 10 || mode == 12;
        raster.fill(vertices, paint);
      } else {
        raster.polygon(vertices, mode == 0 ? 0x81468146 : 0x8b8b8b8b, static_cast<apache::render::fill_pattern>(mode), remapping);
      }
      std::cout << std::hex << apache::resources::fingerprint(frame.image.pixels) << std::dec << '\n';
    }
  } else if(command == "models" || command == "model-traces") {
    inspect_models(data, command == "model-traces");
  } else if(command == "matrices") {
    apache::math::trigonometry const trig{data};
    uint32_t heading{0};
    uint32_t pitch{0};
    uint32_t roll{0};
    while(std::cin >> heading >> pitch >> roll) {
      auto const matrix{trig.transformation(heading, pitch, roll)};
      for(auto const v : matrix) std::cout << v << ' ';
      std::cout << '\n';
    }
  } else if(command == "images") {
    for(auto const &name : data.names()) {
      if(argc >= 4 && name != apache::resources::data_store::normalise(argv[3])) continue;
      auto const file{data.read(name)};
      if(file.size() < 4 || std::string_view{reinterpret_cast<char const *>(file.data()), 4} != "FORM") continue;
      auto const image{apache::resources::read_iff(file)};
      std::cout << name << '\t' << image.width << '\t' << image.height << '\t' << std::hex << apache::resources::fingerprint(image.pixels) << std::dec << '\n';
      if(argc >= 5) apache::resources::write_ppm(argv[4], image);
    }
  } else if(command == "terrain") {
    for(std::string_view const zone : {"FORT", "CYPRUS", "KOREA", "YEMEN"}) {
      std::string const prefix{std::string{zone} + ".WZ/"};
      apache::resources::terrain const terrain{data.read(prefix + "GRIDPOST.BIN")};
      auto const objects{apache::resources::read_scenery(data.read(prefix + "GNDOBJS.BIN"))};
      uint64_t checksum{0};
      for(int32_t x{0}; x != 256; ++x) {
        for(int32_t y{0}; y != 256; ++y) checksum = checksum * 0x100000001b3ull + static_cast<uint32_t>(terrain.elevation(x * apache::resources::terrain_step + 500001, y * apache::resources::terrain_step + 700003));
      }
      std::cout << zone << '\t' << objects.size() << '\t' << std::hex << checksum << std::dec << '\n';
    }
  } else {
    throw std::runtime_error{"Unknown inspection command"};
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}
