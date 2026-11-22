#include "inspect_war_grid.h"
#include <istream>
#include <map>
#include <ostream>
#include <stdexcept>
#include <string>
#include "resources/terrain.h"
#include "simulation/war_grid.h"

void inspect_war_grid(std::istream &input, std::ostream &output, apache::resources::data_store const &data) {
  namespace simulation = apache::simulation;
  auto grid{std::make_unique<simulation::war_grid>()}; std::unique_ptr<apache::resources::terrain> terrain;
  std::map<uint32_t, simulation::war_contact> contacts;
  auto const target{[&](uint32_t id){ auto &c{contacts[id]}; if(!c.target) c.target = std::make_shared<simulation::target_reference>(simulation::target_reference{{simulation::target_kind::mobile, 0, id}, {}, 0}); return c.target; }};
  auto const id{[](simulation::shared_target const &t){ return t ? t->identity.index : 0; }};
  std::string op;
  while(input >> op) {
    if(op == "reset") { std::string zone; input >> zone; terrain = std::make_unique<apache::resources::terrain>(data.read(zone + ".WZ/GRIDPOST.BIN")); grid = std::make_unique<simulation::war_grid>(); contacts.clear(); } else if(op == "metadata") { for(uint32_t model{0}; model != 1315; ++model) output << simulation::radar_model(model) << ' ' << simulation::radar_range(model) << ' ' << simulation::radar_floor(model) << ' '; output << '\n'; } else if(op == "height") { int32_t x{0}, y{0}; input >> x >> y; if(!terrain) throw std::runtime_error{"No terrain"}; output << terrain->approximate_elevation(x, y) << '\n'; } else if(op == "radar") {
      uint32_t tid{0}, model{0}, side{0}; int32_t x{0}, y{0}; apache::math::vector3 p; input >> tid >> model >> side >> x >> y; for(auto &v : p) input >> v;
      if(!terrain) throw std::runtime_error{"No terrain"};
      auto t{target(tid)}; t->position = p; grid->insert_radar(x, y, side, t, model, p, [&](int32_t a, int32_t b){ return terrain->approximate_elevation(a, b); });
    } else if(op == "contact") {
      uint32_t tid{0}; input >> tid; auto t{target(tid)}; auto &c{contacts[tid]}; input >> c.flags >> c.classification >> c.task >> c.player; for(auto &v : t->position) input >> v;
    } else if(op == "insert") {
      uint32_t tid{0}; int32_t x{0}, y{0}; input >> tid >> x >> y; grid->insert(x, y, contacts.at(tid));
    } else if(op == "remove") {
      uint32_t tid{0}; int32_t x{0}, y{0}; size_t count{0}; input >> tid >> x >> y >> count;
      if(count > 10000) throw std::runtime_error{"Too many rescan contacts"};
      std::vector<simulation::war_contact> scan; for(size_t i{0}; i != count; ++i) { uint32_t entry{0}; input >> entry; scan.push_back(contacts.at(entry)); }
      auto const &c{contacts.at(tid)}; grid->remove(x, y, c, c.target->position, [&](int32_t, int32_t){ return scan; });
    } else if(op == "query") {
      int32_t x{0}, y{0}; unsigned int side{0}; input >> x >> y >> side;
      output << id(grid->radar(x, y, side)) << ' ' << grid->floor(x, y, side) << ' ' << grid->status(x >> 22, y >> 22, side, [&](auto const &t){ return static_cast<int32_t>(t.identity.index % 3); }) << '\n';
    } else if(op == "snapshot") {
      for(auto v : grid->counts()) output << v << ' '; for(auto v : grid->peak_counts()) output << v << ' ';
      for(int32_t x{0}; x != 64; ++x) for(int32_t y{0}; y != 64; ++y) for(unsigned int side{1}; side != 3; ++side) if(auto const *cell{grid->sector(x, y, side)}) {
        output << x << ' ' << y << ' ' << side << ' ' << id(cell->radar);
        for(auto f : cell->floor) output << ' ' << f;
        for(auto const *bucket : {&cell->air, &cell->ground}) { output << ' ' << static_cast<unsigned int>(bucket->count); for(auto const &t : bucket->targets) output << ' ' << id(t); }
        output << ' ';
      }
      output << '\n';
    } else throw std::runtime_error{"Unknown war-grid command"};
    if(!input) throw std::runtime_error{"Incomplete war-grid input"};
  }
}
