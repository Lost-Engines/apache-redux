#include "inspect_targeting.h"
#include <iomanip>
#include <istream>
#include <ostream>
#include <stdexcept>
#include <vector>
#include "simulation/player_route.h"
#include "simulation/player_targeting.h"

void inspect_targeting(apache::resources::data_store const &data, std::istream &input, std::ostream &output) {
  using apache::simulation::player_route;
  using apache::simulation::player_targeting;
  using apache::simulation::radar_target_priority;
  using apache::simulation::route_events;
  using apache::simulation::target_description;
  using apache::simulation::target_kind;
  using apache::simulation::target_priority_inputs;
  using apache::simulation::target_reference;
  using apache::simulation::targeting_events;
  apache::math::trigonometry trig{data};
  std::string command;
  while(input >> command) {
    if(command == "route") {
      player_route state;
      unsigned int action{0};
      unsigned int count{0};
      unsigned int mode{0};
      unsigned int key{0};
      unsigned int surface{0};
      int index{-1};
      bool handled{false};
      apache::math::vector3 position;
      input >> action >> count >> index >> state.last_distance >> state.landing_message >> mode >> key >> handled >> surface;
      for(auto &v : position) input >> v;
      std::vector<apache::resources::route_waypoint> path(count);
      for(auto &point : path) {
        input >> point.type;
        for(auto &v : point.position) input >> v;
      }
      std::optional<size_t> current;
      if(index >= 0) current = static_cast<size_t>(index);
      std::vector<int64_t> calls;
      route_events callbacks;
      callbacks.key_event = [&](uint16_t k){
        calls.insert(calls.end(), {1, k});
        return handled;
      };
      callbacks.achieved = [&](size_t i){calls.insert(calls.end(), {2, static_cast<int64_t>(i)});};
      callbacks.sound = [&](unsigned int id){calls.insert(calls.end(), {3, id});};
      callbacks.surface = [&](int32_t x, int32_t y){
        calls.insert(calls.end(), {4, x, 5, y});
        return surface;
      };
      if(action == 0) {
        state.update(path, current, position, callbacks);
      } else {
        state.cycle(path, current, action == 1, static_cast<uint16_t>(key), mode, callbacks);
      }
      output << (current ? static_cast<int64_t>(*current) : -1) << ' ' << state.last_distance << ' ' << state.landing_message << ' ' << calls.size() / 2;
      for(auto v : calls) output << ' ' << v;
      output << '\n';
    } else if(command == "priority") {
      target_priority_inputs f;
      unsigned int kind{0};
      input >> kind >> f.source >> f.enabled >> f.model_priority >> f.marked >> f.heading;
      f.kind = static_cast<target_kind>(kind);
      for(auto &v : f.offset) input >> v;
      output << radar_target_priority(f, trig) << '\n';
    } else {
      player_targeting state;
      unsigned int mode{0};
      unsigned int faults{0};
      unsigned int key{0};
      unsigned int value{0};
      unsigned int kind{0};
      bool handled{false};
      input >> mode >> faults >> key >> handled >> value;
      input >> state.tads >> state.automatic >> state.locked >> state.previous_tads >> state.laser >> state.cycle_direction >> state.ground >> state.air >> state.tactical >> state.frozen >> state.automatic_ecm >> state.scan_all >> state.threat >> state.ground_range_index >> state.ground_range >> state.ground_corner >> state.ground_mark >> state.air_range >> state.tactical_range >> state.tactical_corner >> state.tactical_mark >> kind >> state.current.identity.index;
      state.current.identity.kind = static_cast<target_kind>(kind);
      state.name = "BEFORE";
      std::vector<int64_t> events;
      targeting_events callbacks;
      callbacks.key_event = [&](uint16_t k){
        events.insert(events.end(), {1, k});
        return handled;
      };
      callbacks.cockpit_sound = [&](unsigned int id){events.insert(events.end(), {2, id});};
      callbacks.show_radar = [&]{events.insert(events.end(), {3, 0});};
      if(command == "cycle-target") {
        state.cycle_target(value != 0, mode);
      } else if(command == "auto") {
        state.toggle_automatic(mode, faults);
      } else if(command == "lock") {
        state.toggle_lock(static_cast<uint16_t>(key), mode, faults, callbacks);
      } else if(command == "cycle-radar") {
        state.cycle_radar(static_cast<uint16_t>(key), mode, callbacks);
      } else if(command == "set-radar") {
        state.set_radar(value, faults, callbacks);
      } else if(command == "freeze") {
        state.toggle_freeze(static_cast<uint16_t>(key), mode, callbacks);
      } else if(command == "ecm") {
        state.toggle_ecm(static_cast<uint16_t>(key), mode, callbacks);
      } else if(command == "range") {
        state.cycle_ground_range(static_cast<uint16_t>(key), mode, callbacks);
      } else if(command == "tactical-range") {
        state.toggle_tactical_range();
      } else if(command == "select") {
        std::vector<target_reference> ranked;
        for(uint32_t i{1}; i <= value; ++i) ranked.push_back({{target_kind::scenery, 0, i}, {}, 1});
        target_reference optical;
        unsigned int optical_kind{0};
        int32_t optical_status{0};
        input >> optical_kind >> optical.identity.index >> optical_status;
        optical.identity.kind = static_cast<target_kind>(optical_kind);
        state.update(ranked, optical, [&](auto const &target){
          target_description d;
          d.name = "CONTACT";
          d.status = target.identity.index == optical.identity.index ? optical_status : target.identity.index % 2 ? 0x800 : 0;
          d.designated = (d.status & 0x800) != 0;
          return d;
        });
      } else {
        throw std::runtime_error{"Unknown targeting command"};
      }
      output << state.tads << ' ' << state.automatic << ' ' << state.locked << ' ' << state.previous_tads << ' ' << state.laser << ' ' << state.cycle_direction << ' ' << state.ground << ' ' << state.air << ' ' << state.tactical << ' ' << state.frozen << ' ' << state.automatic_ecm << ' ' << state.scan_all << ' ' << state.threat << ' ' << state.ground_range_index << ' ' << state.ground_range << ' ' << state.ground_corner << ' ' << state.ground_mark << ' ' << state.air_range << ' ' << state.tactical_range << ' ' << state.tactical_corner << ' ' << state.tactical_mark << ' ' << static_cast<unsigned int>(state.current.identity.kind) << ' ' << state.current.identity.index << ' ' << std::quoted(state.name) << ' ' << events.size() / 2;
      for(auto v : events) output << ' ' << v;
      output << '\n';
    }
    if(!input) throw std::runtime_error{"Incomplete targeting command"};
  }
}
