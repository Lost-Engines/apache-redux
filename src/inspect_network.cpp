#include "inspect_network.h"
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include "multiplayer/casualties.h"
#include "multiplayer/crew.h"
#include "multiplayer/lifecycle.h"
#include "multiplayer/lobby.h"
#include "multiplayer/packet.h"
#include "multiplayer/replication.h"
#include "multiplayer/rules.h"
#include "simulation/mission_end.h"
#include "simulation/remote_weapons.h"

namespace apache {
namespace {
resources::bytes hex_bytes(std::string const &hex) {
  if(hex.size() % 2) throw std::runtime_error{"Odd hex packet length"};
  resources::bytes result;
  for(size_t i{0}; i < hex.size(); i += 2) result.push_back(static_cast<uint8_t>(std::stoul(hex.substr(i, 2), nullptr, 16)));
  return result;
}
}
void inspect_network(std::istream &input, std::ostream &output) {
  std::string line;
  while(std::getline(input, line)) {
    if(line.empty()) continue;
    std::istringstream in{line}; std::string command; in >> command;
    if(command == "lobby") {
      multiplayer::replication state; multiplayer::lobby lobby{state}; unsigned int mode, source;
      in >> state.local >> state.server >> state.active >> mode >> lobby.flow >> lobby.other_players >> lobby.remote_rank >> lobby.second_product >> lobby.selected_mission >> lobby.rank >> lobby.active >> lobby.cd_available >> lobby.cd_found >> std::quoted(lobby.own_name) >> std::quoted(lobby.last_message);
      lobby.mode = static_cast<multiplayer::game_mode>(mode);
      for(auto &value : lobby.preferences) in >> value;
      for(auto &value : lobby.two_player) in >> value;
      for(auto &player : state.players) in >> player.flags >> player.aircraft.model >> std::quoted(player.name);
      in >> source; std::string hex; in >> hex;
      if(!in) throw std::runtime_error{"Incomplete lobby context"};
      std::ostringstream events;
      lobby.events.connection_name = [&](auto id, auto name){ events << " name " << id << ' ' << std::quoted(std::string{name}); };
      lobby.events.identify = [&](auto id){ events << " identify " << id; };
      lobby.events.announce = [&]{ events << " announce"; };
      lobby.events.refresh = [&]{ events << " refresh"; };
      lobby.events.recount = [&]{ events << " recount"; };
      lobby.events.preferences_changed = [&]{ events << " preferences"; };
      lobby.events.reset_connection = [&]{ events << " reset"; };
      lobby.events.settings_changed = [&]{ events << " settings"; };
      lobby.events.start = [&]{ events << " start"; };
      lobby.events.buttons = [&](bool enabled){ events << " buttons " << enabled; };
      lobby.events.selection = [&](unsigned int choice, auto const &arguments){ events << " selection " << choice; for(auto value : arguments) events << ' ' << value; };
      lobby.events.message = [&](auto text){ events << " message " << std::quoted(std::string{text}); };
      lobby.events.chat = [&](auto text){ events << " chat " << std::quoted(std::string{text}); };
      lobby.events.cd_reply = [&](auto id){ events << " cd-reply " << id; };
      auto const message{multiplayer::packet::decode(hex_bytes(hex))};
      if(!message) throw std::runtime_error{"Invalid lobby packet"};
      lobby.receive(*message, source);
      output << lobby.other_players << ' ' << lobby.remote_rank << ' ' << lobby.second_product << ' ' << lobby.selected_mission << ' ' << lobby.rank << ' ' << lobby.active << ' ' << lobby.cd_found << ' ' << std::quoted(lobby.own_name) << ' ' << std::quoted(lobby.last_message);
      for(auto value : lobby.preferences) output << ' ' << value;
      for(auto value : lobby.two_player) output << ' ' << value;
      for(auto const &player : state.players) output << ' ' << player.flags << ' ' << player.aircraft.model << ' ' << std::quoted(player.name);
      output << events.str() << '\n'; continue;
    }
    if(command == "mission-end") {
      simulation::mission_end state; bool invulnerable, no_crashes; unsigned int reason, frames;
      in >> state.cause >> state.active >> state.connected >> state.countdown >> state.advance_frames >> state.quitting >> invulnerable >> no_crashes >> reason >> frames;
      if(!in) throw std::runtime_error{"Incomplete mission ending context"};
      bool const crash{state.request(reason, invulnerable, no_crashes)};
      output << state.cause << ' ' << state.active << ' ' << state.countdown << ' ' << crash;
      for(unsigned int i{0}; i < frames; ++i) state.finish_frame();
      output << ' ' << state.active << ' ' << state.countdown << ' ' << state.quitting << '\n'; continue;
    }
    if(command == "rules") {
      multiplayer::game_settings settings; multiplayer::mission_rules state; unsigned int mode;
      in >> mode >> settings.second_seat >> settings.night >> settings.difficult >> settings.weather >> settings.infinite_fuel >> settings.infinite_weapons >> settings.crashes;
      settings.mode = static_cast<multiplayer::game_mode>(mode);
      in >> state.control >> state.rank >> state.forces >> state.radio >> state.wind_strength >> state.wind_direction >> state.weather >> state.time_of_day >> state.client_server >> state.no_crashes >> state.infinite_fuel >> state.infinite_weapons;
      if(!in) throw std::runtime_error{"Incomplete multiplayer rules"};
      state = multiplayer::apply_rules(settings, state);
      output << state.control << ' ' << state.rank << ' ' << state.forces << ' ' << state.radio << ' ' << state.wind_strength << ' ' << state.wind_direction << ' ' << state.weather << ' ' << state.time_of_day << ' ' << state.client_server << ' ' << state.no_crashes << ' ' << state.infinite_fuel << ' ' << state.infinite_weapons << '\n';
      continue;
    }
    if(command == "remote-fire") {
      simulation::remote_fire_input state; int32_t height; unsigned int kind; bool found;
      in >> state.weapon >> state.seed >> kind >> found >> height;
      for(auto &v : state.aircraft.position) in >> v; for(auto &v : state.aircraft.rotation) in >> v;
      for(auto &v : state.gun_direction) in >> v;
      state.source.identity = {simulation::target_kind::mobile, 0, 1};
      if(found && (kind == 1 || kind == 2)) state.target = simulation::target_reference{{static_cast<simulation::target_kind>(kind), 0, 2}, {}, 0};
      auto const shots{simulation::remote_weapon_shots(state, [height](int32_t, int32_t){ return height; })};
      output << shots.size();
      for(auto const &shot : shots) {
        output << ' ' << static_cast<unsigned int>(shot.kind) << ' ' << shot.request.weapon << ' ' << shot.seed << ' ' << shot.request.selected.has_value() << ' ' << shot.request.designator.has_value();
        for(auto v : shot.request.launch.pose.position) output << ' ' << v; for(auto v : shot.request.launch.pose.rotation) output << ' ' << v;
      }
      output << '\n'; continue;
    }
    if(command == "casualties") {
      multiplayer::casualties queue; unsigned int count, sender; bool scenery;
      in >> scenery >> sender >> count;
      for(unsigned int i{0}; i < count; ++i) {
        int32_t first, second; unsigned int source; in >> first >> second >> source;
        output << (scenery ? queue.scenery(first, second, static_cast<uint8_t>(source)) : queue.mobile(first, second, static_cast<uint8_t>(source))) << ' ';
      }
      auto result{scenery ? queue.take_scenery(sender, "PILOT123") : queue.take_mobile(sender, "PILOT123")};
      if(result) for(auto b : result->bytes()) output << std::hex << std::setfill('0') << std::setw(2) << static_cast<unsigned int>(b);
      else output << "empty";
      output << std::dec << ' ' << queue.take_mobile(sender, "PILOT123").has_value() << ' ' << queue.take_scenery(sender, "PILOT123").has_value() << '\n'; continue;
    }
    if(command == "lifecycle" || command == "departure") {
      multiplayer::replication state; multiplayer::lifecycle life{state}; unsigned int mode;
      in >> state.local >> state.server >> state.active >> state.client_server >> mode >> life.following >> life.other_players;
      life.mode = static_cast<multiplayer::game_mode>(mode);
      std::array<int, 16> assignments{}, destroyed{};
      for(size_t i{0}; i < 16; ++i) {
        auto &p{state.players[i]}; in >> assignments[i] >> destroyed[i] >> p.flags >> p.score >> p.aircraft.model;
        p.name = "PLAYER" + std::to_string(i); p.assigned = assignments[i] >= 0;
      }
      std::ostringstream events;
      auto link{[&](size_t id){ return multiplayer::actor_link{id, destroyed[id], id == state.local}; }};
      life.events.player = [&](uint32_t id)->std::optional<multiplayer::actor_link> {
        if(id >= assignments.size() || assignments[id] < 0) return {}; return link(static_cast<size_t>(assignments[id]));
      };
      life.events.find = [&](int32_t serial, int32_t task)->std::optional<multiplayer::actor_link> {
        if(serial < 0 || serial >= 16 || task != serial % 3) return {}; return link(static_cast<size_t>(serial));
      };
      life.events.remove = [&](size_t id){ events << " remove " << id; destroyed[id] = 1; };
      life.events.leave = [&](size_t id){ events << " leave " << id; destroyed[id] = 1; };
      life.events.restart = [&](size_t id, math::pose const &pose, int32_t model){
        events << " restart " << id << ' ' << model;
        for(auto v : pose.position) events << ' ' << v; for(auto v : pose.rotation) events << ' ' << v;
        destroyed[id] = 0;
      };
      life.events.countermeasure = [&](size_t id, bool flare){ events << (flare ? " flare " : " chaff ") << id; };
      life.events.death = [&](unsigned int cause){ events << " death " << cause; };
      life.events.player_killed = [&]{ events << " killed"; };
      life.events.mobile_killed = [&](size_t id, uint8_t source){ events << " mobile " << id << ' ' << static_cast<unsigned int>(source); };
      life.events.scenery_killed = [&](math::vector3 const &position, uint8_t source){ events << " scenery"; for(auto v : position) events << ' ' << v; events << ' ' << static_cast<unsigned int>(source); };
      life.events.flag_won = [&](uint32_t id){ events << " flag " << id; };
      life.events.notice = [&](std::string_view text, int32_t priority, int32_t lifetime){ events << " notice " << std::quoted(std::string{text}) << ' ' << priority << ' ' << lifetime; };
      life.events.scores = [&]{ events << " scores"; };
      int32_t now{0}; uint32_t next_server{0};
      if(command == "departure") {
        in >> now >> next_server >> state.server_in_game >> state.server_waiting >> state.new_server_data >> state.all_synced >> state.answer_deadline;
        life.events.unregister = [&](uint32_t slot){ events << " unregister " << slot; };
        life.events.next_server = [&](uint32_t slot){ events << " next " << slot; return next_server; };
        life.events.unplace = [&](size_t id){ events << " leave " << id; };
      }
      std::string hex; in >> hex; auto const message{multiplayer::packet::decode(hex_bytes(hex))};
      if(!in || !message) throw std::runtime_error{"Incomplete lifecycle context"};
      life.receive(*message, now);
      output << state.active << ' ' << life.following << ' ' << life.other_players;
      for(size_t i{0}; i < 16; ++i) output << ' ' << state.players[i].flags << ' ' << state.players[i].score << ' ' << destroyed[i];
      if(command == "departure") output << ' ' << state.server << ' ' << state.server_in_game << ' ' << state.server_waiting << ' ' << state.new_server_data << ' ' << state.all_synced << ' ' << state.answer_deadline;
      output << events.str() << '\n'; continue;
    }
    if(command == "exchange") {
      multiplayer::replication state; unsigned int seat, selector; int32_t now; in >> state.local >> state.server >> state.active >> state.client_server >> state.dead >> seat >> now >> state.ping_timer >> selector;
      state.seat = static_cast<multiplayer::crew_role>(seat);
      state.war_seed = 123; state.weapon_seed = 456;
      state.synced[selector % 16] = true; state.initial_synced[selector % 16] = true;
      state.number_synced = state.number_initial_synced = 1;
      for(auto &p : state.players) {
        in >> p.assigned >> p.flags >> p.score >> p.updated_at >> p.frame_time >> p.accumulated_frame_time >> p.weapon >> p.shot_seed >> p.shot_target.kind;
        for(auto &v : p.shot_target.data) in >> v;
        for(auto &v : p.aircraft.location.position) in >> v; for(auto &v : p.aircraft.location.rotation) in >> v;
        in >> p.aircraft.speed >> p.aircraft.model;
      }
      unsigned int crew_events{0}, score_events{0};
      state.crew_received = [&](uint32_t, auto const &){ ++crew_events; };
      state.score_changed = [&](uint32_t, int32_t){ ++score_events; };
      std::string method; in >> method;
      if(method == "extrapolate") state.extrapolate(now);
      else if(method == "reset") state.reset_server();
      else if(method == "frame") state.begin_frame();
      else {
        auto const value{multiplayer::packet::decode(hex_bytes(method))};
        if(!value) throw std::runtime_error{"Invalid exchange packet"};
        state.receive(*value, now);
      }
      if(!in) throw std::runtime_error{"Incomplete replication context"};
      output << std::setprecision(17) << state.server << ' ' << state.number_synced << ' ' << state.number_initial_synced << ' ' << state.wind_strength << ' ' << state.wind_direction << ' ' << state.war_seed << ' ' << state.weapon_seed << ' ' << state.server_in_game << ' ' << state.server_waiting << ' ' << state.new_server_data << ' ' << state.all_synced << ' ' << state.answer_deadline << ' ' << state.client_frames << ' ' << state.ping_timer << ' ' << state.interpolated_time << ' ' << crew_events << ' ' << score_events << ' ';
      for(auto v : state.synced) output << v << ' '; for(auto v : state.initial_synced) output << v << ' ';
      for(auto const &p : state.players) {
        output << p.flags << ' ' << p.score << ' ' << p.updated_at << ' ' << p.frame_time << ' ' << p.accumulated_frame_time << ' ' << p.weapon << ' ' << p.shot_seed << ' ' << p.shot_target.kind << ' ';
        for(auto v : p.shot_target.data) output << v << ' ';
        for(auto v : p.aircraft.location.position) output << v << ' '; for(auto v : p.aircraft.location.rotation) output << v << ' ';
        output << p.aircraft.speed << ' ' << p.aircraft.model << ' ';
      }
      output << '\n'; continue;
    }
    if(command == "crew-pack" || command == "crew-read" || command == "crew-valid") {
      unsigned int selected; in >> selected; auto const role{static_cast<multiplayer::crew_role>(selected)};
      multiplayer::crew_state state;
      if(command == "crew-pack") {
        unsigned int death, message, ticks, landing, count; in >> state.leaving >> death >> message >> count;
        state.death = static_cast<uint8_t>(death); state.message = static_cast<uint8_t>(message);
        for(unsigned int i{0}; i < count; ++i) { uint16_t key; in >> key; state.keys.push_back(key); }
        if(role == multiplayer::crew_role::pilot) {
          auto &p{state.pilot}; for(auto &v : p.angles) in >> v; for(auto &v : p.position) in >> v;
          in >> p.flight_time >> p.climb >> p.fuel >> p.weight >> p.torque >> p.sidespeed >> p.airspeed;
          for(auto &v : p.engines) in >> v;
          in >> ticks >> landing >> p.landed; p.frame_ticks = static_cast<uint8_t>(ticks); p.landing = static_cast<uint8_t>(landing);
        } else {
          auto &g{state.gunner}; in >> g.hydra_elevation; for(auto &v : g.display_position) in >> v; for(auto &v : g.position) in >> v;
          in >> g.target_kind >> g.target_source >> g.drone >> g.task;
        }
        if(!in) throw std::runtime_error{"Incomplete crew state"};
        for(auto b : multiplayer::encode_crew(role, state)) output << std::hex << std::setfill('0') << std::setw(2) << static_cast<unsigned int>(b);
        output << std::dec << ' ' << static_cast<unsigned int>(state.message) << ' ' << state.keys.size() << ' ' << static_cast<unsigned int>(state.pilot.landing) << '\n';
      } else {
        std::string hex; in >> hex; auto const value{multiplayer::decode_crew(role, hex_bytes(hex))};
        if(command == "crew-valid") { output << value.has_value() << '\n'; continue; }
        if(!value) throw std::runtime_error{"Invalid crew state"};
        state = *value;
        output << state.leaving << ' ' << static_cast<unsigned int>(state.death) << ' ' << static_cast<unsigned int>(state.message) << ' ' << state.keys.size() << ' ';
        for(auto key : state.keys) output << key << ' ';
        if(role == multiplayer::crew_role::pilot) {
          auto const &p{state.pilot}; for(auto v : p.angles) output << v << ' '; for(auto v : p.position) output << v << ' ';
          output << std::setprecision(17) << p.flight_time << ' ' << p.climb << ' ' << p.fuel << ' ' << p.weight << ' ' << p.torque << ' ' << p.sidespeed << ' ' << p.airspeed << ' ';
          for(auto v : p.engines) output << v << ' ';
          output << static_cast<unsigned int>(p.frame_ticks) << ' ' << static_cast<unsigned int>(p.landing) << ' ' << p.landed;
        } else {
          auto const &g{state.gunner}; output << g.hydra_elevation << ' '; for(auto v : g.display_position) output << v << ' '; for(auto v : g.position) output << v << ' ';
          output << g.target_kind << ' ' << g.target_source << ' ' << g.drone << ' ' << g.task;
        }
        output << '\n';
      }
      continue;
    }
    if(command == "crew-keys") {
      unsigned int mode, count; in >> mode >> count; multiplayer::crew_state state;
      for(unsigned int i{0}; i < count; ++i) { uint16_t key; in >> key; output << state.notify_key(key, mode) << ' '; }
      output << state.keys.size(); for(auto key : state.keys) output << ' ' << key; output << '\n'; continue;
    }
    if(command == "unpack" || command == "valid") {
      std::string hex; in >> hex; auto const message{multiplayer::packet::decode(hex_bytes(hex))};
      if(command == "valid") { output << static_cast<int>(message.has_value()) << '\n'; continue; }
      if(!message) throw std::runtime_error{"Invalid packet"};
      for(auto const &player : multiplayer::server_state(*message)) {
        for(auto v : player.location.position) output << v << ' ';
        for(auto v : player.location.rotation) output << v << ' ';
        output << player.speed << ' ' << player.model << ' ';
      }
      output << '\n'; continue;
    }
    multiplayer::packet result{multiplayer::message_type::identity, 0};
    if(command == "server") {
      std::array<multiplayer::aircraft_state, 16> players;
      for(auto &player : players) {
        for(auto &v : player.location.position) in >> v;
        for(auto &v : player.location.rotation) in >> v;
        in >> player.speed >> player.model;
      }
      result = multiplayer::server_state(players);
    } else {
      uint32_t sender{0}; std::string name; in >> sender >> std::quoted(name);
      if(command == "simple") { uint32_t type; in >> type; result = {static_cast<multiplayer::message_type>(type), sender, name}; } else if(command == "nomination") { double time; in >> time; result = multiplayer::nomination(sender, name, time); } else if(command == "settings") { std::array<int32_t, 5> values; for(auto &v : values) in >> v; result = multiplayer::two_player_settings(values); } else if(command == "chat") { std::string text; in >> std::quoted(text); result = multiplayer::chat(sender, name, text); } else if(command == "crew") { std::string hex; in >> hex; auto const data{hex_bytes(hex)}; if(data.size() != 104) throw std::runtime_error{"Wrong crew payload size"}; result = multiplayer::crew(sender, name, std::span<uint8_t const, 104>{data.data(), 104}); } else if(command == "kills") { bool scenery; unsigned int count; std::string hex; in >> scenery >> count >> hex; auto const data{hex_bytes(hex)}; result = multiplayer::kills(sender, name, scenery, static_cast<uint8_t>(count), data); } else if(command == "flag") result = multiplayer::flag_won(sender, name);
      else if(command == "preferences") { std::array<int32_t, 5> values; for(auto &v : values) in >> v; result = multiplayer::preferences(sender, values); } else if(command == "weather") { int32_t strength, direction; in >> strength >> direction; result = multiplayer::weather(sender, name, strength, direction); } else if(command == "peer") { uint32_t type, model; in >> type >> model; result = multiplayer::identification(static_cast<multiplayer::message_type>(type), sender, name, static_cast<int32_t>(model)); } else if(command == "pose") {
        uint32_t type; math::pose value; int32_t speed; multiplayer::target_state target; double delta;
        in >> type; for(auto &v : value.position) in >> v; for(auto &v : value.rotation) in >> v;
        in >> speed >> target.kind; for(auto &v : target.data) in >> v; in >> delta;
        if(type == 12) result = multiplayer::position(sender, value, speed, target);
        else if(type == 20) result = multiplayer::restarted(sender, name, value);
        else result = multiplayer::client_state(sender, value, speed, delta);
      } else if(command == "scores") { std::array<int32_t, 16> values; for(auto &v : values) in >> v; result = multiplayer::scores(sender, name, values); } else if(command == "event") { uint32_t type; int32_t first, second; in >> type >> first >> second; result = multiplayer::event(static_cast<multiplayer::message_type>(type), sender, name, first, second); } else if(command == "menu") { uint32_t selected; std::array<int32_t, 5> values; in >> selected; for(auto &v : values) in >> v; result = multiplayer::menu_selection(sender, name, selected, values); } else if(command == "initial") { math::vector3 position; uint32_t war, weapon; for(auto &v : position) in >> v; in >> war >> weapon; result = multiplayer::initial_state(sender, name, position, war, weapon); } else if(command == "fire") { uint32_t weapon, shot_seed; multiplayer::target_state target; math::vector3 gun; in >> weapon >> shot_seed >> target.kind; for(auto &v : target.data) in >> v; for(auto &v : gun) in >> v; result = multiplayer::fired(sender, name, weapon, shot_seed, target, gun); } else throw std::runtime_error{"Unknown network inspection command: " + command};
    }
    if(!in) throw std::runtime_error{"Incomplete network inspection command"};
    for(auto const b : result.bytes()) output << std::hex << std::setfill('0') << std::setw(2) << static_cast<unsigned int>(b);
    output << std::dec << '\n';
  }
}
} // namespace apache
