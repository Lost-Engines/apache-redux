#include "mission_instruments.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include "instrument_state.h"
#include "sight_geometry.h"

namespace apache::simulation {
namespace {
int32_t distance(math::vector3 const &a, math::vector3 const &b) {
  double sum{0}; for(size_t i{0}; i != 3; ++i) { double const v{static_cast<double>(math::subtract(a[i], b[i]))}; sum += v * v; }
  return std::bit_cast<int32_t>(static_cast<uint32_t>(static_cast<uint64_t>(std::sqrt(sum))));
}
std::vector<uint8_t> letters(std::span<resources::route_waypoint const> path) {
  std::vector<uint8_t> result(path.size()); uint8_t normal{'A'}, target{'X'};
  for(size_t i{0}; i != path.size(); ++i) switch(path[i].type) {
    case 0: case 1: case 2: case 3: case 7: case 8: result[i] = normal++; if(normal == 'T') normal = 'a'; break;
    case 4: case 6: result[i] = target++; if(target == '[') target = 'x'; break;
    default: break;
  }
  return result;
}
} // anonymous namespace
mission_instruments::mission_instruments(mission_runtime &runtime, unsigned int const video_mode, unsigned int const game_mode)
  : mission{runtime}, helmet{runtime.executable, runtime.trig, video_mode} {
  state.displays.initialise(game_mode);
}
void mission_instruments::radar_contacts(std::span<radar_return const> const contacts, bool const tactical, std::vector<render::radar_contact> &output) const {
  output.clear(); auto const &selected{mission.sensors->state.current};
  for(auto const &contact : contacts) {
    auto const &id{contact.target.identity}; uint32_t model{0}, status{0};
    if(id.kind == target_kind::scenery) { auto const &object{mission.structures.at(id.layer, id.index)}; model = object.model; status = object.status; } else if(id.kind == target_kind::mobile && mission.mobiles.contains(id.index)) { auto const &object{mission.mobiles.at(id.index)}; model = object.id; status = object.status; }
    if(model >= mission.models.size() || !mission.models[model]) continue;
    auto symbol{static_cast<unsigned int>(mission.models[model]->metadata[6])};
    if(tactical && id.kind == target_kind::mobile && symbol == 2 && (status & 6) == 4) symbol = 1;
    output.push_back({contact.display[0], contact.display[1], symbol, selected.identity.kind != target_kind::none && equivalent_targets(selected, contact.target)});
  }
}
instrument_state mission_instruments::flight_display() const {
  if(!mission.pilot || !mission.timing.player) throw std::runtime_error{"Instruments require a player aircraft"};
  auto const &flight{mission.pilot->readings()}; auto const &actor{mission.actors.units[*mission.timing.player]}; auto const &body{actor.object};
  auto const &path{mission.mission.tasks[actor.location.task].actors[actor.location.group].waypoints}; auto const labels{letters(path)};
  instrument_inputs input{.position{body.world.position}, .heading{flight.angles[0]}, .pitch{flight.angles[1]}, .roll{flight.angles[2]}, .mission_time{mission.mission.time_of_day}, .game_time{mission.timing.now}, .terrain_height{mission.terrain.elevation(body.world.position[0], body.world.position[1])}, .climb_rate{flight.climb}, .airspeed{flight.airspeed}, .sidespeed{flight.sidespeed}, .torque{flight.torque}, .waypoint{}};
  if(actor.waypoint && *actor.waypoint < path.size()) { auto const i{*actor.waypoint}; input.waypoint = {path[i].position[0], path[i].position[1], labels[i]}; }
  return instrument_values(input, mission.trig);
}
void mission_instruments::update(unsigned int const view, math::pose const &camera, render::projection const &projection) {
  if(!mission.pilot || !mission.player_armament || !mission.sensors || !mission.timing.player) throw std::runtime_error{"Cockpit requires a player mission"};
  auto &pilot{*mission.pilot}; auto &arm{*mission.player_armament}; auto &sensors{*mission.sensors}; auto &target{sensors.state.current};
  auto const &flight{pilot.readings()}; auto &actor{mission.actors.units[*mission.timing.player]}; auto &body{actor.object};
  auto const &path{mission.mission.tasks[actor.location.task].actors[actor.location.group].waypoints}; auto const labels{letters(path)};
  auto const display{sampled ? *sampled : flight_display()}; auto &h{state.helmet};
  h.view = view; h.mode = arm.inventory.helmet_mode; h.faults = pilot.faults(); h.aircraft = body.world; h.camera = camera;
  h.heading = display.heading; h.pitch = display.pitch; h.roll = display.roll; h.command_heading = display.command_heading;
  h.barometric_altitude = display.barometric_altitude; h.radar_altitude = display.radar_altitude; h.climb_rate = display.climb_rate; h.airspeed = display.airspeed; h.torque = display.torque;
  h.waypoint_letter = display.waypoint_letter; h.waypoint_distance = display.waypoint_distance; h.waypoint_time = display.waypoint_time; state.clock_time = display.clock_time;
  state.navigation = render::navigation_instruments{flight.angles[0], flight.airspeed};
  auto const &engine{flight.engines}; state.engines = {engine[0], engine[1], engine[2], engine[3], engine[4], engine[5], engine[6], engine[7]};
  auto const &inventory{arm.inventory}; auto const &slots{inventory.pylons};
  for(size_t i{0}; i != 2; ++i) state.stores.pairs[i] = {slots[i * 2].weapon, {static_cast<unsigned int>(slots[i * 2].ammunition), static_cast<unsigned int>(slots[i * 2 + 1].ammunition)}};
  state.stores.wingtip = {static_cast<unsigned int>(slots[4].ammunition), static_cast<unsigned int>(slots[5].ammunition)};
  state.stores.gun = static_cast<unsigned int>(inventory.ammunition[1]); state.stores.chaff = static_cast<unsigned int>(inventory.ammunition[7]); state.stores.flares = static_cast<unsigned int>(inventory.ammunition[8]);
  state.stores.armed = inventory.armed; state.stores.auto_chaff = inventory.automatic_chaff; state.stores.auto_ecm = sensors.state.automatic_ecm;
  state.route.clear(); state.waypoint = 0;
  if(actor.waypoint) { state.waypoint = *actor.waypoint; for(size_t i{0}; i != path.size(); ++i) state.route.push_back({path[i].position[0], path[i].position[1], labels[i]}); }
  auto &w{h.weapons}; w.selected = inventory.current; w.name = weapon_definitions().at(w.selected).name; w.rounds = inventory.ammunition.at(w.selected); w.armed = inventory.armed; w.landed = pilot.landed();
  w.impact_time = arm.impacts.current(mission.timing.now); w.lock_status = arm.sight.status; w.rocket_elevation = arm.mounts.hydra_elevation; w.box_half_size = weapon_definitions().at(w.selected).target_box_half_size;
  w.target_exists = target.identity.kind != target_kind::none; w.target_locked = sensors.state.locked; w.tads_active = sensors.state.tads;
  w.tads_valid = sensors.optical_symbol.has_value(); if(sensors.optical_symbol) w.tads = helmet.unproject_point(view, (*sensors.optical_symbol)[0], (*sensors.optical_symbol)[1]);
  target_description description; if(w.target_exists) description = mission.targets.describe(target);
  w.target_aircraft = description.movement.classification == 2; w.target_designated = description.designated; w.target_name = w.target_exists ? description.name : "";
  w.target_position = description.movement.position; w.target_range = w.target_exists ? distance(body.world.position, w.target_position) : 0; w.target_speed = description.movement.speed;
  w.target_heading = description.movement.direction;
  w.target_valid = false;
  if(w.target_exists) if(auto const point{helmet.project_point(view, camera, projection, w.target_position)}) { w.target = *point; w.target_valid = true; }
  w.impact = {};
  if(w.selected == 1) {
    arm.mounts.position_gun(mission.timing.now, w.selected, body, w.target_exists ? std::optional{description.movement} : std::nullopt, arm.helmet);
    auto const barrel{arm.mounts.launch_pose(1, 0, 0, body)};
    w.impact_position = chain_gun_impact(barrel, 768000, w.target_exists ? std::optional{arm.mounts.intercept} : std::nullopt, mission.environment.height);
    if(auto const point{helmet.project_point(view, camera, projection, w.impact_position)}) w.impact = *point;
  }
  w.intercept = {};
  if(w.target_exists && w.selected >= 4 && w.selected <= 6) {
    auto movement{description.movement}; movement.speed = math::negate(movement.speed);
    auto const aim{intercept_point(body.world.position, weapon_definitions().at(w.selected).maximum_speed, movement, 3)};
    if(auto const point{helmet.project_point(view, camera, projection, aim)}) w.intercept = *point;
  }
  state.target_model = nullptr; state.target_camera_aim = {};
  if(target.identity.kind == target_kind::mobile && mission.mobiles.contains(target.identity.index)) { state.target_model = &mission.mobiles.at(target.identity.index); state.target_camera_aim = state.target_model->relative.position; } else if(target.identity.kind == target_kind::scenery) {
    auto const &source{mission.structures.at(target.identity.layer, target.identity.index)};
    if(source.model && source.model < mission.models.size() && mission.models[source.model]) {
      auto const angle{static_cast<uint32_t>(source.heading) << 16}; auto const sine{mission.trig.sine(angle)}, cosine{mission.trig.cosine(angle)};
      ground_target = render::construct_model(mission.models, source.model, {{source.x, source.y, 0}, {cosine, math::negate(sine), 0, sine, cosine, 0, 0, 0, INT32_MAX}});
      ground_target->flags = source.animation; state.target_model = &*ground_target; state.target_camera_aim = target.position;
    }
  }
  for(bool const tactical : {false, true}) {
    auto &radar{tactical ? state.tactical : state.radar}; auto const &s{sensors.state};
    radar.mode = tactical ? render::radar_mode::tactical : s.ground ? render::radar_mode::ground : s.air ? render::radar_mode::air : render::radar_mode::inactive;
    radar.range = tactical ? s.tactical_range : s.ground ? s.ground_range : s.air_range; radar.corner_range = tactical ? s.tactical_corner : radar.range;
    radar.mark_length = tactical ? s.tactical_mark : s.ground ? s.ground_mark : 0x3f4aa; radar.heading = flight.angles[0]; radar.failed = (h.faults & 0x200) != 0;
    radar.frozen = s.frozen; radar.tads = s.tads; radar.locked = s.locked; radar.scan_all = s.scan_all; radar.threat = s.threat; radar.range_index = s.ground_range_index;
    radar.has_target = target.identity.kind == target_kind::scenery || target.identity.kind == target_kind::mobile;
    radar.target_name = radar.has_target && description.model < mission.models.size() && mission.models[description.model] ? mission.models[description.model]->name : "";
    radar.target_distance = radar.has_target ? distance(body.world.position, state.target_camera_aim) : 0;
    auto &contacts{tactical ? state.tactical_contacts : state.radar_contacts};
    if(tactical) radar_contacts(sensors.tactical, true, contacts);
    else if(s.ground) radar_contacts(sensors.ground_radar, false, contacts);
    else if(s.air) radar_contacts(sensors.air_radar, true, contacts);
    else contacts.clear();
  }
}
} // namespace apache::simulation
