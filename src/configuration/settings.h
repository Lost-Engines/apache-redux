#pragma once
#include <filesystem>
#include "graphics.h"
#include "input/controllers.h"
#include "preferences.h"
#include "resources/data_store.h"
#include "simulation/control_input.h"

namespace apache::configuration {
class settings {
  std::filesystem::path filename;
  std::optional<double> fov_override;
  std::optional<double> lod_override;
public:
  preferences values;
  input::controller_settings controllers;
  std::string controller_error;
  configuration::graphics graphics;
  unsigned int video_mode{1};
  unsigned int detail{9};
  unsigned int model{0};
  unsigned int forces{0};
  unsigned int volume{63};
  unsigned int deadband{0};
  bool planner{false};
  bool animations{true};
  bool speech{true};
  bool music{true};
  bool effects{true};
  bool minimal_effects{false};
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
