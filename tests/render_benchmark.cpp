#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <boost/program_options.hpp>
#include "frontend/mission_session.h"
#if __has_include(<valgrind/callgrind.h>)
#include <valgrind/callgrind.h>
#else
#define CALLGRIND_START_INSTRUMENTATION ((void)0)
#define CALLGRIND_STOP_INSTRUMENTATION ((void)0)
#endif

int main(int argc, char **argv) try {
  namespace frontend = apache::frontend;
  namespace resources = apache::resources;
  std::string directory, fov, lod;
  unsigned int width{0}, height{0}, frames{0}, view{0};
  int mission{-3}, altitude{0};
  boost::program_options::options_description arguments_description{"Software rendering benchmark"};
  arguments_description.add_options()
    ("help", "Show command-line options")
    ("data-directory", boost::program_options::value<std::string>(&directory)->required(), "Original game data directory")
    ("width", boost::program_options::value<unsigned int>(&width)->required(), "Render width")
    ("height", boost::program_options::value<unsigned int>(&height)->required(), "Render height")
    ("frames", boost::program_options::value<unsigned int>(&frames)->required(), "Number of measured frames")
    ("view", boost::program_options::value<unsigned int>(&view)->default_value(0), "View mode")
    ("mission", boost::program_options::value<int>(&mission)->default_value(-3), "Mission ID; negative IDs select Quickstart")
    ("altitude", boost::program_options::value<int>(&altitude)->default_value(0), "Altitude above terrain in feet")
    ("fov", boost::program_options::value<std::string>(&fov)->default_value("90"), "Horizontal field of view in degrees")
    ("lod-distance", boost::program_options::value<std::string>(&lod)->default_value("6"), "Model LOD distance multiplier");
  boost::program_options::positional_options_description positions;
  positions.add("data-directory", 1).add("width", 1).add("height", 1).add("frames", 1)
    .add("view", 1).add("mission", 1).add("altitude", 1).add("fov", 1).add("lod-distance", 1);
  boost::program_options::variables_map arguments;
  boost::program_options::store(boost::program_options::command_line_parser(argc, argv)
    .options(arguments_description).positional(positions)
    .style(boost::program_options::command_line_style::default_style &
      ~boost::program_options::command_line_style::allow_guessing &
      ~boost::program_options::command_line_style::allow_short)
    .run(), arguments);
  if(arguments.contains("help")) {
    std::cout << arguments_description;
    return EXIT_SUCCESS;
  }
  boost::program_options::notify(arguments);
  resources::data_store data{directory};
  if(!frames || frames > 10000 || altitude < 0 || altitude > 50000 || mission < -4 || !mission || view > 19) {
    throw std::runtime_error{"Invalid benchmark frame count, altitude, mission or view"};
  }
  frontend::mission_options options;
  options.graphics.field_of_view = apache::configuration::parse_field_of_view(fov);
  options.graphics.lod_distance_multiplier = apache::configuration::parse_lod_distance_multiplier(lod);
  auto session{mission < 0
    ? std::make_unique<frontend::mission_session>(data, frontend::mission_session::quick_start(data, -mission), options)
    : std::make_unique<frontend::mission_session>(data, mission, options)};
  session->step(10);
  session->paused = true;
  session->select_view(view);
  session->resize(width, height);
  if(altitude) {
    auto position{session->runtime.pilot->readings().position.position};
    position[2] = session->runtime.terrain.elevation(position[0], position[1]) + altitude * 256;
    session->runtime.pilot->reposition(position);
    session->runtime.actors.synchronize_models();
  }
  for(unsigned int i{0}; i < 3; ++i) session->draw();
  using clock = std::chrono::steady_clock;
  std::chrono::duration<double, std::milli> draw{};
  std::chrono::duration<double, std::milli> colour{};
  std::vector<apache::render::rgba> rgb;
  session->frame.colour_pixels(rgb);
  CALLGRIND_START_INSTRUMENTATION;
  for(unsigned int i{0}; i < frames; ++i) {
    auto const a{clock::now()};
    session->draw();
    auto const b{clock::now()};
    session->frame.colour_pixels(rgb);
    auto const c{clock::now()};
    draw += b - a;
    colour += c - b;
  }
  CALLGRIND_STOP_INSTRUMENTATION;
  std::cout << "mission=" << mission << " view=" << view << " altitude=" << altitude << " fov=" << options.graphics.field_of_view << " lod=" << options.graphics.lod_distance_multiplier << ' ' << width << 'x' << height << " frames=" << frames
    << std::fixed << std::setprecision(3) << " draw_ms=" << draw.count() / frames << " rgba_ms=" << colour.count() / frames
    << " pixels=" << std::hex << resources::fingerprint(session->frame.image.pixels)
    << " rgba=" << resources::fingerprint(std::span{reinterpret_cast<uint8_t const*>(rgb.data()), rgb.size() * sizeof(apache::render::rgba)}) << std::dec << '\n';
  return EXIT_SUCCESS;
} catch(std::exception const &e) {
  std::cerr << e.what() << '\n';
  return EXIT_FAILURE;
}
