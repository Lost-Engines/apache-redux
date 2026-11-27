#include "settings.h"
#include <algorithm>
#include <format>
#include <fstream>
#include <stdexcept>

namespace apache::configuration {
settings::settings(resources::data_store const &data) {
  if(data.contains("PREFS.CFG")) {
    auto const bytes{data.read("PREFS.CFG")};
    values.load({reinterpret_cast<char const*>(bytes.data()), bytes.size()});
  }
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
  // Older preference files have neither Redux setting. Ignore malformed edits.
  graphics.field_of_view = 90;
  graphics.lod_distance_multiplier = 6;
  if(auto value{values.get("Redux FOV")}) {
    try {
      graphics.field_of_view = parse_field_of_view(*value);
    } catch(std::runtime_error const&) {}
  }
  if(auto value{values.get("Redux LOD distance")}) {
    try {
      graphics.lod_distance_multiplier = parse_lod_distance_multiplier(*value);
    } catch(std::runtime_error const&) {}
  }
  if(fov_override) graphics.field_of_view = *fov_override;
  if(lod_override) graphics.lod_distance_multiplier = *lod_override;
  auto const integer{[&](std::string_view key, int fallback, int maximum){
    return static_cast<unsigned int>(std::clamp(values.integer(key).value_or(fallback), 0, maximum));
  }};
  video_mode = integer("Visual Resolution", 1, 1);
  detail = integer("Visual Detail", 9, 9);
  model = values.integer("Model ID").value_or(0) == 0 ? 0u : 2u;
  forces = integer("Forces Type", 0, 9);
  volume = integer("Sound Volume", 63, 63);
  deadband = integer("Control Deadband Size", 0, 126);
  planner = integer("Mission Planner", 0, 1);
  animations = integer("Animations", 1, 1);
  speech = integer("Sound Speech", 1, 1);
  music = integer("Sound Music", 1, 1);
  effects = integer("Sound effects", 1, 1);
  minimal_effects = integer("Sound effects level", 0, 1);
  controls.cyclic = integer("Cyclic control", 0, 4);
  controls.collective = integer("Collective control", 0, 6);
  controls.tail_rotor = integer("Tailrotor control", 0, 5);
  controls.resolve();
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
  bool const saved_preferences{std::filesystem::exists(path)};
  if(saved_preferences) {
    auto const size{std::filesystem::file_size(path)};
    if(size > 1024 * 1024) throw std::runtime_error{"Preference file is too large"};
    std::ifstream input{path, std::ios::binary};
    std::string text(static_cast<size_t>(size), '\0');
    if(!input.read(text.data(), static_cast<std::streamsize>(size))) throw std::runtime_error{"Cannot read preferences: " + path.string()};
    values.load(text);
    apply();
  }
  filename = path;
  auto const controls_path{directory / "controls.cfg"};
  controller_error.clear();
  if(std::filesystem::exists(controls_path)) {
    try {
      if(std::filesystem::file_size(controls_path) > 1024 * 1024) throw std::runtime_error{"Controller settings are too large"};
      std::ifstream input{controls_path, std::ios::binary};
      std::string text{std::istreambuf_iterator<char>{input}, {}};
      if(!input || input.bad()) throw std::runtime_error{"Cannot read controller settings"};
      controllers = input::controller_settings::parse(text);
    } catch(std::exception const &error) {
      // Show the problem in Controls rather than aborting startup or silently
      // overwriting the player's file with defaults.
      controller_error = error.what();
    }
  } else if(saved_preferences && controls.input_type != 1) {
    input::device_state legacy;
    legacy.id = {"*", "First joystick", 0};
    legacy.axes.resize(4);
    legacy.buttons.resize(4);
    legacy.hats.resize(1);
    auto imported{input::joystick_defaults(legacy, false)};
    imported.name = "Imported controls";
    if(controls.rudder) {
      imported.axes[1].positive = {legacy.id, input::source_kind::axis, 2};
      imported.axes[1].inverted = true;
    }
    if(controls.absolute_throttle) {
      imported.axes[0].positive = {legacy.id, input::source_kind::axis, 3};
      imported.axes[0].inverted = true;
    }
    for(size_t i{0}; i < imported.axes.size(); ++i) {
      auto &a{imported.axes[i]};
      a.deadzone = std::clamp(deadband / 127.0, 0., .5);
      a.response = i == 0 ? 1 : 2;
      if(a.positive.assigned()) {
        auto const &old{axes[a.positive.index]};
        a.minimum = old.minimum / 127.5 - 1;
        a.centre = old.centre / 127.5 - 1;
        a.maximum = old.maximum / 127.5 - 1;
      }
    }
    imported.buttons.fill({});
    for(unsigned int i{0}; i < 4; ++i) {
      auto const decoded{simulation::decode_joystick_buttons(controls.button_mode, 1u << i, false, 255)};
      for(size_t j{0}; j < decoded.count; ++j) {
        auto const scan{static_cast<uint16_t>(decoded.keys[j])};
        auto const found{std::ranges::find(input::actions, scan, &input::action::scan)};
        if(found != input::actions.end()) imported.buttons[static_cast<size_t>(found - input::actions.begin())] = {legacy.id, input::source_kind::button, i};
      }
    }
    if(controls.cyclic >= 2) {
      for(auto const &[scan, direction] : std::array<std::pair<uint16_t, unsigned int>, 4>{{{0x3c, 1}, {0x3f, 2}, {0x3d, 4}, {0x3e, 8}}}) {
        auto const found{std::ranges::find(input::actions, scan, &input::action::scan)};
        imported.buttons[static_cast<size_t>(found - input::actions.begin())] = {legacy.id, input::source_kind::hat, 0, direction};
      }
    }
    controllers.profiles = {std::move(imported)};
  }
}
void settings::save() const {
  if(filename.empty()) return;
  std::filesystem::create_directories(filename.parent_path());
  auto temporary{filename};
  temporary += ".tmp";
  {
    std::ofstream output{temporary, std::ios::binary | std::ios::trunc};
    auto const text{values.serialise()};
    if(!output || !output.write(text.data(), static_cast<std::streamsize>(text.size())) || !output.flush()) throw std::runtime_error{"Cannot save preferences: " + filename.string()};
  }
  std::filesystem::rename(temporary, filename);
  if(controller_error.empty()) {
    auto const controls_path{filename.parent_path() / "controls.cfg"};
    auto controls_temporary{controls_path};
    controls_temporary += ".tmp";
    {
      std::ofstream output{controls_temporary, std::ios::binary | std::ios::trunc};
      auto const text{controllers.serialise()};
      if(!output || !output.write(text.data(), static_cast<std::streamsize>(text.size())) || !output.flush()) throw std::runtime_error{"Cannot save controller profiles: " + controls_path.string()};
    }
    std::filesystem::rename(controls_temporary, controls_path);
  }
}
void settings::set(std::string_view const key, int const value) {
  values.set(key, value);
  apply();
}
void settings::set_field_of_view(double const value) {
  auto const text{std::format("{}", value)};
  parse_field_of_view(text);
  fov_override.reset();
  values.set("Redux FOV", text);
  apply();
}
void settings::set_lod_distance_multiplier(double const value) {
  auto const text{std::format("{}", value)};
  parse_lod_distance_multiplier(text);
  lod_override.reset();
  values.set("Redux LOD distance", text);
  apply();
}
void settings::override_graphics(std::optional<double> const fov, std::optional<double> const lod) {
  fov_override = fov;
  lod_override = lod;
  apply();
}
} // namespace apache::configuration
