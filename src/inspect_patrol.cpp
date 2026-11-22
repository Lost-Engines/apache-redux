#include "inspect_patrol.h"
#include <istream>
#include <map>
#include <ostream>
#include <stdexcept>
#include <string>
#include "simulation/combat_air_patrol.h"

void inspect_patrol(std::istream &input, std::ostream &output, apache::resources::data_store const &data) {
  namespace math = apache::math;
  namespace simulation = apache::simulation;
  math::trigonometry const trig{data}; auto const weapons{simulation::weapon_definitions()};
  auto const read{[&](auto &v){ for(auto &x : v) input >> x; }};
  auto const print{[&](auto const &v){ for(auto x : v) output << ' ' << x; }};
  auto const target{[](uint32_t id, math::vector3 const &p, simulation::target_kind kind = simulation::target_kind::mobile){ return std::make_shared<simulation::target_reference>(simulation::target_reference{{kind, 0, id}, p, 2}); }};
  auto const id{[](simulation::shared_target const &t){ return t ? t->identity.index : 0; }};
  std::string op;
  while(input >> op) {
    std::vector<int64_t> events; auto const event{[&](std::initializer_list<int64_t> e){ events.insert(events.end(), e); }};
    if(op == "scan") {
      simulation::patrol_contacts contacts; int32_t now{0}; size_t count{0}; input >> now >> count;
      struct cell { std::array<bool, 2> present; int32_t owner; std::array<int32_t, 2> status, floor; std::array<std::vector<simulation::shared_target>, 2> air; };
      std::map<std::array<int32_t, 2>, cell> cells;
      for(size_t i{0}; i != count; ++i) {
        int32_t x{0}, y{0}; cell c; input >> x >> y >> c.present[0] >> c.present[1] >> c.owner; read(c.status); read(c.floor);
        for(auto &list : c.air) { size_t n{0}; input >> n; if(n > 8) throw std::runtime_error{"Too many sector contacts"}; for(size_t j{0}; j != n; ++j) { uint32_t tid{0}; math::vector3 p; input >> tid; read(p); list.push_back(target(tid, p)); } }
        cells[{x, y}] = std::move(c);
      }
      simulation::patrol_radar_callbacks radar; radar.position = [](auto const &t){ return t.position; };
      radar.floor = [&](math::vector3 const &p, unsigned int side){ event({2, side, p[0], p[1]}); return cells.at({p[0] >> 22, p[1] >> 22}).floor.at(side - 1); };
      simulation::scan_patrol_contacts(contacts, now, [&](int32_t x, int32_t y){ simulation::patrol_sector s; auto it{cells.find({x, y})}; if(it != cells.end()) { s.present = it->second.present; s.owner = it->second.owner; for(size_t i{0}; i != 2; ++i) s.air[i] = it->second.air[i]; } return s; }, [&](int32_t x, int32_t y, unsigned int side){ event({1, side, x, y}); return cells.at({x, y}).status.at(side - 1); }, radar);
      output << contacts.last_scan;
      for(auto const &list : contacts.visible) { output << ' ' << list.size(); for(auto const &t : list) output << ' ' << id(t); }
    } else if(op == "select") {
      simulation::patrol_contacts contacts; simulation::actor_state actor; simulation::flight_path path; int32_t now{0}, attention{0}; unsigned int slot{0}; size_t count{0};
      input >> actor.object.status >> actor.speed >> attention >> slot >> now >> contacts.last_scan; read(actor.object.world.position); int mask{0}; input >> mask; std::array<math::vector3, 3> points; for(auto &p : points) read(p); if(mask & 1) path.previous = points[0]; if(mask & 2) path.current = points[1]; if(mask & 4) path.next = points[2];
      unsigned int const side{(actor.object.status & 6) == 2 ? 0u : 1u}; contacts.assigned.at(side).at(slot) = target(100, {});
      input >> count; if(count > 40) throw std::runtime_error{"Too many patrol contacts"};
      for(size_t i{0}; i != count; ++i) { math::vector3 p; read(p); contacts.visible[side].push_back(target(static_cast<uint32_t>(i + 1), p)); }
      auto const t{simulation::select_patrol_target(contacts, actor, attention, path, slot, now, trig, [&]{ event({1}); contacts.last_scan = now; }, [](auto const &p){ return p.position; })};
      output << id(t) << ' ' << id(contacts.assigned.at(side).at(slot)) << ' ' << contacts.last_scan;
    } else if(op == "acquire") {
      simulation::actor_state actor; simulation::actor_armament arms; int32_t now{0}, floor{0}, radar_status{0}; unsigned int weapon{0}; bool has_target{false}, basic{false}, selection{false}, radar_present{false};
      input >> actor.conditions >> actor.object.status >> actor.classification >> actor.speed >> actor.retreat_until >> now >> has_target >> basic >> selection >> weapon >> floor >> radar_present >> radar_status; read(actor.object.world.position);
      std::array<simulation::shared_target, 7> refs; std::array<int32_t, 7> classes{};
      for(uint32_t i{1}; i != 6; ++i) { unsigned int kind{0}; math::vector3 p; input >> kind >> classes[i]; read(p); refs[i] = target(i, p, static_cast<simulation::target_kind>(kind)); }
      if(has_target) arms.target = refs[1];
      simulation::patrol_acquisition_callbacks cb; cb.radar.position = [](auto const &t){ return t.position; };
      cb.basic = [&]{ event({1}); if(basic) arms.target = refs[2]; };
      cb.weapon_range = [&]{ event({2}); return weapons.at(weapon).maximum_range; };
      cb.radar.floor = [&](math::vector3 const &p, unsigned int side){ event({3, side, p[0], p[1]}); return floor; };
      cb.radar.radar = [&](math::vector3 const &p, unsigned int side){ event({4, side, p[0], p[1]}); return radar_present ? refs[5] : nullptr; };
      cb.radar.status = [&](auto const &t){ event({5, t.identity.index}); return radar_status; };
      cb.fixed = [&](math::vector3 const &p){ event({6, p[0], p[1], p[2]}); refs[6] = target(6, p, simulation::target_kind::fixed); return refs[6]; };
      cb.released = [&]{ event({7}); }; cb.select = [&]{ event({8, (actor.object.status & 6) == 2 ? 1 : 2}); return selection ? refs[3] : nullptr; }; cb.actor_class = [&](auto const &t){ return classes.at(t.identity.index); };
      simulation::acquire_patrol_target(actor, arms, refs[4], now, cb);
      output << actor.conditions << ' ' << actor.retreat_until << ' ' << id(arms.target);
    } else throw std::runtime_error{"Unknown patrol command"};
    if(!input) throw std::runtime_error{"Incomplete patrol command"};
    output << ' ' << events.size(); print(events); output << '\n';
  }
}
