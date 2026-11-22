#include "inspect_logbook.h"
#include <charconv>
#include <iomanip>
#include <istream>
#include <ostream>
#include <stdexcept>
#include "simulation/pilot_log.h"

void inspect_logbook(apache::resources::data_store const &data, std::istream &input, std::ostream &output) {
  auto const definitions{apache::resources::read_mission_briefings(data.read("MISSDATA/MISSIONS.BLK"))};
  std::string command;
  std::string text;
  while(input >> command >> text) {
    if(text.size() % 2) throw std::runtime_error{"Odd pilot byte string"};
    apache::resources::bytes bytes;
    for(size_t i{0}; i != text.size(); i += 2) {
      unsigned int byte{0};
      auto const [end, error]{std::from_chars(text.data() + i, text.data() + i + 2, byte, 16)};
      if(error != std::errc{} || end != text.data() + i + 2) throw std::runtime_error{"Invalid pilot byte string"};
      bytes.push_back(static_cast<uint8_t>(byte));
    }
    auto pilot{apache::simulation::pilot_log::decode(bytes)};
    if(command == "choose") {
      output << pilot.next_mission(definitions) << '\n';
    } else {
      if(command == "update") {
        pilot.update(definitions);
      } else if(command == "recover") {
        uint32_t now;
        input >> now;
        pilot.recover(now);
      } else if(command == "result") {
        uint32_t id;
        uint32_t result;
        bool campaign;
        input >> id >> result >> campaign;
        pilot.log_result(id, static_cast<apache::simulation::mission_result>(result), campaign);
      } else if(command != "codec") throw std::runtime_error{"Unknown pilot-log operation"};
      for(auto byte : pilot.encode()) output << std::hex << std::setfill('0') << std::setw(2) << static_cast<unsigned int>(byte);
      output << std::dec << std::setfill(' ');
      if(command == "update") {
        for(auto field : {pilot.rank, pilot.completed, pilot.successes, pilot.failures, pilot.outstanding, pilot.medals[0], pilot.medals[1], pilot.medals[2], pilot.campaigns_won, pilot.awards}) output << ' ' << field;
      }
      output << '\n';
    }
    if(!input) throw std::runtime_error{"Incomplete pilot-log operation"};
  }
}
