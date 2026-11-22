#include "inspect_player_weapons.h"
#include <iomanip>
#include <istream>
#include <ostream>
#include <stdexcept>
#include <string>
#include <vector>
#include "simulation/player_sight.h"
#include "simulation/player_warnings.h"
#include "simulation/player_weapon_mounts.h"
#include "simulation/player_weapons.h"

void inspect_player_weapons(apache::resources::data_store const &data, std::istream &input, std::ostream &output) {
    using apache::simulation::player_shot;
  using apache::simulation::player_weapon_context;
  using apache::simulation::player_weapons;
  apache::resources::executable_data executable{data}; player_weapons state{executable}; std::string command;
  while(input >> command) {
    player_weapon_context context; unsigned int key{0}; bool handled{false}; uint32_t failures{0}; apache::math::pose pose;
    input >> context.now >> context.control_mode >> context.game_mode >> context.frame_time >> context.dead >> context.landed >> context.active >> context.ground_radar >> context.air_radar >> context.target_kind >> context.faults >> key >> handled >> failures;
    input >> state.current >> state.helmet_mode >> state.armed >> state.infinite >> state.automatic_chaff >> state.gun_fired;
    for(auto &p : state.pylons) input >> p.weapon >> p.ammunition;
    for(auto &v : state.ammunition) input >> v; for(auto &v : state.selected) input >> v; for(auto &v : state.deadlines) input >> v;
    for(auto &v : pose.position) input >> v; for(auto &v : pose.rotation) input >> v;
    std::vector<int64_t> events;
    auto add{[&](int code, int64_t value){ events.push_back(code); events.push_back(value); }};
    unsigned int rounds{0};
    state.events.key_event = [&](uint16_t k){ add(0, k); return handled; };
    state.events.cockpit_sound = [&](unsigned int id){ add(1, id); };
    state.events.launch_sound = [&](unsigned int id){ add(2, id); };
    state.events.clear_impact_times = [&]{ add(3, 0); };
    state.events.cycle_target = [&]{ add(4, 0); };
    state.events.fired_at = [&]{ add(5, 0); };
    state.events.launch_pose = [&](unsigned int id, size_t){ add(6, id); return pose; };
    state.events.launch = [&](player_shot const &shot)->std::optional<size_t> {
      add(7, shot.weapon); for(auto v : shot.pose.position) add(8, v); for(auto v : shot.pose.rotation) add(8, v);
      add(9, shot.laser); ++rounds; if((failures >> ((rounds - 1) & 31)) & 1) return {};
      add(10, shot.guided ? 0 : shot.untracked ? 4 : 0); return rounds;
    };
    state.events.missile_launched = [&](size_t r){ add(11, r); };
    state.events.impact_time = [&](size_t r){ add(12, r); };
    state.events.remote_fire = [&](size_t r){ add(13, r); };
    state.events.remove_missile = [&](size_t slot, int32_t ammo){ add(14, static_cast<int64_t>(slot)); add(15, ammo); };
    bool result{false};
    if(command == "fire") result = state.fire(static_cast<uint16_t>(key), context);
    else if(command == "select") result = state.select(static_cast<uint16_t>(key), context);
    else if(command == "next") result = state.next(context);
    else if(command == "toggle") result = state.toggle_countermeasures(static_cast<uint16_t>(key), context);
    else if(command == "available") result = state.available(state.current, context.now);
    else if(command == "rearm") state.rearm();
    else throw std::runtime_error{"Unknown player-weapon command"};
    if(!input) throw std::runtime_error{"Incomplete player-weapon command"};
    output << result << ' ' << state.current << ' ' << state.helmet_mode << ' ' << state.armed << ' ' << state.infinite << ' ' << state.automatic_chaff << ' ' << state.gun_fired << ' ';
    for(auto const &p : state.pylons) output << p.weapon << ' ' << p.ammunition << ' ';
    for(auto v : state.ammunition) output << v << ' '; for(auto v : state.selected) output << v << ' '; for(auto v : state.deadlines) output << v << ' ';
    output << events.size() / 2 << ' '; for(auto v : events) output << v << ' '; output << '\n';
  }
}

