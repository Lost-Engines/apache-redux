#include "inspect_debrief.h"
#include <iomanip>
#include <istream>
#include <ostream>
#include <stdexcept>
#include "simulation/debrief.h"

void inspect_debrief(std::istream &input, std::ostream &output) {
  apache::simulation::debrief_records records;
  std::string command;
  while(input >> command) {
    if(command == "record") {
      apache::simulation::debrief_record record; for(auto &v : record.words) input >> v; records.add(record);
      output << records.entries.size(); for(auto v : records.entries.back().words) output << ' ' << v; output << '\n'; continue;
    }
    if(command == "kill") { uint32_t object, player; input >> object >> player; output << records.player_killed(object, player) << '\n'; continue; }
    if(command == "clear") { records = {}; continue; }
    unsigned int state, distance; uint32_t random; apache::simulation::debrief_result result;
    input >> state >> result.pilot >> result.landed_in_enemy >> distance >> random;
    result.front_distance = static_cast<uint8_t>(distance);
    result.mission = result.targets = static_cast<apache::simulation::mission_result>(state);
    if(command == "survive") { bool enemy, injury; unsigned int distance; input >> enemy >> injury >> distance; result.survive(enemy, injury, distance, random); } else if(command == "landing") { unsigned int grade, death; input >> grade >> death; result.landing(grade, death, random); } else if(command == "position") { int32_t side, altitude, ground; unsigned int distance; input >> side >> altitude >> ground >> distance; result.final_position(side, altitude, ground, distance); } else if(command == "mobiles") { std::array<int32_t, 3> counts, limits; for(auto &v : counts) input >> v; for(auto &v : limits) input >> v; result.mobiles(counts, limits); } else if(command == "required") { unsigned int count, destroyed; input >> count >> destroyed; result.required_ground(count, destroyed); } else if(command == "extra" || command == "prohibited") { bool destroyed; input >> destroyed; if(command == "extra") result.extra_ground(destroyed); else result.prohibited_ground(destroyed); } else if(command == "death") { unsigned int cause, model, surface; int32_t altitude; std::string name; input >> cause >> model >> altitude >> surface >> std::quoted(name); result.death(cause, model, altitude, surface, random, name); } else if(command == "pilot") { std::string name; input >> std::quoted(name); result.pilot_explanation(name); } else throw std::runtime_error{"Unknown debrief operation"};
    if(!input) throw std::runtime_error{"Incomplete debrief operation"};
    for(auto field : {static_cast<uint32_t>(result.mission), static_cast<uint32_t>(result.targets), static_cast<uint32_t>(result.player), result.pilot, static_cast<uint32_t>(result.aborted), static_cast<uint32_t>(result.dead), static_cast<uint32_t>(result.landed_in_enemy), static_cast<uint32_t>(result.front_distance), random}) output << field << ' ';
    output << result.explanations.size();
    for(auto const &line : result.explanations) output << ' ' << std::quoted(line);
    output << '\n';
  }
}
