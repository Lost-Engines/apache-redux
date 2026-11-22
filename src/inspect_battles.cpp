#include "inspect_battles.h"
#include <istream>
#include <ostream>
#include <stdexcept>
#include <string>
#include "simulation/battles.h"

void inspect_battles(std::istream &input, std::ostream &output, apache::resources::data_store const &data) {
  namespace math = apache::math;
  namespace simulation = apache::simulation; math::trigonometry const trig{data}; auto const types{simulation::actor_types()}; auto const weapons{simulation::weapon_definitions()};
  auto const read{[&](auto &v){ for(auto &x : v) input >> x; }};
  auto const print{[&](auto const &v){ for(auto x : v) output << ' ' << x; }};
  std::string op;
  while(input >> op) {
    std::vector<int64_t> events; auto const event{[&](std::initializer_list<int64_t> e){ events.insert(events.end(), e); }};
    if(op == "survival") {
      simulation::actor_state actor; int32_t alive{0}, initial{0}; input >> actor.classification >> actor.conditions >> alive >> initial; output << simulation::battle_survival(actor, alive, initial);
    } else if(op == "schedule") {
      std::array<simulation::battle_cell, 5> cells; std::array<int32_t, 8> ratio{}; simulation::combat_random random; int32_t now{0}; input >> now >> random.value >> random.global_seed;
      for(size_t i{1}; i != 5; ++i) { input >> cells[i].active; for(size_t j{0}; j != 2; ++j) { size_t const task{(i - 1) * 2 + j}; cells[i].sides[j].task = task; input >> ratio[task] >> cells[i].sides[j].release_time; } }
      simulation::battle_callbacks cb; cb.survival = [&](size_t task, int32_t){ event({1, static_cast<int64_t>(task)}); return ratio[task]; };
      cb.release = [&](size_t task){ event({2, static_cast<int64_t>(task)}); }; cb.combat = [&](size_t task){ event({3, static_cast<int64_t>(task)}); }; cb.finish = [&](size_t task, bool won){ event({4, static_cast<int64_t>(task), won}); }; cb.random = [&]{ event({5}); return random.next(); };
      simulation::update_battles(cells, now, cb); output << random.value << ' ' << random.global_seed;
      for(size_t i{1}; i != 5; ++i) for(auto const &s : cells[i].sides) output << ' ' << s.release_time;
    } else if(op == "survivor" || op == "formation" || op == "release") {
      size_t count{0}; int32_t alive{0}, now{0}; simulation::combat_random random; input >> alive >> now >> random.value >> random.global_seed >> count;
      if(count > 200) throw std::runtime_error{"Too many battle actors"};
      std::vector<simulation::actor_state> actors(count); std::vector<simulation::actor_armament> arms(count); std::vector<size_t> order; std::vector<bool> acquire(count); std::vector<int32_t> deadline(count);
      for(size_t i{0}; i != count; ++i) { bool target{false}, get{false}; input >> actors[i].destroyed >> target >> get >> deadline[i]; acquire[i] = get; if(target) arms[i].target = std::make_shared<simulation::target_reference>(); order.push_back(i); }
      if(op == "survivor") {
        auto const choice{simulation::random_task_survivor(order, actors, alive, [&]{ event({1}); return random.next(); })}; output << (choice ? static_cast<int64_t>(*choice) : -1);
      } else if(op == "release") {
        simulation::release_battle_formation(order, arms); output << 0;
      } else {
        simulation::battle_combat_callbacks cb;
        cb.acquire = [&](size_t i){ event({1, static_cast<int64_t>(i)}); if(acquire[i]) arms[i].target = std::make_shared<simulation::target_reference>(); };
        cb.deadline = [&](size_t i){ event({2, static_cast<int64_t>(i)}); return deadline[i]; };
        cb.set_deadline = [&](size_t i, int32_t value){ event({3, static_cast<int64_t>(i), value}); deadline[i] = value; };
        cb.fire = [&](size_t i){ event({4, static_cast<int64_t>(i)}); }; cb.random = [&]{ event({5}); return random.next(); };
        simulation::update_battle_formation(order, actors, arms, now, cb); output << 0;
      }
      output << ' ' << random.value << ' ' << random.global_seed;
      for(size_t i{0}; i != count; ++i) output << ' ' << bool(arms[i].target) << ' ' << deadline[i];
    } else if(op == "acquire") {
      simulation::actor_state actor; simulation::actor_armament arms; simulation::battle_acquisition_context context; uint32_t seed{0}; bool old{false}, candidate{false}; math::vector3 position; int32_t height{0};
      input >> actor.object.id >> actor.conditions >> old >> context.serial >> context.frame >> context.active >> actor.random >> seed >> candidate >> height; read(actor.object.world.position); read(actor.object.world.rotation); read(actor.angles); read(position);
      if(old) arms.target = std::make_shared<simulation::target_reference>(simulation::target_reference{{simulation::target_kind::mobile, 0, 1}, {}, 1});
      simulation::battle_acquisition_callbacks cb;
      cb.select = [&]{ event({1}); return candidate ? std::make_shared<simulation::target_reference>(simulation::target_reference{{simulation::target_kind::mobile, 0, 2}, position, 1}) : nullptr; };
      cb.position = [](auto const &t){ return t.position; };
      cb.height = [&](int32_t x, int32_t y){ event({2, x, y}); return height; };
      cb.fixed = [&](math::vector3 const &p){ event({3, p[0], p[1], p[2]}); return std::make_shared<simulation::target_reference>(simulation::target_reference{{simulation::target_kind::fixed, 0, 3}, p, 1}); };
      simulation::acquire_battle_target(actor, arms, types.at(actor.object.id), weapons, trig, seed, context, cb);
      output << actor.conditions << ' ' << actor.random << ' ' << seed << ' ' << (arms.target ? arms.target->identity.index : 0); print(actor.angles); print(actor.object.world.rotation);
    } else throw std::runtime_error{"Unknown battle command"};
    if(!input) throw std::runtime_error{"Incomplete battle command"};
    output << ' ' << events.size(); print(events); output << '\n';
  }
}