void inspect_player_mounts(apache::resources::data_store const &data, std::istream &input, std::ostream &output) {
    using apache::simulation::movement_target;
  using apache::simulation::player_pod_models;
  using apache::simulation::player_pylon;
  using apache::simulation::player_weapon_mounts;
  auto models{apache::resources::read_models(data.read("3DDATA/3DOBJS.BIN"))}; apache::resources::executable_data executable{data}; apache::resources::initialise_model_metadata(models, executable); apache::math::trigonometry trig{data};
  int32_t now{0};
  while(input >> now) {
    int32_t delta{0}, control{0}, height{0}; unsigned int weapon{0}; bool tracked{false}; apache::math::pose pose; apache::math::vector3 helmet; movement_target target; std::array<player_pylon, 9> pylons; player_weapon_mounts mounts{trig};
    input >> delta >> control >> weapon >> tracked >> height >> mounts.hydra_elevation;
    for(auto &v : pose.position) input >> v; for(auto &v : pose.rotation) input >> v;
    for(auto &v : target.position) input >> v; for(auto &v : target.direction) input >> v; input >> target.speed;
    for(auto &v : helmet) input >> v; for(auto &p : pylons) input >> p.weapon >> p.ammunition;
    auto const pods{player_pod_models(pylons, models)}; auto body{apache::render::construct_model(models, 103, pose, pods)}; apache::render::maintain_model(body);
    for(auto v : pods) output << v << ' ';
    mounts.update_hydra(now, delta, control, pose, tracked ? std::optional{target.position} : std::nullopt, height);
    output << mounts.hydra_elevation << ' ' << mounts.hydras_locked << ' ';
    output << mounts.position_gun(now, weapon, body, tracked ? std::optional{target} : std::nullopt, helmet) << ' ';
    for(auto v : mounts.intercept) output << v << ' ';
    for(unsigned int id{1}; id != 9; ++id) {
      size_t slot{6}; for(size_t j{0}; j != 9; ++j) if(pylons[j].weapon == id) { slot = j; break; }
      auto const launch{mounts.launch_pose(id, slot, pylons[slot].ammunition, body)};
      for(auto v : launch.position) output << v << ' '; for(auto v : launch.rotation) output << v << ' ';
    }
    mounts.update_hydra(now, delta, control, pose, tracked ? std::optional{target.position} : std::nullopt, height);
    output << mounts.hydra_elevation << ' ' << mounts.hydras_locked << '\n';
    if(!input) throw std::runtime_error{"Incomplete player mount case"};
  }
}

void inspect_player_sight(apache::resources::data_store const &data, std::istream &input, std::ostream &output) {
    using apache::simulation::player_impact_times;
  using apache::simulation::player_weapon_sight;
  using apache::simulation::target_kind;
  using apache::simulation::weapon_sight_callbacks;
  using apache::simulation::weapon_sight_inputs;
  apache::math::trigonometry trig{data}; player_weapon_sight sight{trig}; player_impact_times impacts; std::string command;
  while(input >> command) {
    if(command == "sight") {
      weapon_sight_inputs f; bool same{false}, visible{false}; unsigned int gun{0}, kind{0};
      input >> f.weapon >> f.weather >> f.armed >> f.hydras_locked >> f.cloud_bottom >> f.cloud_top >> f.target_classification >> sight.status >> same >> visible >> gun >> kind;
      f.target.identity = {static_cast<target_kind>(kind), 0, 1}; sight.previous = f.target; if(!same) sight.previous.identity.index = 2;
      for(auto &v : f.body.position) input >> v; for(auto &v : f.body.rotation) input >> v; for(auto &v : f.target_position) input >> v;
      std::vector<int> events; weapon_sight_callbacks callbacks;
      callbacks.visible = [&]{ events.push_back(0); return visible; }; callbacks.position_gun = [&]{ events.push_back(1); return gun; }; callbacks.cockpit_sound = [&](unsigned int id){ events.push_back(static_cast<int>(id)); };
      output << sight.evaluate(f, callbacks) << ' ' << events.size() << ' '; for(auto v : events) output << v << ' '; output << '\n';
    } else {
      int32_t now{0}; size_t round{0}; unsigned int weapon{0}; bool target{false}; apache::math::vector3 from{}, to{};
      input >> now >> round >> weapon >> target; for(auto &v : from) input >> v; for(auto &v : to) input >> v;
      if(command == "launch") impacts.launch(round, weapon, from, target ? std::optional{to} : std::nullopt, now);
      else if(command == "remove") impacts.remove(round, weapon);
      else if(command == "clear") impacts.clear();
      else if(command != "time") throw std::runtime_error{"Unknown player sight command"};
      output << impacts.current(now) << ' ' << impacts.entries.size() << ' '; for(auto const &v : impacts.entries) output << v.round << ' ' << v.deadline << ' '; output << '\n';
    }
    if(!input) throw std::runtime_error{"Incomplete player sight case"};
  }
}

void inspect_player_warnings(std::istream &input, std::ostream &output) {
  apache::simulation::player_warnings state; std::string command;
  while(input >> command) {
    std::vector<int64_t> events;
    if(command == "begin") state.begin();
    else if(command == "incoming") { size_t round{0}; unsigned int weapon{0}; int32_t arrival{0}; input >> round >> weapon >> arrival; state.incoming(round, weapon, arrival); } else if(command == "present") {
      apache::simulation::incoming_warning_events callbacks;
      callbacks.sound = [&]{ events.insert(events.end(), {0, 0}); };
      callbacks.cockpit_sound = [&](unsigned int id){ events.insert(events.end(), {1, id}); };
      callbacks.message = [&](size_t slot, bool added, std::string_view){ events.insert(events.end(), {added ? 2 : 3, static_cast<int64_t>(slot)}); };
      state.present(callbacks);
    } else throw std::runtime_error{"Unknown warning command"};
    output << state.count << ' ' << state.previous_count;
    for(auto value : state.priorities) output << ' ' << value;
    for(auto const &entry : state.entries) output << ' ' << (entry.round ? static_cast<int64_t>(*entry.round) : -1) << ' ' << std::quoted(entry.text);
    output << ' ' << events.size() / 2; for(auto value : events) output << ' ' << value; output << '\n';
    if(!input) throw std::runtime_error{"Incomplete warning command"};
  }
}
