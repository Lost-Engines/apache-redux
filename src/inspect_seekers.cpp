#include "inspect_seekers.h"
#include <istream>
#include <ostream>
#include <stdexcept>
#include <string>
#include <vector>
#include "simulation/seeker.h"

void inspect_seekers(std::istream &input, std::ostream &output, apache::resources::data_store const &data) {
  using apache::simulation::combat_random;
  using apache::simulation::combat_tables;
  using apache::simulation::decoy_distracts;
  using apache::simulation::seeker_inputs;
  using apache::simulation::seeker_queries;
  using apache::simulation::seeker_state;
  using apache::simulation::seeker_target;
  using apache::simulation::update_seeker;
  using apache::simulation::weapon_definitions;
  auto const weapons{weapon_definitions()};
  combat_tables const tables{apache::resources::executable_data{data}};
  std::string command;
  while(input >> command) {
    size_t id{0};
    input >> id;
    if(command == "decoy") {
      int32_t mode{0};
      int32_t level{0};
      bool enemy{false};
      combat_random random;
      input >> mode >> enemy >> level >> random.value >> random.global_seed;
      bool const result{decoy_distracts(weapons.at(id), mode, enemy, tables.skill(level).decoy_modifier, random)};
      output << result << ' ' << random.value << ' ' << random.global_seed << '\n';
    } else if(command == "seeker") {
      seeker_state state;
      seeker_inputs context;
      unsigned int adjusted{0};
      input >> state.flags >> state.retry_time >> adjusted >> context.now >> context.weather >> context.cloud_bottom >> context.cloud_top >> context.target_status >> context.target_classification >> context.source_side;
      state.adjusted = static_cast<seeker_target>(adjusted);
      input >> context.adjusted_valid >> context.has_designator >> context.ground_designated >> context.designator_active >> context.designator_destroyed >> context.chaff >> context.flare >> context.source_radar;
      for(auto *p : {&context.missile_position, &context.target_position, &context.source_position, &context.designator_position}) {
        for(auto &v : *p) input >> v;
      }
      bool missile_visible{false};
      bool source_visible{false};
      bool distracted{false};
      int32_t random{0};
      input >> missile_visible >> source_visible >> distracted >> random;
      std::vector<int> calls;
      seeker_queries const queries{
        .visible{[&](auto const &from, auto const &){
          bool const source{from == context.source_position};
          calls.push_back(source ? 1 : 2);
          return source ? source_visible : missile_visible;
        }},
        .distracted{[&]{
          calls.push_back(3);
          return distracted;
        }},
        .random{[&]{
          calls.push_back(4);
          return random;
        }},
      };
      update_seeker(state, weapons.at(id), context, queries);
      output << state.flags << ' ' << state.retry_time << ' ' << static_cast<int>(state.adjusted) << ' ' << calls.size();
      for(auto v : calls) output << ' ' << v;
      output << '\n';
    } else {
      throw std::runtime_error{"Unknown seeker operation"};
    }
    if(!input) throw std::runtime_error{"Incomplete seeker operation"};
  }
}
