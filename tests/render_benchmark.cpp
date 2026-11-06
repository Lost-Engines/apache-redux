#include "frontend/mission_session.h"
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#if __has_include(<valgrind/callgrind.h>)
#include <valgrind/callgrind.h>
#else
#define CALLGRIND_START_INSTRUMENTATION ((void)0)
#define CALLGRIND_STOP_INSTRUMENTATION ((void)0)
#endif

int main(int argc, char **argv) try {
  if(argc < 5 || argc > 9) throw std::runtime_error{"Usage: render_benchmark DATA WIDTH HEIGHT FRAMES [VIEW=0] [MISSION=-3] [ALTITUDE_FEET=0] [FOV=90]"};
  using namespace apache;
  resources::data_store data{argv[1]};
  auto const width{static_cast<unsigned int>(std::stoul(argv[2]))}, height{static_cast<unsigned int>(std::stoul(argv[3]))};
  unsigned int const frames{static_cast<unsigned int>(std::stoul(argv[4]))}, view{argc > 5 ? static_cast<unsigned int>(std::stoul(argv[5])) : 0u};
  int const mission{argc > 6 ? std::stoi(argv[6]) : -3}, altitude{argc > 7 ? std::stoi(argv[7]) : 0};
  if(!frames || frames > 10000 || altitude < 0 || altitude > 50000 || mission < -4 || !mission || view > 19)
    throw std::runtime_error{"Invalid benchmark frame count, altitude, mission or view"};
  frontend::mission_options options;
  options.graphics.field_of_view = argc > 8 ? configuration::parse_field_of_view(argv[8]) : 90;
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
  std::chrono::duration<double,std::milli> draw{}, colour{};
  std::vector<render::rgba> rgb;
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
  std::cout << "mission=" << mission << " view=" << view << " altitude=" << altitude << " fov=" << options.graphics.field_of_view << ' ' << width << 'x' << height << " frames=" << frames
    << std::fixed << std::setprecision(3) << " draw_ms=" << draw.count()/frames << " rgba_ms=" << colour.count()/frames
    << " pixels=" << std::hex << resources::fingerprint(session->frame.image.pixels)
    << " rgba=" << resources::fingerprint(std::span{reinterpret_cast<uint8_t const *>(rgb.data()), rgb.size() * sizeof(render::rgba)}) << std::dec << '\n';
  return EXIT_SUCCESS;
} catch(std::exception const &e) { std::cerr << e.what() << '\n'; return EXIT_FAILURE; }
