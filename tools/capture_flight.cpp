#include <algorithm>
#include <array>
#include <bit>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include "audio/engine.h"
#include "configuration/graphics.h"
#include "frontend/mission_session.h"
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif // _WIN32

namespace {
template<typename Range>
void write_array(Range const &values) {
  std::cout << '[';
  bool first{true};
  for(auto const &value : values) {
    if(!first) std::cout << ',';
    std::cout << value;
    first = false;
  }
  std::cout << ']';
}
void snapshot(apache::frontend::mission_session const &session, size_t const bytes, std::span<float const> const audio, std::span<std::array<int32_t, 4> const> const sounds) {
  auto const &flight{*session.runtime.pilot};
  auto const &readings{flight.readings()};
  auto const &inventory{session.runtime.player_armament->inventory};
  size_t rounds{0};
  for(auto const &round : session.runtime.weapons->rounds.entries()) rounds += bool(round);
  size_t destroyed{0};
  for(auto const &actor : session.runtime.actors.units) destroyed += actor.destroyed;
  std::cout << "{\"clock\":" << session.runtime.timing.now
            << ",\"view\":" << session.views->modes.mode << ",\"pose\":";
  std::array<int32_t, 12> pose;
  std::ranges::copy(readings.position.position, pose.begin());
  std::ranges::copy(readings.position.rotation, pose.begin() + 3);
  write_array(pose);
  std::cout << ",\"angles\":";
  write_array(readings.angles);
  std::cout << ",\"camera\":";
  std::ranges::copy(session.camera.camera.position, pose.begin());
  std::ranges::copy(session.camera.camera.rotation, pose.begin() + 3);
  write_array(pose);
  std::cout << ",\"airspeed\":" << readings.airspeed
            << ",\"climb\":" << readings.climb
            << ",\"collective\":" << flight.power().collective
            << ",\"death\":" << flight.death()
            << ",\"faults\":" << flight.faults()
            << ",\"weapon\":" << inventory.current
            << ",\"armed\":" << inventory.armed
            << ",\"locked\":" << session.runtime.sensors->state.locked << ",\"ammo\":";
  write_array(inventory.ammunition);
  std::cout << ",\"target\":" << std::quoted(session.runtime.targets.describe(*session.runtime.player_armament->target).name);
  std::cout << ",\"rounds\":" << rounds << ",\"destroyed\":" << destroyed
            << ",\"bytes\":" << bytes << ",\"audio_frames\":" << audio.size() / 2
            << ",\"audio_bytes\":" << audio.size_bytes() << ",\"audio_rate\":48000,\"audio_events\":[";
  for(size_t i{0}; i != sounds.size(); ++i) {
    if(i) std::cout << ',';
    write_array(sounds[i]);
  }
  std::cout << "]}\n";
}
} // anonymous namespace

