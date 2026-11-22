#include "inspect_combat_update.h"
#include <istream>
#include <ostream>
#include <stdexcept>
#include <string>
#include "simulation/combat_update.h"

void inspect_combat_update(std::istream &input, std::ostream &output, apache::resources::data_store const &data) {
  namespace simulation = apache::simulation;
  apache::math::trigonometry const trig{data};
  auto const weapons{simulation::weapon_definitions()};
  apache::resources::executable_data const executable{data};
  simulation::combat_tables const tables{executable};
  auto const read{[&](auto &v){
    for(auto &x : v) input >> x;
  }};
  auto const id{[](simulation::shared_target const &t){return t ? t->identity.index : 0;}};
  std::string op;
  while(input >> op) {
    std::vector<int64_t> events;
    auto const event{[&](std::initializer_list<int64_t> values){events.insert(events.end(), values);}};
    std::array<simulation::shared_target, 4> refs;
    for(uint32_t i{1}; i != 4; ++i) refs[i] = std::make_shared<simulation::target_reference>(simulation::target_reference{{simulation::target_kind::mobile, 0, i}, {}, 0});
    if(op == "acquire" || op == "fire" || op == "point") {
      simulation::ground_defence object;
      simulation::combat_random random;
      unsigned int weapon{0};
      uint32_t frame{0};
      uint32_t heading{0};
      int32_t now{0};
      int32_t level{0};
      int32_t status{0};
      int32_t side{0};
      int32_t shot{0};
      bool target{false};
      bool candidate{false};
      input >> weapon >> now >> frame >> level >> random.value >> random.global_seed >> target >> candidate >> status >> side >> shot >> object.heading >> object.deadline >> object.shots >> heading;
      read(object.position);
      for(uint32_t i{1}; i != 4; ++i) read(refs[i]->position);
      object.self = refs[1];
      if(target) object.target = refs[2];
      simulation::ground_defence_callbacks cb;
      cb.position = [&](auto const &t){
        event({2, t.identity.index});
        return t.position;
      };
      cb.status = [&](auto const &t){
        event({1, t.identity.index});
        return status;
      };
      cb.side = [&](auto const &t){
        event({3, t.identity.index});
        return side;
      };
      cb.heading = [&](auto const &, uint16_t value){heading = value;};
      cb.search = [&](auto const &p, int32_t s, int32_t lo, int32_t hi){
        event({4, p[0], p[1], p[2], s, lo, hi});
        return candidate ? refs[3] : nullptr;
      };
      cb.shoot = [&](auto const &s, auto const &t, unsigned int w){
        event({5, s.identity.index, t.identity.index, w});
        return static_cast<simulation::firing_status>(shot);
      };
      if(op == "acquire") {
        simulation::acquire_ground_defence_target(object, weapons.at(weapon), now, frame, tables.skill(level), tables.skill(9), random, trig, cb);
      } else if(op == "fire") {
        simulation::fire_ground_defence(object, weapon, weapons.at(weapon), now, tables.skill(level), random, trig, cb);
      } else {
        simulation::point_ground_defence(object, trig, cb);
      }
      output << id(object.target) << ' ' << object.heading << ' ' << object.deadline << ' ' << object.shots << ' ' << heading << ' ' << random.value << ' ' << random.global_seed;
    } else if(op == "sectors") {
      apache::math::vector3 position;
      read(position);
      int seed{0};
      input >> seed;
      simulation::visit_ground_defence_sectors(position, [&](int x, int y){return (x * 7 + y * 11 + seed) % 4;}, [&](int x, int y){event({x, y});});
      output << 0;
    } else if(op == "actor") {
      simulation::actor_state actor;
      simulation::actor_armament arms;
      int32_t task{0};
      int32_t serial{0};
      int32_t status{0};
      int32_t acquisition{0};
      uint32_t frame{0};
      unsigned int weapon{0};
      bool target{false};
      bool behind{false};
      input >> actor.classification >> actor.conditions >> actor.object.status >> actor.destroyed >> actor.speed >> actor.retreat_until >> task >> serial >> frame >> target >> status >> acquisition >> weapon >> behind;
      read(actor.object.world.position);
      read(refs[2]->position);
      read(refs[3]->position);
      if(target) arms.target = refs[2];
      simulation::actor_combat_callbacks cb;
      cb.status = [&](auto const &t){
        event({1, t.identity.index});
        return status;
      };
      cb.position = [&](auto const &t){
        event({6, t.identity.index});
        return t.position;
      };
      auto const acquire{[&](int kind){
        event({kind});
        if(acquisition == 1) {
          arms.target = refs[3];
        } else if(acquisition == 2) arms.target.reset();
      }};
      cb.air_acquire = [&]{acquire(2);};
      cb.ground_acquire = [&]{acquire(3);};
      cb.artillery_acquire = [&]{acquire(4);};
      cb.weapon = [&]{
        event({5});
        return weapon;
      };
      cb.fire = [&]{event({7});};
      cb.behind = [&]{
        event({8});
        return behind;
      };
      cb.released = [&]{event({9});};
      auto const result{simulation::update_actor_combat(actor, arms, task, serial, frame, weapons, cb)};
      output << result << ' ' << actor.conditions << ' ' << actor.retreat_until << ' ' << id(arms.target);
    } else if(op == "formations") {
      size_t count{0};
      int64_t player{0};
      input >> count >> player;
      if(count > 100) throw std::runtime_error{"Too many formations"};
      std::vector<simulation::actor_group> groups;
      std::vector<simulation::actor_state> actors;
      std::vector<apache::resources::mission_task> tasks(count);
      std::vector<int> results;
      for(size_t g{0}; g != count; ++g) {
        size_t members{0};
        input >> tasks[g].type >> members;
        if(members > 100) throw std::runtime_error{"Too many members"};
        simulation::actor_group group;
        group.task = g;
        for(size_t m{0}; m != members; ++m) {
          simulation::actor_state a;
          int result{0};
          input >> a.conditions >> a.network >> result;
          group.units.push_back(actors.size());
          actors.push_back(std::move(a));
          results.push_back(result);
        }
        groups.push_back(std::move(group));
      }
      simulation::update_actor_formations(groups, actors, tasks, player >= 0 ? std::optional<size_t>{static_cast<size_t>(player)} : std::nullopt, [&](size_t i){
        event({static_cast<int64_t>(i)});
        return results.at(i);
      });
      output << 0;
    } else {
      throw std::runtime_error{"Unknown combat update command"};
    }
    if(!input) throw std::runtime_error{"Incomplete combat update input"};
    output << ' ' << events.size();
    for(auto e : events) output << ' ' << e;
    output << '\n';
  }
}
