#include "settings.h"
#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace apache::configuration {
settings::settings(resources::data_store const &data) {
  if(data.contains("PREFS.CFG")) { auto const bytes{data.read("PREFS.CFG")}; values.load({reinterpret_cast<char const*>(bytes.data()), bytes.size()}); }
  // Redux's initial choices override the CD defaults; saved player preferences load later.
  values.set("Visual Detail", 9);
  values.set("Model ID", 0);
  values.set("Sound Speech", 1);
  values.set("Sound Music", 1);
  values.set("Sound effects", 1);
  values.set("Sound effects level", 0);
  apply();
}
void settings::apply() {
  auto const integer{[&](std::string_view key, int fallback, int maximum){ return static_cast<unsigned int>(std::clamp(values.integer(key).value_or(fallback), 0, maximum)); }};
  video_mode = integer("Visual Resolution", 1, 1); detail = integer("Visual Detail", 9, 9);
  model = values.integer("Model ID").value_or(0) == 0 ? 0u : 2u;
  forces = integer("Forces Type", 0, 9); volume = integer("Sound Volume", 63, 63); deadband = integer("Control Deadband Size", 0, 126);
  planner = integer("Mission Planner", 0, 1); animations = integer("Animations", 1, 1);
  speech = integer("Sound Speech", 1, 1); music = integer("Sound Music", 1, 1);
  effects = integer("Sound effects", 1, 1); minimal_effects = integer("Sound effects level", 0, 1);
  controls.cyclic = integer("Cyclic control", 0, 4); controls.collective = integer("Collective control", 0, 6); controls.tail_rotor = integer("Tailrotor control", 0, 5); controls.resolve();
  std::array<std::string, 4> const names{"Ax", "Ay", "Bx", "By"};
  for(size_t i{0}; i != axes.size(); ++i) {
    std::string const prefix{"Joystick " + names[i]};
    simulation::joystick_axis axis{static_cast<int32_t>(integer(prefix + " Min", 0, 255)), static_cast<int32_t>(integer(prefix + " Mid", 128, 255)), static_cast<int32_t>(integer(prefix + " Max", 255, 255)), 128};
    // GLFW exposes normalized axes. Absent or invalid DOS calibration uses
    // their full normalized range, without copying hardware-specific timers.
    axes[i] = axis.minimum < axis.centre && axis.centre < axis.maximum ? axis : simulation::joystick_axis{};
  }
}
void settings::load(std::filesystem::path const &directory) {
  auto const path{directory / "prefs.cfg"};
  if(std::filesystem::exists(path)) {
    auto const size{std::filesystem::file_size(path)}; if(size > 1024 * 1024) throw std::runtime_error{"Preference file is too large"};
    std::ifstream input{path, std::ios::binary}; std::string text(static_cast<size_t>(size), '\0');
    if(!input.read(text.data(), static_cast<std::streamsize>(size))) throw std::runtime_error{"Cannot read preferences: " + path.string()};
    values.load(text); apply();
  }
  filename = path;
}
void settings::save() const {
  if(filename.empty()) return;
  std::filesystem::create_directories(filename.parent_path()); auto temporary{filename}; temporary += ".tmp";
  { std::ofstream output{temporary, std::ios::binary | std::ios::trunc}; auto const text{values.serialise()}; if(!output || !output.write(text.data(), static_cast<std::streamsize>(text.size())) || !output.flush()) throw std::runtime_error{"Cannot save preferences: " + filename.string()}; }
  std::filesystem::rename(temporary, filename);
}
void settings::set(std::string_view const key, int const value) { values.set(key, value); apply(); }
} // namespace apache::configuration
