#include "inspect_options.h"
#include <iostream>
#include <set>
#include <string_view>
#include <vector>
#include <boost/program_options.hpp>

namespace apache {
inspection_options read_inspection_options(int const argc, char const *const argv[]) {
  inspection_options result;
  boost::program_options::options_description header{"Inspection commands"};
  header.add_options()
    ("help", boost::program_options::bool_switch(&result.help), "Show help without loading game data")
    ("data-directory", boost::program_options::value<std::string>(&result.directory), "Original game data directory")
    ("command", boost::program_options::value<std::string>(&result.command), "Inspection command")
    ("arguments", boost::program_options::value<std::vector<std::string>>(), "Command arguments");
  // Consume named command arguments while finding the command. The second
  // pass applies that command's types and required-argument rules.
  for(auto const *name : {"mission", "video-mode", "flight-model", "seat", "game-mode", "connection", "players", "zone", "resource", "output", "trace-mode", "diagnostics"}) {
    header.add_options()(name, boost::program_options::value<std::string>(), "Command argument");
  }
  header.add_options()("trace", "Trace model faces")("verbose", "Print diagnostics");
  boost::program_options::positional_options_description initial_positions;
  initial_positions.add("data-directory", 1).add("command", 1).add("arguments", -1);
  auto const style{boost::program_options::command_line_style::default_style &
    ~boost::program_options::command_line_style::allow_guessing &
    ~boost::program_options::command_line_style::allow_short};
  boost::program_options::variables_map initial;
  boost::program_options::store(boost::program_options::command_line_parser(argc, argv)
    .options(header).positional(initial_positions).style(style).allow_unregistered().run(), initial);
  boost::program_options::notify(initial);

  boost::program_options::options_description options{"Apache inspection options"};
  options.add_options()
    ("help", boost::program_options::bool_switch(&result.help), "Show help without loading game data")
    ("data-directory", boost::program_options::value<std::string>(&result.directory)->required(), "Original game data directory")
    ("command", boost::program_options::value<std::string>(&result.command)->required(), "Inspection command");
  boost::program_options::positional_options_description positions;
  positions.add("data-directory", 1).add("command", 1);
  std::string trace_mode;
  std::set<std::string_view> const mission_commands{
    "mission-sensors", "mission-combat", "mission-weapons", "mission-battle",
    "mission-targets", "mission-spatial", "mission-radar", "mission-scenery",
    "mission-actors", "mission-actor-state", "mission-battles", "ground-trace", "actor-trace"
  };
  std::set<std::string_view> const capture_commands{
    "map-editor", "map-camera", "map-view", "map-routes", "helmet-components", "helmet-weapons",
    "avionics", "map", "mfd-map", "contours", "landscape", "scene", "atmosphere", "scene-effects",
    "scene-mobiles", "scene-craters", "scene-fragments", "scene-rgb", "mfd-flir", "panel-status",
    "mfd-basic", "mfd-weapons", "mfd-radar", "mfd-flight-plan", "instruments", "cockpits",
    "scene-palette", "craters", "shadows", "billboards", "cloud-vertices", "clouds", "celestial",
    "horizon", "feature-frames", "terrain-polygons", "fog-tables", "model-renders",
    "model-renders-alternate", "model-renders-scenery", "circles"
  };
  auto const &command{result.command};
  if(command == "session" || command == "session-shared" || command == "session-network") {
    options.add_options()
      ("mission", boost::program_options::value<int>(&result.mission)->required(), "Mission ID; negative IDs select Quickstart")
      ("video-mode", boost::program_options::value<unsigned int>(&result.video)->required(), "Original video mode, 0 or 1")
      ("flight-model", boost::program_options::value<int>(&result.flight_model)->required(), "Flight model, 0..2");
    positions.add("mission", 1).add("video-mode", 1).add("flight-model", 1);
    if(command == "session-shared") {
      options.add_options()("seat", boost::program_options::value<int>(&result.seat)->required(), "Shared cockpit seat");
      positions.add("seat", 1);
    } else if(command == "session-network") {
      options.add_options()
        ("game-mode", boost::program_options::value<int>(&result.game_mode)->required(), "Network game mode")
        ("connection", boost::program_options::value<unsigned int>(&result.connection)->required(), "Local connection slot")
        ("players", boost::program_options::value<unsigned int>(&result.players)->default_value(2), "Player count");
      positions.add("game-mode", 1).add("connection", 1).add("players", 1);
    }
  } else if(command == "network-group" || command == "network-loopback") {
    options.add_options()("players", boost::program_options::value<unsigned int>(&result.players)->default_value(command == "network-group" ? 3 : 4), "Player count");
    positions.add("players", 1);
  } else if(mission_commands.contains(command)) {
    options.add_options()
      ("zone", boost::program_options::value<std::string>(&result.zone)->required(), "Warzone name")
      ("resource", boost::program_options::value<std::string>(&result.resource)->required(), "Mission resource path");
    positions.add("zone", 1).add("resource", 1);
  } else if(command == "animation" || command == "briefings" || command == "mission-data") {
    options.add_options()("resource", boost::program_options::value<std::string>(&result.resource)->required(), "Original resource path");
    positions.add("resource", 1);
  } else if(command == "images") {
    options.add_options()
      ("resource", boost::program_options::value<std::string>(&result.resource), "Image resource to inspect")
      ("output", boost::program_options::value<std::string>(&result.output), "Write the selected image as PPM");
    positions.add("resource", 1).add("output", 1);
  } else if(command == "terrain-vertices") {
    options.add_options()
      ("verbose", boost::program_options::bool_switch(&result.verbose), "Print diagnostic vertex fields")
      ("diagnostics", boost::program_options::value<std::string>(), "Legacy positional diagnostic marker");
    positions.add("diagnostics", 1);
  } else if(capture_commands.contains(command)) {
    options.add_options()("output", boost::program_options::value<std::string>(&result.output), "Capture filename or output prefix");
    positions.add("output", 1);
    if(command == "model-renders" || command == "model-renders-alternate" || command == "model-renders-scenery") {
      options.add_options()
        ("trace", boost::program_options::bool_switch(&result.trace), "Trace rendered model faces")
        ("trace-mode", boost::program_options::value<std::string>(&trace_mode), "Legacy positional trace marker");
      positions.add("trace-mode", 1);
    }
  }
  boost::program_options::variables_map arguments;
  boost::program_options::store(boost::program_options::command_line_parser(argc, argv)
    .options(options).positional(positions).style(style).run(), arguments);
  if(result.help) {
    std::cout << "Usage: apache_inspect DATA_DIRECTORY COMMAND [arguments]\n"
              << "Use DATA_DIRECTORY COMMAND --help for command-specific options.\n" << options;
    return result;
  }
  boost::program_options::notify(arguments);
  result.resource_requested = arguments.contains("resource");
  result.output_requested = arguments.contains("output");
  result.verbose = result.verbose || arguments.contains("diagnostics");
  result.trace = result.trace || trace_mode == "trace";
  if(command == "images" && result.output_requested && !result.resource_requested) {
    throw boost::program_options::required_option{"resource"};
  }
  return result;
}
} // namespace apache