// Offline capture tool. Each request is:
// frame|audio|step TICKS HELD_COUNT SCAN... EVENT_COUNT SCAN...
// A response is one JSON line, optional RGBA pixels, then optional stereo f32le PCM.
int main(int argc, char **argv) try {
  bool const capture_audio{argc == 8 && std::string_view{argv[7]} == "--audio"};
  if(argc != 7 && !capture_audio) throw std::runtime_error{
    "Usage: apache_capture DATA_DIRECTORY MISSION WIDTH HEIGHT FOV LOD [--audio]\n"
    "Negative mission IDs select Quickstart: -1 Yemen, -2 Korea, -3 Cyprus.\n"
    "Use frame|audio|step TICKS HELD_COUNT SCAN... EVENT_COUNT SCAN... on stdin."
  };
#ifdef _WIN32
  _setmode(_fileno(stdout), _O_BINARY);
#endif // _WIN32
  std::ios::sync_with_stdio(false);
  std::cin.tie(nullptr);
  std::cout << std::setprecision(17);
  apache::resources::data_store data{argv[1]};
  int const mission{std::stoi(argv[2])};
  auto const resolution{apache::configuration::parse_resolution(std::string{argv[3]} + 'x' + argv[4])};
  if(!resolution) throw std::runtime_error{"An explicit capture resolution is required"};
  apache::frontend::mission_options options;
  options.graphics.field_of_view = apache::configuration::parse_field_of_view(argv[5]);
  options.graphics.lod_distance_multiplier = apache::configuration::parse_lod_distance_multiplier(argv[6]);
  options.video_mode = 1;
  auto owner{mission < 0 ? std::make_unique<apache::frontend::mission_session>(data,
    apache::frontend::mission_session::quick_start(data, -mission), options)
    : std::make_unique<apache::frontend::mission_session>(data, mission, options)};
  auto &session{*owner};
  session.resize(resolution->width, resolution->height);
  std::unique_ptr<apache::audio::engine> sound;
  std::vector<std::array<int32_t, 4>> sound_events;
  if(capture_audio) {
    sound = std::make_unique<apache::audio::engine>(data);
    sound->configure(apache::configuration::settings{data});
    sound->mission(true);
    sound_events.push_back({0, 21, 30, 32768});
    auto const effect{[&](unsigned int id, int32_t volume, int32_t pan){
      sound_events.push_back({0, static_cast<int32_t>(id), volume, pan});
      sound->play(id, volume, pan);
    }};
    session.play_sound = effect;
    session.flight_sound = [&](auto event){
      sound_events.push_back({3, static_cast<int32_t>(event), 0, 0});
      sound->flight(event);
    };
    session.runtime.events.sound = [&](unsigned int id, bool cockpit){
      sound_events.push_back({cockpit ? 1 : 2, static_cast<int32_t>(id), 0, 0});
      sound->voice(id, cockpit);
    };
    session.runtime.events.launch_sound = [effect](unsigned int type){effect(type + 8, 30, 32768);};
    session.runtime.events.incoming_sound = [effect]{effect(14, 30, 32768);};
    session.runtime.events.gun_sound = [&](bool active){
      sound_events.push_back({4, 8, active ? 1 : 0, 0});
      sound->gun(active);
    };
  }
  std::vector<float> samples;
  static_assert(sizeof(float) == 4 && std::endian::native == std::endian::little);
  std::array<bool, 128> held{};
  static_assert(sizeof(apache::render::rgba) == 4);
  std::vector<apache::render::rgba> pixels;
  std::string operation;
  while(std::cin >> operation) {
    if(operation != "frame" && operation != "audio" && operation != "step") throw std::runtime_error{"Unknown capture operation: " + operation};
    unsigned int ticks{0};
    unsigned int count{0};
    std::cin >> ticks >> count;
    if(ticks > 200 || count > held.size()) throw std::runtime_error{"Invalid capture interval or key count"};
    std::array<bool, 128> next{};
    for(unsigned int i{0}; i != count; ++i) {
      unsigned int key{0};
      std::cin >> key;
      if(key >= next.size()) throw std::runtime_error{"Invalid held scan code"};
      next[key] = true;
    }
    for(size_t key{0}; key != held.size(); ++key) {
      if(next[key] != held[key]) session.key(static_cast<uint16_t>(key), next[key], false);
    }
    held = next;
    std::cin >> count;
    if(count > 128) throw std::runtime_error{"Too many capture events"};
    for(unsigned int i{0}; i != count; ++i) {
      unsigned int key{0};
      std::cin >> key;
      if(key > UINT16_MAX) throw std::runtime_error{"Invalid event scan code"};
      session.post(static_cast<uint16_t>(key));
    }
    if(!std::cin) throw std::runtime_error{"Incomplete capture request"};
    if(ticks) session.step(ticks);
    if(operation != "step") session.draw();
    if(sound) {
      sound->pause(session.paused || session.quit_prompt || session.help_requested);
      samples.resize(static_cast<size_t>(ticks) * 240 * 2);
      sound->render(samples);
    }
    if(operation == "frame") {
      session.frame.colour_pixels(pixels);
      snapshot(session, pixels.size() * sizeof(pixels[0]), samples, sound_events);
      std::cout.write(reinterpret_cast<char const *>(pixels.data()), static_cast<std::streamsize>(pixels.size() * sizeof(pixels[0])));
    } else {
      snapshot(session, 0, samples, sound_events);
    }
    if(!samples.empty()) std::cout.write(reinterpret_cast<char const *>(samples.data()), static_cast<std::streamsize>(samples.size() * sizeof(samples[0])));
    sound_events.clear();
    std::cout.flush();
    if(!std::cout) throw std::runtime_error{"Capture output closed"};
  }
  return 0;
} catch(std::exception const &error) {
  std::cerr << error.what() << '\n';
  return 1;
}
