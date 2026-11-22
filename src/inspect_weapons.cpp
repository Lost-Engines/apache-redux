#include "inspect_weapons.h"
#include <bit>
#include <iomanip>
#include <istream>
#include <ostream>
#include <stdexcept>
#include "simulation/weapon_rules.h"

void inspect_weapon_definitions(std::ostream &output) {
  auto const weapons{apache::simulation::weapon_definitions()};
  for(size_t id{0}; id != weapons.size(); ++id) {
    auto const &w{weapons[id]};
    output << id << ' ' << std::quoted(w.name) << ' ' << w.movement_class << ' ' << w.guidance;
    for(auto value : {w.flags, w.alternate_penetration, w.burst_size, w.weapon_role, w.damage, w.decoy_denominator}) output << ' ' << static_cast<unsigned int>(value);
    for(auto value : {w.motor_acceleration, w.motor_ticks, w.burst_pause_ticks, w.guidance_start_distance, w.blast_radius, w.minimum_pitch, w.maximum_pitch, w.heading_limit, w.penetration, w.minimum_range, w.maximum_range, w.minimum_speed, w.maximum_speed, w.launch_speed, w.model, w.target_box_half_size, w.fire_interval_ticks, w.lateral_acceleration, w.tracking_or_dispersion, w.unresolved_5c}) output << ' ' << value;
    output << ' ' << std::bit_cast<uint64_t>(w.drag_coefficient) << '\n';
  }
}
void inspect_weapon_rules(std::istream &input, std::ostream &output, apache::resources::data_store const &data) {
  using apache::simulation::acquisition_delay;
  using apache::simulation::actor_types;
  using apache::simulation::can_fire;
  using apache::simulation::combat_random;
  using apache::simulation::combat_tables;
  using apache::simulation::damage_context;
  using apache::simulation::fire_conditions;
  using apache::simulation::firing_interval;
  using apache::simulation::projectile_damage;
  using apache::simulation::projectile_expired;
  using apache::simulation::projectile_motion;
  using apache::simulation::select_air_weapon;
  using apache::simulation::select_ground_weapon;
  using apache::simulation::update_projectile_speed;
  using apache::simulation::weapon_definitions;
  using apache::simulation::weapon_selection;
  auto const weapons{weapon_definitions()};
  auto const actors{actor_types()};
  combat_tables const tables{apache::resources::executable_data{data}};
  std::string operation;
  while(input >> operation) {
    unsigned int id{0};
    if(operation == "density") {
      int32_t altitude{0};
      input >> altitude;
      output << std::bit_cast<uint64_t>(tables.density(altitude)) << '\n';
    } else if(operation == "skill") {
      int level{0};
      input >> level;
      auto const &skill{tables.skill(level)};
      output << skill.acquire_ticks << ' ' << skill.damage_percent << ' ' << skill.range_percent << ' ' << skill.decoy_interval_percent << ' ' << skill.fire_interval_percent << ' ' << skill.burst_size_percent << ' ' << skill.decoy_modifier << '\n';
    } else if(operation == "speed" || operation == "expiry") {
      projectile_motion round;
      double dt{0};
      input >> id >> round.speed >> round.distance >> round.age >> round.altitude >> round.flags >> dt;
      if(operation == "expiry") {
        output << projectile_expired(round, weapons.at(id)) << '\n';
      } else {
        bool const flame{update_projectile_speed(round, weapons.at(id), dt, tables)};
        output << round.speed << ' ' << round.flags << ' ' << flame << '\n';
      }
    } else if(operation == "damage") {
      damage_context context;
      input >> id >> context.range >> context.armour >> context.radius_override >> context.damage_override >> context.alternate_armour >> context.enemy_source >> context.difficulty_percent >> context.mode >> context.role_random >> context.round_random;
      output << projectile_damage(id, weapons.at(id), context) << '\n';
    } else if(operation == "ground" || operation == "air") {
      weapon_selection state;
      input >> id >> state.flags >> state.task >> state.range >> state.air_minimum >> state.air_maximum;
      for(auto &value : state.ammunition) input >> value;
      input >> state.weapon >> state.slot;
      if(operation == "air") {
        select_air_weapon(state, actors.at(id), weapons);
      } else {
        select_ground_weapon(state, actors.at(id), weapons);
      }
      output << state.weapon << ' ' << state.slot << '\n';
    } else if(operation == "fire") {
      fire_conditions state;
      input >> state.weapon_id >> state.has_target >> state.actor_flags >> state.target_kind >> state.task >> state.ammunition >> state.deadline >> state.now >> state.weather;
      output << can_fire(state, weapons.at(static_cast<size_t>(state.weapon_id))) << '\n';
    } else if(operation == "interval" || operation == "acquire") {
      int32_t ammunition{0};
      int level{0};
      combat_random random;
      input >> id >> ammunition >> level >> random.value >> random.global_seed;
      int32_t const value{operation == "interval" ? firing_interval(weapons.at(id), ammunition, tables.skill(level), random) : acquisition_delay(id == 1, tables.skill(level), tables.skill(9), random)};
      output << value << ' ' << random.value << ' ' << random.global_seed << '\n';
    } else {
      throw std::runtime_error{"Unknown weapon inspection operation"};
    }
    if(!input) throw std::runtime_error{"Incomplete weapon inspection case"};
  }
}
