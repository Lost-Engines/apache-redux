#include "effects.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace apache::audio {
namespace {
void load_block(std::span<uint8_t const> bytes, bool maximal, std::array<effect, 189> &bank) {
  resources::reader input{bytes, "Sound effect block"};
  uint32_t const declared{maximal ? input.u32() : 0};
  size_t total{0};
  std::array<bool, 189> seen{};
  for(;;) {
    auto const id{input.i32()};
    if(id == -1) break;
    if(id < 0 || static_cast<size_t>(id) >= bank.size() || seen[static_cast<size_t>(id)]) input.fail("Invalid or repeated effect ID");
    auto const size{input.u32()};
    auto pcm{input.take(size)};
    auto &entry{bank[static_cast<size_t>(id)]};
    if(maximal) {
      entry.pcm.assign(pcm.begin(), pcm.end());
    } else if(!entry.minimal || !std::ranges::equal(entry.pcm, pcm)) input.fail("Minimal bank differs from full effects");
    seen[static_cast<size_t>(id)] = true;
    total += size;
  }
  if(input.remaining() || (maximal && total != declared)) input.fail("Invalid sound block length");
  for(size_t i{0}; i != bank.size(); ++i) {
    if((maximal || bank[i].minimal) && !seen[i]) input.fail("Missing effect");
  }
}
} // anonymous namespace
std::array<effect, 189> load_effects(resources::data_store const &data) {
  resources::executable_data const executable{data};
  resources::reader table{executable.at(0x4e9418, 190 * 40), "Effect definitions"};
  std::array<effect, 189> result;
  for(auto &entry : result) {
    if(!table.u32()) table.fail("Missing effect definition");
    table.skip(8);
    entry.rate = table.u32();
    entry.repeat = table.u32() != 0;
    table.skip(16);
    entry.minimal = table.u32() != 0;
    if(!entry.rate || entry.rate > 192000) table.fail("Invalid sample rate");
  }
  if(table.u32()) table.fail("Unterminated effect definitions");
  load_block(data.read("SOUND/MAXIMAL.BLK"), true, result);
  load_block(data.read("SOUND/MINIMAL.BLK"), false, result);
  return result;
}
int volume_decibels(unsigned int const volume) {
  return volume ? std::clamp(1000 - static_cast<int>(64000u / volume), -10000, 0) : -10000;
}
float volume_gain(unsigned int const volume) {
  static auto const gains{[]{
    std::array<float, 128> result;
    for(unsigned int i{0}; i != result.size(); ++i) result[i] = std::pow(10.0f, static_cast<float>(volume_decibels(i)) / 2000.0f);
    return result;
  }()};
  return volume < gains.size() ? gains[volume] : 1.0f;
}
bool effects::playing(int const channel) const {
  return channel >= 0 && static_cast<size_t>(channel) < channels.size() && channels[static_cast<size_t>(channel)].active;
}
void effects::stop(int const channel) {
  if(channel >= 0 && static_cast<size_t>(channel) < channels.size()) channels[static_cast<size_t>(channel)].active = false;
}
void effects::stop_effect(unsigned int const id) {
  for(auto &channel : channels) {
    if(channel.id == static_cast<int>(id)) channel.active = false;
  }
}
void effects::stop_all() {
  for(auto &channel : channels) channel.active = false;
}
int effects::play(unsigned int const id, unsigned int const volume, int const /*pan*/) {
  // The reference Windows backend submits mono effects without panning.
  if(!enabled || id >= bank.size() || (minimal && !bank[id].minimal) || bank[id].pcm.empty()) return -1;
  auto const slot{std::ranges::find(channels, false, &effect_channel::active)};
  if(slot == channels.end()) return -1;
  int const index{static_cast<int>(slot - channels.begin())};
  if(index == radio_channel) radio_channel = -1;
  if(index == cockpit_channel) cockpit_channel = -1;
  unsigned int const level{std::min(master, 63u) * std::min(volume, 63u) / 63};
  *slot = {
    .id{static_cast<int>(id)},
    .active{true},
    .rate{bank[id].rate},
    .volume{level},
    .gain{volume_gain(level)}
  };
  return index;
}
void effects::cockpit(unsigned int const id) {
  if(id == 122) {
    stop(cockpit_channel);
  } else if(playing(cockpit_channel)) {
    if(id == static_cast<unsigned int>(cockpit_effect)) return;
    bool const weapons{cockpit_effect >= 27 && cockpit_effect <= 31 && id >= 27 && id <= 31};
    bool const status{cockpit_effect >= 78 && cockpit_effect <= 80 && id >= 78 && id <= 80};
    if(!weapons && !status) return;
    stop(cockpit_channel);
  }
  if(voices) {
    cockpit_channel = play(id, 35);
    cockpit_effect = static_cast<int>(id);
  }
}
void effects::radio(unsigned int const id) {
  if(voices && !playing(radio_channel)) radio_channel = play(id, 10);
}
void effects::rotor(unsigned int const state) {
  for(unsigned int id{21}; id <= 24; ++id) stop_effect(id);
  if(state >= 1 && state <= 4) play(state + 20, 30);
}
void effects::render(std::span<float> const stereo) {
  if(stereo.size() % 2) throw std::invalid_argument{"Audio requires interleaved stereo frames"};
  if(paused) return;
  for(auto &channel : channels) {
    if(!channel.active) continue;
    auto const &sample{bank[static_cast<size_t>(channel.id)]};
    uint64_t const end{sample.pcm.size() * output_rate};
    for(size_t i{0}; i < stereo.size(); i += 2) {
      auto const index{static_cast<size_t>(channel.phase / output_rate)};
      auto const next{index + 1 == sample.pcm.size() ? (sample.repeat ? 0 : index) : index + 1};
      float const fraction{static_cast<float>(channel.phase % output_rate) / output_rate};
      float const a{static_cast<float>(sample.pcm[index]) - 128};
      float const b{static_cast<float>(sample.pcm[next]) - 128};
      float const value{(a + (b - a) * fraction) * (channel.gain / 128)};
      stereo[i] += value;
      stereo[i + 1] += value;
      channel.phase += channel.rate;
      if(channel.phase >= end) {
        if(sample.repeat) {
          channel.phase %= end;
        } else {
          channel.active = false;
          break;
        }
      }
    }
  }
}
} // namespace apache::audio
