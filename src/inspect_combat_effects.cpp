#include "inspect_combat_effects.h"
#include <istream>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>
#include "resources/features.h"
#include "simulation/combat_effects.h"

void inspect_combat_effects(std::istream &input, std::ostream &output, apache::resources::data_store const &data) {
  namespace math = apache::math;
  namespace resources = apache::resources;
  namespace simulation = apache::simulation;
  auto models{resources::read_models(data.read("3DDATA/3DOBJS.BIN"))}; resources::executable_data const executable{data}; resources::initialise_model_metadata(models, executable);
  math::trigonometry const trig{data}; simulation::effects effects; simulation::craters craters; simulation::combat_random random;
  std::optional<resources::terrain> terrain; std::optional<resources::feature_layer> normal, super;
  std::unique_ptr<simulation::combat_effects> combat;
  std::vector<simulation::explosion_audio> sounds;
  auto const point{[&]{ math::vector3 p; for(auto &v : p) input >> v; return p; }};
  auto const state{[&](int32_t model){
    resources::bytes raw; unsigned int count{0};
    auto const word{[&](uint32_t value){ for(unsigned int i{0}; i != 4; ++i) { raw.push_back(static_cast<uint8_t>(value)); value >>= 8; } }};
    auto const vector{[&](math::vector3 const &p){ for(auto v : p) word(static_cast<uint32_t>(v)); }};
    for(size_t i{0}; i != effects.explosions.size(); ++i) {
      auto const &e{effects.explosions[i]}; if(!e.kind) continue;
      ++count; word(static_cast<uint32_t>(i)); word(e.kind); vector(e.position); vector(e.velocity); word(static_cast<uint32_t>(e.radius)); word(static_cast<uint32_t>(e.frame));
    }
    for(size_t i{0}; i != 256; ++i) {
      auto const &c{i < 128 ? craters.short_lived[i] : craters.long_lived[i - 128]}; if(c.model < 0) continue;
      ++count; word(static_cast<uint32_t>(i + 128)); vector(c.position); word(static_cast<uint32_t>(c.model)); word(c.heading); word(static_cast<uint32_t>(c.age)); vector(c.scale);
    }
    output << random.value << ' ' << random.global_seed << ' ' << model << ' ' << count << ' ' << resources::fingerprint(raw) << ' ' << sounds.size();
    for(auto const &sound : sounds) output << ' ' << sound.volume << ' ' << sound.pan;
    output << '\n';
  }};
  std::string command;
  while(input >> command) {
    if(command == "reset") {
      std::string zone; input >> zone >> random.value >> random.global_seed;
      terrain.emplace(data.read(zone + ".WZ/GRIDPOST.BIN")); normal.emplace(data, zone, false); super.emplace(data, zone, true); effects = {}; craters = {}; effects.reset(); craters.reset();
      combat = std::make_unique<simulation::combat_effects>(models, *terrain, trig, effects, craters, random, [&](int32_t x, int32_t y){ return resources::surface_type(*normal, *super, x, y); });
      combat->play_sound = [&](simulation::explosion_audio const &sound){ sounds.push_back(sound); };
    } else {
      if(!combat) throw std::runtime_error{"Reset the combat effects first"};
      sounds.clear();
      if(command == "camera") { combat->listener = point(); input >> combat->listener_heading; } else if(command == "point" || command == "objectpoint") {
        int32_t radius{0}; uint32_t model{0}, heading{0};
        if(command == "point") input >> radius; else input >> model >> heading;
        auto const p{point()}; auto const result{command == "point" ? combat->random_point(p, radius) : combat->random_point_in_model(p, model, heading)};
        output << random.value << ' ' << random.global_seed; for(auto v : result) output << ' ' << v; output << '\n';
      } else if(command == "sound") {
        int32_t radius{0}; input >> radius; auto const result{combat->audio(point(), radius)}; output << result.volume << ' ' << result.pan << '\n';
      } else {
        int32_t result{-1};
        if(command == "advance") { int32_t delta{0}; input >> delta; effects.advance(delta, *terrain); craters.advance(); } else if(command == "crater" || command == "cluster") {
          int32_t radius{0}; uint32_t model{0}, heading{0}; input >> radius;
          if(command == "cluster") input >> model;
          input >> heading; auto const p{point()};
          if(command == "crater") combat->crater(p, radius, heading); else combat->cluster(p, radius, model, heading);
        } else if(command == "ground") {
          resources::scenery_placement object; bool partial{false}; input >> object.model >> object.heading >> partial;
          auto const p{point()}; combat->ground(object, p, partial); result = object.model;
        } else if(command == "mobile") {
          uint32_t model{0}, heading{0}, pitch{0}, roll{0}; bool partial{false}; int32_t speed{0}; input >> model >> partial >> speed >> heading >> pitch >> roll;
          auto const p{point()}; combat->mobile(model, {p, trig.transformation(heading, pitch, roll)}, partial);
        } else if(command == "weapon") {
          unsigned int id{0}; bool expired{false}, skip{false}, override{false}; uint32_t heading{0}, pitch{0}, roll{0}; input >> id >> expired >> skip >> override >> heading >> pitch >> roll;
          auto const p{point()}, hit{point()}; combat->weapon(id, {p, trig.transformation(heading, pitch, roll)}, expired, skip, override ? std::optional{hit} : std::nullopt);
        } else throw std::runtime_error{"Unknown combat effect operation"};
        state(result);
      }
    }
    if(!input) throw std::runtime_error{"Incomplete combat effect operation"};
  }
}
