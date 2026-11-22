#include "inspect_projectile_pool.h"
#include <iomanip>
#include <istream>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>
#include "simulation/projectiles.h"

void inspect_projectile_pool(std::istream &input, std::ostream &output, apache::resources::data_store const &data) {
  namespace math = apache::math;
  namespace resources = apache::resources;
  namespace simulation = apache::simulation;
  auto const models{resources::read_models(data.read("3DDATA/3DOBJS.BIN"))};
  math::trigonometry const trig{data};
  simulation::combat_tables const tables{resources::executable_data{data}};
  apache::render::mobile_scene mobiles{models};
  simulation::effects smoke;
  std::optional<resources::terrain> terrain;
  std::vector<resources::scenery_placement> placements;
  std::unique_ptr<simulation::scenery_collisions> scenery;
  std::unique_ptr<simulation::projectiles> pool;
  std::array<simulation::projectile_aim, 256> aims;
  uint32_t seed{1};
  auto const point{[&]{
    math::vector3 v;
    for(auto &x : v) input >> x;
    return v;
  }};
  auto const pose{[&]{
    auto const p{point()};
    uint32_t h{0};
    uint32_t pitch{0};
    uint32_t roll{0};
    input >> h >> pitch >> roll;
    return math::pose{p, trig.transformation(h, pitch, roll)};
  }};
  auto const print{[&](auto const &v){
    for(auto x : v) output << x << ' ';
  }};
  simulation::projectile_callbacks events;
  events.guidance = [&](simulation::projectile_record &round, int32_t){
    for(size_t i{0}; i != pool->entries().size(); ++i) {
      if(pool->entries()[i] && &pool->at(i) == &round) return aims[i];
    }
    return simulation::projectile_aim{};
  };
  events.event = [&](size_t i, simulation::projectile_record &, simulation::projectile_event_info const &event){
    output << "E " << i << ' ' << static_cast<unsigned int>(event.kind) << ' ';
    print(event.position);
    if(event.target) {
      output << static_cast<unsigned int>(event.target->kind) << ' ' << event.target->layer << ' ' << event.target->index;
    } else {
      output << "0 0 0";
    }
    output << '\n';
  };
  std::string command;
  while(input >> command) {
    if(command == "reset") {
      std::string zone;
      uint32_t weapon_random{0};
      input >> zone >> seed >> weapon_random;
      pool.reset();
      mobiles.clear();
      smoke.reset();
      aims = {};
      terrain.emplace(data.read(zone + ".WZ/GRIDPOST.BIN"));
      placements = resources::read_scenery(data.read(zone + ".WZ/GNDOBJS.BIN"));
      resources::initialise_scenery(placements);
      scenery = std::make_unique<simulation::scenery_collisions>(models, trig, placements);
      pool = std::make_unique<simulation::projectiles>(models, trig, tables, *terrain, *scenery, mobiles, smoke, seed, events);
      pool->weapon_random = weapon_random;
    } else if(command == "add") {
      uint32_t model{0};
      uint32_t flags{0};
      input >> model >> flags;
      auto const p{pose()};
      auto const id{mobiles.add(model, p)};
      mobiles.at(id).status = flags;
      apache::render::maintain_model(mobiles.at(id));
    } else if(command == "new") {
      simulation::projectile_request request;
      input >> request.weapon;
      request.launch.pose = pose();
      request.launch.source.position = point();
      request.launch.source.direction = point();
      unsigned int source_kind{0};
      bool selected{false};
      int32_t now{0};
      input >> request.launch.source.speed >> source_kind >> request.source.identity.layer >> request.source.identity.index >> selected >> request.launch.simple_value >> now;
      request.source.identity.kind = static_cast<simulation::target_kind>(source_kind);
      request.source.position = request.launch.source.position;
      if(selected) request.selected = simulation::target_reference{{simulation::target_kind::fixed}, {}};
      auto const index{pool->construct(request)};
      if(index) {
        pool->launch(*index, now);
        aims[*index] = {};
        output << "N " << *index << ' ' << (pool->at(*index).mobile ? static_cast<int64_t>(*pool->at(*index).mobile) : -1) << '\n';
      } else {
        output << "N -1 -1\n";
      }
    } else if(command == "aim") {
      size_t index{0};
      bool exists{false};
      bool actor{false};
      input >> index >> exists >> actor;
      simulation::movement_target target;
      target.position = point();
      target.direction = point();
      input >> target.speed;
      aims.at(index) = {exists ? std::optional{target} : std::nullopt, actor};
    } else if(command == "step") {
      simulation::projectile_time time;
      input >> time.now >> time.delta >> time.seconds >> time.smoke_radius >> time.smoke_lifetime >> time.smoke_density;
      pool->update(time);
      smoke.advance(time.delta, *terrain);
    } else if(command == "remove") {
      size_t index{0};
      input >> index;
      pool->remove(index);
    } else if(command == "state") {
      output << "S " << pool->active_count() << ' ' << seed << ' ' << pool->weapon_random << '\n';
      for(size_t i{0}; i != pool->entries().size(); ++i) {
        if(pool->entries()[i]) {
          auto const &round{*pool->entries()[i]};
          auto const &state{round.state};
          auto const &b{state.body};
          output << "R " << i << ' ' << state.weapon << ' ' << state.launched_at << ' ' << state.random << ' ' << b.motion.flags << ' ';
          if(state.weapon && simulation::weapon_definitions()[state.weapon].movement_class == 3) {
            print(state.impact);
            output << state.impact_at << ' ';
          } else {
            output << b.motion.distance << ' ' << b.motion.speed << ' ';
            print(b.pose.position);
            print(b.pose.rotation);
            print(b.previous);
            print(b.angles);
            output << b.seeker.flags << ' ' << b.seeker.retry_time << ' ' << static_cast<int>(b.seeker.adjusted) << ' ';
            auto const &children{mobiles.at(*round.mobile).children};
            output << (children.empty() || !children[0] ? 0 : children[0]->id) << ' ';
          }
          output << '\n';
        }
      }
      resources::bytes bytes;
      size_t puffs{0};
      auto const word{[&](uint32_t v){
        for(unsigned int j{0}; j != 4; ++j) bytes.push_back(static_cast<uint8_t>(v >> (j * 8)));
      }};
      for(size_t i{0}; i != smoke.smoke_puffs.size(); ++i) {
        auto const &puff{smoke.smoke_puffs[i]};
        if(!puff.active) continue;
        ++puffs;
        word(static_cast<uint32_t>(i));
        word(1);
        for(auto v : puff.position) word(static_cast<uint32_t>(v));
        word(static_cast<uint32_t>(puff.radius));
        word(static_cast<uint32_t>(puff.lifetime));
        word(static_cast<uint32_t>(puff.age));
        word(puff.angle);
      }
      output << "P " << puffs << ' ' << resources::fingerprint(bytes) << '\n';
    } else {
      throw std::runtime_error{"Unknown projectile-pool operation"};
    }
    if(!input) throw std::runtime_error{"Incomplete projectile-pool operation"};
  }
}
