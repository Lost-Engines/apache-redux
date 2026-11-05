#pragma once
#include <filesystem>
#include "preferences.h"
#include "resources/data_store.h"
#include "simulation/control_input.h"

namespace apache::configuration {
class settings {
  std::filesystem::path filename;
public:
  preferences values;
  unsigned int video_mode{1}, detail{9}, model{0}, forces{0}, volume{63}, deadband{0};
  bool planner{false}, animations{true}, speech{true}, music{true}, effects{true}, minimal_effects{false};
  simulation::control_configuration controls;
  std::array<simulation::joystick_axis, 4> axes;
  explicit settings(resources::data_store const &data);
  void apply();
  void load(std::filesystem::path const &directory);
  void save() const;
  void set(std::string_view key, int value);
};
} // namespace apache::configuration
