#pragma once
#include <string>

namespace apache {
struct inspection_options {
  std::string directory;
  std::string command;
  std::string zone;
  std::string resource;
  std::string output;
  int mission{0};
  int flight_model{0};
  int seat{-1};
  int game_mode{0};
  unsigned int video{0};
  unsigned int connection{0};
  unsigned int players{0};
  bool help{false};
  bool output_requested{false};
  bool resource_requested{false};
  bool trace{false};
  bool verbose{false};
};
inspection_options read_inspection_options(int argc, char const *const argv[]);
} // namespace apache
