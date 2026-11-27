#pragma once
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <boost/program_options.hpp>

namespace apache::testing {
struct capture_options {
  std::string directory;
  std::optional<std::filesystem::path> capture;
  bool help{false};
};
inline capture_options read_capture_options(int const argc, char const *const argv[], std::string const &caption, char const *capture_name = "capture-directory", bool const required_capture = false) {
  capture_options result;
  std::string output;
  boost::program_options::options_description options{caption};
  options.add_options()
    ("help", "Show command-line options")
    ("data-directory", boost::program_options::value<std::string>(&result.directory)->required(), "Original game data directory");
  auto *const capture_value{boost::program_options::value<std::string>(&output)};
  if(required_capture) capture_value->required();
  options.add_options()(capture_name, capture_value, "Capture destination");
  boost::program_options::positional_options_description positions;
  positions.add("data-directory", 1).add(capture_name, 1);
  boost::program_options::variables_map arguments;
  boost::program_options::store(boost::program_options::command_line_parser(argc, argv)
    .options(options).positional(positions)
    .style(boost::program_options::command_line_style::default_style & ~boost::program_options::command_line_style::allow_guessing)
    .run(), arguments);
  if(arguments.contains("help")) {
    result.help = true;
    std::cout << options;
    return result;
  }
  boost::program_options::notify(arguments);
  if(arguments.contains(capture_name)) result.capture = output;
  return result;
}
} // namespace apache::testing
