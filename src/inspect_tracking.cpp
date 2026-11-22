#include "inspect_tracking.h"
#include <istream>
#include <ostream>
#include <stdexcept>
#include <string>
#include "simulation/projectile_tracking.h"

void inspect_tracking(std::istream &input, std::ostream &output, apache::resources::data_store const &data) {
  namespace math = apache::math;
  namespace resources = apache::resources;
  namespace simulation = apache::simulation;
  auto models{resources::read_models(data.read("3DDATA/3DOBJS.BIN"))};
  resources::executable_data const executable{data}; resources::initialise_model_metadata(models, executable);
  math::trigonometry const trig{data}; simulation::combat_tables const tables{executable};
  auto const weapons{simulation::weapon_definitions()}; apache::render::mobile_scene mobiles{models};
  std::array<simulation::mobile_target_state, 8> states{};
  for(size_t i{0}; i != states.size(); ++i) mobiles.add(7, {});
  std::array<resources::scenery_placement, 8> placements{};
  simulation::scenery_collisions const scenery{models, trig, placements};
  std::array<std::shared_ptr<simulation::target_reference>, 64> references;
  for(auto &ref : references) ref = std::make_shared<simulation::target_reference>();
  std::array<std::array<int, 3>, 8> links; for(auto &links_for_object : links) links_for_object.fill(-1);
  std::optional<resources::terrain> terrain;
  std::unique_ptr<simulation::target_world> world;
  simulation::projectile_record round; uint32_t seed{1};
  simulation::target_slot_lookup const slots{[&](uint32_t id, simulation::target_slot slot){
    int const index{links.at(id).at(static_cast<size_t>(slot))}; return index < 0 ? std::shared_ptr<simulation::target_reference>{} : references.at(index);
  }};
  auto const point{[&]{ math::vector3 p; for(auto &v : p) input >> v; return p; }};
  auto const identify{[&](simulation::target_reference const *p){
    if(!p) return -1;
    if(round.selected && p == &*round.selected) return 64;
    for(size_t i{0}; i != references.size(); ++i) if(references[i].get() == p) return static_cast<int>(i);
    throw std::runtime_error{"Unknown tracked target"};
  }};
  auto const bound{[&](simulation::projectile_target_binding const &binding){ return binding.selected ? (round.selected ? &*round.selected : nullptr) : binding.target.get(); }};
  std::string command;
  while(input >> command) {
    if(command == "reset") {
      std::string zone; input >> zone; terrain.emplace(data.read(zone + ".WZ/GRIDPOST.BIN"));
      world = std::make_unique<simulation::target_world>(*terrain, scenery, mobiles, models, trig, [&](uint32_t id){
        auto state{states.at(id)}; auto const chaff{slots(id, simulation::target_slot::chaff)}, flare{slots(id, simulation::target_slot::flare)};
        state.chaff_valid = chaff && chaff->identity.kind != simulation::target_kind::none;
        state.flare_valid = flare && flare->identity.kind != simulation::target_kind::none; return state;
      });
    } else if(command == "object") {
      size_t index{0}; uint32_t model{0}, flags{0}, heading{0}, pitch{0}, roll{0}; input >> index >> model >> flags;
      math::pose pose; pose.position = point(); input >> heading >> pitch >> roll; pose.rotation = trig.transformation(heading, pitch, roll);
      auto &s{states.at(index)}; input >> s.type >> s.classification >> s.speed >> s.destroyed >> s.conditions >> s.chaff_at >> s.flare_at >> s.player >> s.player_airspeed >> s.ground_radar >> s.air_radar;
      auto &object{mobiles.at(index)}; object.id = model; object.status = flags; mobiles.move(index, pose);
    } else if(command == "ground") {
      size_t index{0}; input >> index; input >> placements.at(index).model >> placements.at(index).status;
    } else if(command == "reference") {
      size_t index{0}; unsigned int kind{0}; input >> index >> kind; auto &ref{*references.at(index)};
      ref.identity.kind = static_cast<simulation::target_kind>(kind); input >> ref.identity.index; ref.position = point(); input >> ref.classification;
    } else if(command == "slot") {
      size_t owner{0}, slot{0}; input >> owner >> slot; input >> links.at(owner).at(slot);
    } else if(command == "round") {
      int source{0}, selected{0}, designator{0}, adjusted{0}; round = {};
      input >> round.state.weapon >> round.state.body.seeker.flags >> round.state.body.seeker.retry_time >> round.state.random >> seed >> source >> selected >> designator >> adjusted;
      round.state.body.pose.position = point(); round.source = *references.at(source);
      if(selected >= 0) round.selected = *references.at(selected);
      if(designator >= 0) round.designator = *references.at(designator);
      if(adjusted == 64) round.adjusted.selected = true;
      else if(adjusted >= 0) round.adjusted.target = references.at(adjusted);
      round.state.body.seeker.adjusted = adjusted < 0 ? simulation::seeker_target::none : simulation::seeker_target::selected;
    } else if(command == "step") {
      simulation::tracking_environment environment; int level{0}; bool check{false};
      input >> environment.now >> environment.weather >> environment.cloud_bottom >> environment.cloud_top >> environment.mode >> level >> check;
      environment.decoy_modifier = tables.skill(level).decoy_modifier;
      auto const &weapon{weapons.at(round.state.weapon)};
      auto *selected{bound(simulation::selected_projectile_target(round, weapon, *world, slots))};
      output << identify(selected && world->valid(*selected) ? selected : nullptr) << ' ';
      bool const has{check && simulation::projectile_has_target(round, weapon, *world, slots, environment.now)};
      auto const aim{simulation::track_projectile(round, weapon, *world, slots, *terrain, environment, seed)};
      output << identify(bound(round.adjusted)) << ' ' << identify(simulation::adjusted_projectile_target(round, *world, slots)) << ' ' << round.state.body.seeker.flags << ' ' << round.state.body.seeker.retry_time << ' ' << round.state.random << ' ' << seed << ' ' << has << ' ' << aim.target.has_value() << ' ' << aim.actor;
      if(aim.target) { for(auto x : aim.target->position) output << ' ' << x; for(auto x : aim.target->direction) output << ' ' << x; output << ' ' << aim.target->speed << ' ' << aim.target->classification; }
      output << ' ' << (round.selected ? round.selected->classification : 0);
      for(auto const &ref : references) output << ' ' << ref->classification;
      output << '\n';
    } else throw std::runtime_error{"Unknown tracking inspection operation"};
    if(!input) throw std::runtime_error{"Incomplete tracking inspection operation"};
  }
}
