#include "inspect_target_search.h"
#include <istream>
#include <map>
#include <ostream>
#include <stdexcept>
#include <string>
#include "simulation/target_search.h"

void inspect_target_search(std::istream &input, std::ostream &output) {
  namespace math = apache::math;
  namespace simulation = apache::simulation;
  using simulation::shared_target;
  std::string command;
  while(input >> command) {
    if(command == "ring") {
      math::vector3 origin;
      int32_t side{0};
      int32_t minimum{0};
      int32_t maximum{0};
      int32_t sample{0};
      size_t count{0};
      for(auto &v : origin) input >> v;
      input >> side >> minimum >> maximum >> sample >> count;
      std::map<std::array<int32_t, 2>, shared_target> cells;
      for(size_t i{0}; i != count; ++i) {
        int32_t x{0};
        int32_t y{0};
        uint32_t id{0};
        input >> x >> y >> id;
        cells[{x, y}] = std::make_shared<simulation::target_reference>(simulation::target_reference{{simulation::target_kind::fixed, 0, id}, {}, 0});
      }
      std::vector<int32_t> queries;
      size_t draws{0};
      auto const result{simulation::search_sector_rings(origin, side, minimum, maximum, [&](int32_t x, int32_t y, int32_t faction){
        queries.insert(queries.end(), {x, y, faction});
        auto const found{cells.find({x, y})};
        return found == cells.end() ? shared_target{} : found->second;
      }, [&]{
        ++draws;
        return sample;
      })};
      output << (result ? result->identity.index : 0) << ' ' << draws << ' ' << queries.size();
      for(auto v : queries) output << ' ' << v;
      output << '\n';
    } else if(command == "bucket") {
      unsigned int count{0};
      bool remove{false};
      bool priority{false};
      uint32_t id{0};
      input >> count >> remove >> priority >> id;
      simulation::contact_bucket bucket;
      bucket.count = static_cast<uint8_t>(count);
      std::map<uint32_t, shared_target> refs;
      auto const reference{[&](uint32_t number){
        if(!number) return shared_target{};
        if(!refs.contains(number)) refs[number] = std::make_shared<simulation::target_reference>(simulation::target_reference{{simulation::target_kind::mobile, 0, number}, {}, 0});
        return refs[number];
      }};
      for(auto &target : bucket.targets) {
        uint32_t number{0};
        input >> number;
        target = reference(number);
      }
      bool rescan{false};
      if(remove) {
        rescan = bucket.remove(reference(id));
      } else {
        bucket.insert(reference(id), priority);
      }
      output << static_cast<unsigned int>(bucket.count) << ' ' << rescan;
      for(auto const &target : bucket.targets) output << ' ' << (target ? target->identity.index : 0);
      output << '\n';
    } else if(command == "sector") {
      unsigned int mode{0};
      unsigned int air_count{0};
      unsigned int ground_count{0};
      bool randomised{false};
      bool present{false};
      int32_t side{0};
      int32_t sample{0};
      simulation::sector_search_inputs sector;
      input >> mode >> randomised >> side >> present >> air_count >> ground_count >> sector.scenery_side >> sample;
      math::vector3 origin;
      for(auto &v : origin) input >> v;
      std::array<shared_target, 21> refs{};
      std::array<math::vector3, 21> positions{};
      std::array<int32_t, 21> statuses{};
      std::array<int32_t, 21> tasks{};
      for(size_t i{1}; i != refs.size(); ++i) {
        unsigned int kind{0};
        input >> kind >> statuses[i] >> tasks[i];
        for(auto &v : positions[i]) input >> v;
        refs[i] = std::make_shared<simulation::target_reference>(simulation::target_reference{{static_cast<simulation::target_kind>(kind), 0, static_cast<uint32_t>(i)}, positions[i], 0});
      }
      simulation::contact_bucket air;
      simulation::contact_bucket ground;
      air.count = static_cast<uint8_t>(present ? air_count : 0);
      ground.count = static_cast<uint8_t>(present ? ground_count : 0);
      auto const read{[&](auto &targets){
        for(auto &target : targets) {
          size_t id{0};
          input >> id;
          target = refs.at(id);
        }
      }};
      read(air.targets);
      read(ground.targets);
      read(sector.guns);
      read(sector.missiles);
      sector.air = air.candidates();
      sector.ground = ground.candidates();
      size_t draws{0};
      simulation::target_search_callbacks callbacks;
      callbacks.position = [&](auto const &target){return positions.at(target.identity.index);};
      callbacks.status = [&](auto const &target){return statuses.at(target.identity.index);};
      callbacks.mobile_air_defence = [&](auto const &target){return tasks.at(target.identity.index) == 19;};
      callbacks.random = [&]{
        ++draws;
        return sample;
      };
      auto const result{simulation::choose_sector_target(sector, origin, side, static_cast<simulation::search_kind>(mode), randomised, callbacks)};
      output << (result ? result->identity.index : 0) << ' ' << draws << '\n';
    } else {
      throw std::runtime_error{"Unknown target-search operation"};
    }
    if(!input) throw std::runtime_error{"Incomplete target-search operation"};
  }
}
