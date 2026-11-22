#pragma once
#include <array>
#include <span>
#include "resources/executable.h"

namespace apache::audio {
struct effect {
  resources::bytes pcm;
  uint32_t rate{0};
  bool repeat{false}, minimal{false};
};
std::array<effect, 189> load_effects(resources::data_store const &data);
int volume_decibels(unsigned int volume);
float volume_gain(unsigned int volume);
struct effect_channel {
  int id{-1};
  bool active{false};
  uint32_t rate{0}, volume{0};
  uint64_t phase{0};
  float gain{0};
};
class effects {
  std::array<effect, 189> bank;
public:
  static unsigned int constexpr output_rate{48000};
  std::array<effect_channel, 16> channels;
  bool enabled{true}, minimal{false}, voices{true}, paused{false};
  unsigned int master{63};
  int radio_channel{-1}, cockpit_channel{-1}, cockpit_effect{-1};
  explicit effects(resources::data_store const &data) : bank{load_effects(data)} {}
  effect const &sample(size_t id) const {
    return bank.at(id);
  }
  int play(unsigned int id, unsigned int volume, int pan = 32768);
  void cockpit(unsigned int id);
  void radio(unsigned int id);
  bool playing(int channel) const;
  void stop(int channel);
  void stop_effect(unsigned int id);
  void stop_all();
  void rotor(unsigned int state);
  void render(std::span<float> stereo);
};
} // namespace apache::audio
