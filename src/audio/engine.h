#pragma once
#include <map>
#include <mutex>
#include "configuration/settings.h"
#include "effects.h"
#include "simulation/flight_systems.h"
#include "stream.h"

namespace apache::audio {
std::string speech_name(resources::executable_data const &executable, int kind, int flow, int zone, int level, int mission);
class engine {
  resources::data_store const &data;
  resources::executable_data executable;
  std::mutex mutex;
  effects sounds;
  stream transport;
  std::map<std::string, std::shared_ptr<resources::bytes const>, std::less<>> cache;
  stream_source theme;
  bool movie_active{false};
  bool music_enabled{false};
  bool speech_enabled{false};
  bool in_flight{false};
  enum class transition {
    none,
    start,
    restore,
    stop
  };
  transition pending{transition::none};
  stream_source next;
  unsigned int fade{0};
  unsigned int fade_volume{63};
  unsigned int resample_phase{0};
  void change(transition action, stream_source source = {});
  void finish_change();
  stream_source source(std::string const &name, unsigned int channels);
public:
  explicit engine(resources::data_store const &data);
  void configure(configuration::settings const &settings);
  void speech(int kind, int flow, int zone, int level, int mission);
  void stop_speech();
  bool speaking();
  void mission(bool active);
  void pause(bool active);
  void cinematic(bool active);
  void play(unsigned int id, int32_t volume, int32_t pan = 32768);
  void voice(unsigned int id, bool cockpit);
  void gun(bool active);
  void flight(simulation::flight_event event);
  void render(std::span<float> stereo);
};
} // namespace apache::audio
