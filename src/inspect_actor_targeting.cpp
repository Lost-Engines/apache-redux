#include "inspect_actor_targeting.h"
#include <istream>
#include <ostream>
#include <stdexcept>
#include <string>
#include "simulation/actor_targeting.h"

void inspect_actor_targeting(std::istream &input, std::ostream &output, apache::resources::data_store const &data) {
  namespace math = apache::math;
  namespace simulation = apache::simulation;
  auto const types{simulation::actor_types()}; auto const weapons{simulation::weapon_definitions()}; math::trigonometry const trig{data};
  apache::resources::executable_data const executable{data}; simulation::combat_tables const tables{executable};
  auto const read{[&](auto &v){ for(auto &x : v) input >> x; }};
  auto const write{[&](auto const &v){ for(auto x : v) output << ' ' << x; }};
  auto const path{[&]{ simulation::flight_path p; int mask{0}; input >> mask >> p.type; std::array<math::vector3, 3> points; for(auto &v : points) read(v); if(mask & 1) p.previous = points[0]; if(mask & 2) p.current = points[1]; if(mask & 4) p.next = points[2]; return p; }};
  std::string command;
  while(input >> command) {
    if(command == "attention") {
      int32_t task{0}, side{0}, level{0}; input >> task >> side >> level;
      output << simulation::attention_range(task, side, tables.skill(level)) << '\n'; continue;
    }
    simulation::actor_state actor; simulation::actor_armament armament; simulation::actor_attention attention;
    if(command == "behind") { math::vector3 p; read(actor.object.world.position); read(actor.object.world.rotation); read(p); output << simulation::target_behind_actor(actor, p) << '\n'; continue; }
    if(command == "path") { auto const route{path()}; math::vector3 p; read(p); output << simulation::distance_from_flight_path(route, p, trig) << '\n'; continue; }
    bool has_target{false}, clear{false}, candidate{false}; int level{0}, damage{0}, processed{0}; simulation::actor_targeting_context context; simulation::combat_random random; int32_t message_time{0}, height{0}; std::vector<int64_t> events;
    input >> actor.object.id >> actor.conditions >> actor.object.status >> actor.classification >> actor.weapon >> actor.weapon_slot >> actor.retreat_until >> has_target >> actor.speed >> actor.destroyed >> actor.network >> attention.range >> damage >> processed;
    attention.damage = static_cast<uint8_t>(damage); attention.processed_damage = static_cast<uint8_t>(processed);
    read(actor.ammunition); read(armament.deadlines); input >> armament.ground_maximum >> armament.ground_minimum >> armament.air_maximum >> armament.air_minimum;
    input >> context.now >> context.task >> context.start_time >> context.has_formation >> context.radar_model >> level >> random.value >> random.global_seed >> message_time >> clear >> candidate >> height;
    read(actor.object.world.position); read(actor.object.world.rotation); context.path = path();
    std::array<simulation::shared_target, 5> targets; std::array<int32_t, 5> classes{};
    for(uint32_t i{1}; i != 4; ++i) { unsigned int kind{0}; auto t{std::make_shared<simulation::target_reference>()}; input >> kind >> t->classification >> classes[i]; t->identity = {static_cast<simulation::target_kind>(kind), 0, i}; read(t->position); targets[i] = t; }
    if(has_target) armament.target = targets[1]; context.player_target = targets[3];
    auto const event{[&](std::initializer_list<int64_t> values){ events.insert(events.end(), values); }};
    simulation::actor_targeting_callbacks callbacks;
    callbacks.describe = [](auto const &t){ return simulation::movement_target{t.position, {}, 0, t.classification}; };
    callbacks.actor_class = [&](auto const &t){ return classes.at(t.identity.index); };
    callbacks.search = [&](auto const &, int32_t side, int32_t minimum, int32_t maximum, simulation::search_kind kind){ event({kind == simulation::search_kind::air ? 1 : 2, side, minimum, maximum}); return candidate ? targets[2] : nullptr; };
    callbacks.ground_sector = [&](int32_t x, int32_t y, int32_t side){ event({3, x, y, side}); return candidate ? targets[2] : nullptr; };
    callbacks.visible = [&](auto const &t){ event({4, t.identity.index}); return clear; };
    callbacks.released = [&]{ event({5}); };
    callbacks.targeted = [&](auto const &t){ event({6, t.identity.index}); };
    callbacks.fixed = [&](math::vector3 const &p){ event({7, p[0], p[1], p[2]}); targets[4] = std::make_shared<simulation::target_reference>(simulation::target_reference{{simulation::target_kind::fixed, 0, 4}, p, 1}); return targets[4]; };
    callbacks.height = [&](int32_t x, int32_t y){ event({8, x, y}); return height; };
    callbacks.sound = [&](unsigned int sound){ event({9, sound}); };
    callbacks.message = [&](std::string_view text){ event({10, text == "Designate!" ? 1 : 2}); };
    callbacks.cap = [&]{ event({11}); armament.target = candidate ? targets[2] : nullptr; };
    simulation::actor_targeting targeting{actor, armament, attention, types.at(actor.object.id), weapons, trig, tables.skill(level), tables.skill(9), random, message_time, context, callbacks};
    if(command == "basic") targeting.basic_aircraft(); else if(command == "strategic") targeting.strategic_bomber(); else if(command == "tactical") targeting.tactical_bomber(); else if(command == "laser") targeting.laser_bomber(); else if(command == "strike") targeting.ground_strike(); else if(command == "air") targeting.aircraft(); else if(command == "ground") targeting.air_defence(); else if(command == "artillery") targeting.artillery(); else if(command == "lock") targeting.check_air_lock(); else throw std::runtime_error{"Unknown actor targeting operation"};
    if(!input) throw std::runtime_error{"Incomplete actor targeting input"};
    output << actor.conditions << ' ' << actor.weapon << ' ' << actor.weapon_slot << ' ' << actor.retreat_until << ' ' << (armament.target ? armament.target->identity.index : 0) << ' ' << int{attention.processed_damage}; write(armament.deadlines);
    output << ' ' << random.value << ' ' << random.global_seed << ' ' << message_time << ' ' << events.size(); write(events); output << '\n';
  }
}
