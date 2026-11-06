#pragma once
#include <filesystem>
#include "graphics.h"
#include "preferences.h"
#include "resources/data_store.h"
#include "simulation/control_input.h"

namespace apache::configuration {
class settings {
  std::filesystem::path filename;
  std::optional<double> fov_override, lod_override;
public:
  preferences values;
  configuration::graphics graphics;
  unsigned int video_mode{1}, detail{9}, model{0}, forces{0}, volume{63}, deadband{0};
  bool planner{false}, animations{true}, speech{true}, music{true}, effects{true}, minimal_effects{false};
  simulation::control_configuration controls;
  std::array<simulation::joystick_axis, 4> axes;
  explicit settings(resources::data_store const &data);
  void apply();
  void load(std::filesystem::path const &directory);
  void save() const;
  void set(std::string_view key, int value);
  void set_field_of_view(double value);
  void set_lod_distance_multiplier(double value);
  void override_graphics(std::optional<double> fov, std::optional<double> lod);
};
} // namespace apache::configuration
