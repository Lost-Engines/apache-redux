#include "inspect_damage.h"
#include <iomanip>
#include <istream>
#include <ostream>
#include <stdexcept>
#include <string>
#include "simulation/damage.h"

void inspect_damage(std::istream &input, std::ostream &output, apache::resources::data_store const &data) {
  namespace resources = apache::resources;
  namespace simulation = apache::simulation;
  auto models{resources::read_models(data.read("3DDATA/3DOBJS.BIN"))}; resources::executable_data const executable{data}; resources::initialise_model_metadata(models, executable);
  simulation::combat_tables const tables{executable}; auto const weapons{simulation::weapon_definitions()};
  std::string command;
  while(input >> command) {
    if(command == "hit") {
      simulation::damage_recipient target; simulation::damage_source source; unsigned int damage{0}, kind{0}, weapon{0}; int level{0};
      input >> target.model >> target.flags >> target.mobile >> target.actor >> target.player >> target.network >> damage; target.damage = static_cast<uint8_t>(damage);
      for(auto &x : target.position) input >> x;
      input >> kind >> source.side >> source.actor >> source.network >> weapon; source.kind = static_cast<simulation::target_kind>(kind);
      apache::math::vector3 impact; for(auto &x : impact) input >> x;
      simulation::blast_limits blast; simulation::damage_random random; simulation::damage_environment environment;
      input >> blast.damage >> blast.radius >> random.weapon >> random.round >> random.global >> environment.mode >> environment.flow >> level;
      environment.difficulty_percent = tables.skill(level).damage_percent; environment.decoy_modifier = tables.skill(level).decoy_modifier;
      auto const result{simulation::apply_damage(target, models.at(target.model).value(), source, weapon, weapons.at(weapon), impact, blast, random, environment)};
      output << result.amount << ' ' << target.flags << ' ' << static_cast<unsigned int>(target.damage) << ' ' << blast.damage << ' ' << blast.radius << ' ' << random.weapon << ' ' << random.round << ' ' << random.global << ' ' << result.actions.size();
      for(auto action : result.actions) output << ' ' << static_cast<unsigned int>(action);
    } else if(command == "item" || command == "systems") {
      simulation::player_damage_state state; unsigned int damage{0}, processed{0}, item{0}; int32_t resistance{0}, now{0};
      input >> state.faults >> state.broken_mfds >> state.warning_at >> state.random >> processed >> state.invulnerable >> state.ground_radar >> state.air_radar >> damage >> resistance >> now >> item; state.processed_damage = static_cast<uint8_t>(processed);
      simulation::damage_feedbacks events;
      if(command == "item") simulation::damage_player_item(state, item, events);
      else simulation::damage_player_systems(state, static_cast<uint8_t>(damage), resistance, now, events);
      output << state.faults << ' ' << state.broken_mfds << ' ' << state.warning_at << ' ' << state.random << ' ' << static_cast<unsigned int>(state.processed_damage) << ' ' << events.size();
      for(auto const &event : events) output << ' ' << static_cast<unsigned int>(event.kind) << ' ' << event.value << ' ' << std::quoted(event.text);
    } else throw std::runtime_error{"Unknown damage inspection operation"};
    if(!input) throw std::runtime_error{"Incomplete damage inspection operation"};
    output << '\n';
  }
}
