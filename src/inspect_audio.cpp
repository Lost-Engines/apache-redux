#include "inspect_audio.h"
#include <bit>
#include <fstream>
#include <iomanip>
#include <istream>
#include <ostream>
#include <stdexcept>
#include "audio/effects.h"
#include "audio/engine.h"
#include "audio/stream.h"

void inspect_audio(apache::resources::data_store const &data, std::istream &input, std::ostream &output) {
  apache::audio::effects effects{data}; apache::audio::stream stream;
  apache::resources::executable_data executable{data};
  std::unique_ptr<apache::audio::engine> engine;
  apache::configuration::settings settings{data};
  apache::resources::bytes captured;
  stream.started = [&](auto const &source, uint64_t clock){ output << "start " << clock << ' ' << std::quoted(source.name) << ' ' << source.channels << '\n'; };
  std::string command;
  while(input >> command) {
    if(command.starts_with("engine")) {
      if(!engine) engine = std::make_unique<apache::audio::engine>(data);
      if(command == "engineconfig") { input >> settings.volume >> settings.music >> settings.speech >> settings.effects >> settings.minimal_effects; engine->configure(settings); } else if(command == "enginespeech") { int kind, flow, zone, level, mission; input >> kind >> flow >> zone >> level >> mission; engine->speech(kind, flow, zone, level, mission); } else if(command == "enginestop") engine->stop_speech();
      else if(command == "enginemission") { bool active; input >> active; engine->mission(active); } else if(command == "enginemovie") { bool active; input >> active; engine->cinematic(active); } else if(command == "enginepause") { bool active; input >> active; engine->pause(active); } else if(command == "enginegun") { bool active; input >> active; engine->gun(active); } else if(command == "engineplay") { unsigned int id; int volume; input >> id >> volume; engine->play(id, volume); } else if(command == "enginevoice") { unsigned int id; bool cockpit; input >> id >> cockpit; engine->voice(id, cockpit); } else if(command == "enginemix") {
        size_t frames, chunk; std::string path; input >> frames >> chunk >> std::quoted(path);
        if(frames > 48000 * 600 || chunk == 0) throw std::invalid_argument{"Invalid audio capture size"};
        std::vector<float> pcm(frames * 2);
        for(size_t offset{0}; offset < frames;) { auto const count{std::min(chunk, frames - offset)}; engine->render(std::span{pcm}.subspan(offset * 2, count * 2)); offset += count; }
        if(path != "-") { std::ofstream file{path, std::ios::binary}; file.write(reinterpret_cast<char const*>(pcm.data()), static_cast<std::streamsize>(pcm.size() * sizeof(float))); }
        output << "mixed " << apache::resources::fingerprint({reinterpret_cast<uint8_t const*>(pcm.data()), pcm.size() * sizeof(float)}) << '\n';
      } else throw std::runtime_error{"Invalid audio engine command"};
    } else if(command == "speechname") { int kind, flow, zone, level, mission; input >> kind >> flow >> zone >> level >> mission; output << apache::audio::speech_name(executable, kind, flow, zone, level, mission) << '\n'; } else if(command == "settings") input >> effects.master >> effects.enabled >> effects.minimal >> effects.voices;
    else if(command == "owners") input >> effects.radio_channel >> effects.cockpit_channel >> effects.cockpit_effect;
    else if(command == "effect") { unsigned int id, volume; int pan; input >> id >> volume >> pan; output << "slot " << effects.play(id, volume, pan) << '\n'; } else if(command == "cockpit") { unsigned int id; input >> id; effects.cockpit(id); } else if(command == "radio") { unsigned int id; input >> id; effects.radio(id); } else if(command == "rotor") { unsigned int id; input >> id; effects.rotor(id); } else if(command == "stop") { int channel; input >> channel; effects.stop(channel); } else if(command == "stopall") effects.stop_all();
    else if(command == "pause") input >> effects.paused;
    else if(command == "channels") {
      output << "channels " << effects.radio_channel << ' ' << effects.cockpit_channel << ' ' << effects.cockpit_effect;
      for(auto const &c : effects.channels) output << ' ' << c.id << ' ' << c.active << ' ' << c.rate << ' ' << c.volume << ' ' << c.phase;
      output << '\n';
    } else if(command == "bank") {
      for(size_t i{0}; i != 189; ++i) { auto const &s{effects.sample(i)}; output << i << ' ' << s.rate << ' ' << s.repeat << ' ' << s.minimal << ' ' << s.pcm.size() << ' ' << apache::resources::fingerprint(s.pcm) << '\n'; }
    } else if(command == "gain") { unsigned int value; input >> value; output << apache::audio::volume_decibels(value) << '\n'; } else if(command == "mix") {
      size_t frames; std::string path; input >> frames >> std::quoted(path); if(frames > 48000 * 600) throw std::invalid_argument{"Audio capture too large"};
      std::vector<float> pcm(frames * 2); effects.render(pcm); if(path != "-") { std::ofstream file{path, std::ios::binary}; file.write(reinterpret_cast<char const*>(pcm.data()), static_cast<std::streamsize>(pcm.size() * sizeof(float))); }
      output << "mixed " << apache::resources::fingerprint({reinterpret_cast<uint8_t const*>(pcm.data()), pcm.size() * sizeof(float)}) << '\n';
    } else if(command == "stream") {
      apache::audio::stream_source source; input >> std::quoted(source.name) >> source.channels >> source.repeat >> source.loop;
      source.pcm = std::make_shared<apache::resources::bytes const>(data.read(source.name)); stream.start(std::move(source));
    } else if(command == "store") stream.store();
    else if(command == "halt") stream.stop();
    else if(command == "restore") stream.restore();
    else if(command == "advance") { unsigned int ms; input >> ms; if(ms > 600000) throw std::invalid_argument{"Audio capture too long"}; stream.advance(size_t{ms} * 22, [&](auto pcm, unsigned int){ captured.insert(captured.end(), pcm.begin(), pcm.end()); }); } else if(command == "clear") captured.clear();
    else if(command == "capture") { std::string path; input >> std::quoted(path); if(path != "-") { std::ofstream file{path, std::ios::binary}; file.write(reinterpret_cast<char const*>(captured.data()), static_cast<std::streamsize>(captured.size())); } output << "capture " << captured.size() << ' ' << apache::resources::fingerprint(captured) << '\n'; } else if(command == "streamstate") output << "streamstate " << stream.clock << ' ' << stream.head << ' ' << stream.tail << ' ' << stream.position << ' ' << stream.space << ' ' << stream.file_position << ' ' << stream.open << ' ' << stream.waiting << ' ' << stream.playing << ' ' << stream.stored() << ' ' << stream.stored_position() << '\n';
    else throw std::runtime_error{"Unknown audio command"};
    if(!input) throw std::runtime_error{"Incomplete audio command"};
  }
}
