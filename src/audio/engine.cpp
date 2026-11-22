#include "engine.h"
#include <algorithm>
#include <cstdio>
#include <stdexcept>

namespace apache::audio {
std::string speech_name(resources::executable_data const &executable, int const kind, int const flow, int const zone, int const level, int const mission) {
  if(kind < 1 || kind > 10 || zone < 1 || zone > 4 || level < 0 || level > 9 || mission < 0 || mission > 110) return {};
  std::string name{"E"};
  name += std::array<std::string_view, 11>{"", "CS", "CS", "CM", "CF", "CD", "D", "O", "P", "F", "T"}[static_cast<size_t>(kind)];
  if(kind != 10) name += std::to_string(zone);
  if(kind != 2) {
    char number[16]{};
    if(flow == 4) {
      if(kind != 5) {
        if(level > 0) name += std::to_string(level + (kind == 4)).front();
        if(kind == 3 && mission > 0) {
          resources::reader mapping{executable.at(0x4d9db8 + static_cast<uint32_t>(mission) * 4, 4)};
          std::snprintf(number, sizeof(number), "%u", mapping.u32());
          name += number;
        }
      }
    } else {
      std::snprintf(number, sizeof(number), kind == 10 ? "%d" : "%02d", mission);
      name += number;
    }
  }
  if(name.size() > 8) return {};
  name.resize(8, '_');
  return "SPEECH/" + name + ".RAW";
}
engine::engine(resources::data_store const &data)
  : data{data},
    executable{data},
    sounds{data} {
  if(data.contains("SOUND/THEME.RAW")) {
    theme = source("SOUND/THEME.RAW", 2);
    theme.repeat = true;
    theme.loop = 0x75600;
  }
}
stream_source engine::source(std::string const &name, unsigned int const channels) {
  auto entry{cache.find(name)};
  if(entry == cache.end()) entry = cache.emplace(name, std::make_shared<resources::bytes const>(data.read(name))).first;
  return {
    .pcm{entry->second},
    .name{name},
    .channels{channels}
  };
}
void engine::finish_change() {
  auto const action{pending};
  pending = transition::none;
  fade = 0;
  if(action == transition::start) {
    transport.start(std::move(next));
  } else if(action == transition::restore) {
    transport.restore();
  } else if(action == transition::stop) transport.stop();
  transport.volume = sounds.master;
  resample_phase = 0;
}
void engine::change(transition const action, stream_source value) {
  pending = action;
  next = std::move(value);
  fade = 0;
  fade_volume = transport.volume;
  if(!transport.playing) finish_change();
}
void engine::configure(configuration::settings const &settings) {
  std::lock_guard lock{mutex};
  sounds.master = settings.volume;
  transport.volume = settings.volume;
  if(sounds.enabled && !settings.effects) sounds.stop_all();
  if(!sounds.minimal && settings.minimal_effects) {
    for(size_t i{0}; i != sounds.channels.size(); ++i) {
      if(sounds.channels[i].id >= 0 && !sounds.sample(static_cast<size_t>(sounds.channels[i].id)).minimal) sounds.stop(static_cast<int>(i));
    }
  }
  bool const restart{!sounds.enabled && settings.effects && in_flight};
  sounds.enabled = settings.effects;
  sounds.minimal = settings.minimal_effects;
  if(restart) sounds.rotor(1);
  speech_enabled = settings.speech;
  if(music_enabled != settings.music) {
    music_enabled = settings.music;
    if(music_enabled && !in_flight && theme.pcm) {
      change(transition::start, theme);
    } else if(!music_enabled) {
      transport.forget();
      change(transition::stop);
    }
  }
}
void engine::speech(int const kind, int const flow, int const zone, int const level, int const mission) {
  std::lock_guard lock{mutex};
  if(!speech_enabled || in_flight) return;
  auto const name{speech_name(executable, kind, flow, zone, level, mission)};
  if(name.empty() || !data.contains(name)) return;
  auto value{source(name, 1)};
  if(transport.source.repeat && !transport.stored()) transport.store();
  change(transition::start, std::move(value));
}
void engine::stop_speech() {
  std::lock_guard lock{mutex};
  if(transport.playing && !transport.source.repeat) {
    change(transition::restore);
  } else if(pending == transition::start && !next.repeat) {
    pending = transition::none;
    fade = 0;
    transport.forget();
  }
}
bool engine::speaking() {
  std::lock_guard lock{mutex};
  return (transport.playing && !transport.source.repeat) || (pending == transition::start && !next.repeat);
}
void engine::mission(bool const active) {
  std::lock_guard lock{mutex};
  in_flight = active;
  sounds.paused = false;
  sounds.stop_all();
  if(active) {
    if(music_enabled && !transport.stored()) transport.store();
    change(transition::stop);
    sounds.rotor(1);
  } else if(music_enabled && transport.stored()) {
    change(transition::restore);
  } else if(music_enabled && theme.pcm) change(transition::start, theme);
}
void engine::cinematic(bool const active) {
  std::lock_guard lock{mutex};
  movie_active = active;
}
void engine::pause(bool const active) {
  std::lock_guard lock{mutex};
  sounds.paused = active;
}
void engine::play(unsigned int const id, int32_t const volume, int32_t const pan) {
  std::lock_guard lock{mutex};
  sounds.play(id, static_cast<unsigned int>(std::clamp(volume, 0, 63)), pan);
}
void engine::voice(unsigned int const id, bool const cockpit) {
  std::lock_guard lock{mutex};
  if(cockpit) {
    sounds.cockpit(id);
  } else {
    sounds.radio(id);
  }
}
void engine::gun(bool const active) {
  std::lock_guard lock{mutex};
  if(active) {
    sounds.play(8, 40);
  } else {
    sounds.stop_effect(8);
  }
}
void engine::flight(simulation::flight_event const event) {
  std::lock_guard lock{mutex};
  using enum simulation::flight_event;
  if(event == stop_rotor) {
    sounds.rotor(0);
  } else if(event == powered_rotor) {
    sounds.rotor(1);
  } else if(event == windmilling_rotor) {
    sounds.rotor(2);
  } else if(event == warning_sound || event == pull_up_sound) {
    auto const channel{sounds.play(13, 40)};
    if(channel >= 0 && event == pull_up_sound) sounds.channels[static_cast<size_t>(channel)].rate *= 2;
  }
}
void engine::render(std::span<float> const stereo) {
  if(stereo.size() % 2) throw std::invalid_argument{"Audio requires stereo frames"};
  std::lock_guard lock{mutex};
  std::ranges::fill(stereo, 0);
  if(movie_active) return;
  sounds.render(stereo);
  for(size_t i{0}; i < stereo.size(); i += 2) {
    auto const a{transport.sample()};
    auto const b{transport.sample(1)};
    float const fraction{static_cast<float>(resample_phase) / 48000};
    auto const volume{pending == transition::none ? transport.volume : (63 - fade / 96) * fade_volume / 63};
    float const gain{volume_gain(volume)};
    for(size_t channel{0}; channel != 2; ++channel) stereo[i + channel] = std::clamp(stereo[i + channel] + (a[channel] + (b[channel] - a[channel]) * fraction) * gain, -1.0f, 1.0f);
    resample_phase += 22000;
    if(resample_phase >= 48000) {
      resample_phase -= 48000;
      transport.advance(1);
    }
    if(pending != transition::none && ++fade == 64 * 96) finish_change();
  }
}
} // namespace apache::audio
