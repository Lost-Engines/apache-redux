#include "inspect_sensors.h"
#include <istream>
#include <ostream>
#include <stdexcept>
#include "simulation/mission_runtime.h"
#include "simulation/player_sensors.h"

void inspect_sensors(apache::resources::data_store const &data, std::string const &zone, std::string const &path, std::istream &input, std::ostream &output) {
  using apache::simulation::mission_runtime;
  using apache::simulation::player_sensors;
  using apache::simulation::target_kind;
  using apache::simulation::target_reference;
  mission_runtime runtime{data, zone, path, false, false};
  player_sensors sensors{runtime.terrain, runtime.models, runtime.scene, runtime.mobiles, runtime.targets, runtime.trig};
  auto const id{[&](target_reference const &target)->int64_t {
    auto const &t{target.identity};
    if(t.kind == target_kind::none) return 0;
    return t.kind == target_kind::scenery ? 1 + t.index + t.layer * 100000 : 200001 + static_cast<int64_t>(runtime.spatial.actor_index(t.index).value());
  }};
  auto const contacts{[&](std::string_view label, auto const &list){
    output << label << ' ' << list.size() << '\n';
    for(auto const &entry : list) {
      output << id(entry.target);
      for(auto v : entry.sector) output << ' ' << v;
      for(auto v : entry.position) output << ' ' << v;
      output << ' ' << entry.target.classification << '\n';
    }
  }};
  auto const radar{[&](std::string_view label, auto const &list, bool priority){
    output << label << ' ' << list.size() << '\n';
    for(auto const &entry : list) {
      output << id(entry.target);
      for(auto v : entry.display) output << ' ' << v;
      output << ' ' << entry.target.classification;
      if(priority) output << ' ' << entry.priority;
      output << '\n';
    }
  }};
  std::string command;
  while(input >> command) {
    if(command == "move") {
      size_t actor{0};
      apache::math::vector3 position;
      input >> actor;
      for(auto &v : position) input >> v;
      runtime.spatial.move(runtime.actors.units.at(actor), position);
    } else if(command == "scan") {
      apache::math::pose body;
      std::array<uint32_t, 3> angles;
      int64_t player{-1};
      uint32_t faults{0};
      for(auto &v : body.position) input >> v;
      for(auto &v : angles) input >> v;
      input >> player >> sensors.state.ground >> sensors.state.air >> sensors.state.tactical >> sensors.state.frozen >> sensors.state.scan_all >> sensors.state.threat >> sensors.state.ground_range >> sensors.state.air_range >> sensors.state.tactical_range >> faults;
      body.rotation = runtime.trig.transformation(angles[0], angles[1], angles[2]);
      sensors.scan_contacts(body, player < 0 ? std::nullopt : std::optional{runtime.spatial.mobile_id(static_cast<size_t>(player))});
      sensors.scan_radars(body, angles[0], faults);
      contacts("scenery", sensors.ground);
      contacts("mobiles", sensors.mobile);
      contacts("missiles", sensors.missiles);
      radar("ground", sensors.ground_radar, false);
      radar("air", sensors.air_radar, false);
      radar("tactical", sensors.tactical, false);
      radar("ranked", sensors.ranked, true);
    } else if(command == "sight") {
      apache::math::pose view;
      std::array<uint32_t, 3> angles;
      uint32_t heading{0};
      uint32_t faults{0};
      unsigned int mode{0};
      int width{0};
      int height{0};
      int64_t selected{0};
      for(auto &v : view.position) input >> v;
      for(auto &v : angles) input >> v;
      input >> heading >> width >> height >> mode >> faults >> sensors.state.locked >> sensors.state.tads >> selected;
      sensors.state.current = {};
      sensors.optical = {};
      if(selected >= 200001) {
        sensors.state.current.identity = {target_kind::mobile, 0, runtime.spatial.mobile_id(static_cast<size_t>(selected - 200001))};
      } else if(selected >= 100001) {
        sensors.state.current.identity = {target_kind::scenery, 1, static_cast<uint32_t>(selected - 100001)};
      } else if(selected) sensors.state.current.identity = {target_kind::scenery, 0, static_cast<uint32_t>(selected - 1)};
      for(auto &v : sensors.state.current.position) input >> v;
      view.rotation = runtime.trig.transformation(angles[0], angles[1], angles[2]);
      size_t messages{0};
      sensors.message = [&](std::string_view){++messages;};
      sensors.pick_optical(view, heading, apache::render::projection{0, 0, width, height}, mode, faults);
      output << "sight " << sensors.state.locked << ' ' << sensors.state.tads << ' ' << id(sensors.state.current);
      for(auto v : sensors.state.current.position) output << ' ' << v;
      output << ' ' << id(sensors.optical) << ' ' << sensors.optical_symbol.has_value();
      if(sensors.optical_symbol) {
        for(auto v : *sensors.optical_symbol) output << ' ' << v;
      }
      output << ' ' << messages << '\n';
    } else {
      throw std::runtime_error{"Unknown sensor command"};
    }
    if(!input) throw std::runtime_error{"Incomplete sensor command"};
  }
}
