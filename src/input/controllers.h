#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace apache::input {
struct device_id {
  std::string guid;
  std::string name;
  unsigned int instance{0};
  bool operator==(device_id const &) const = default;
};
struct device_state {
  device_id id;
  std::vector<float> axes;
  std::vector<unsigned char> buttons;
  std::vector<unsigned char> hats;
  std::vector<float> gamepad_axes;
  std::vector<unsigned char> gamepad_buttons;
};
enum class source_kind { none, axis, button, hat, gamepad_axis, gamepad_button };
struct binding {
  device_id device;
  source_kind kind{source_kind::none};
  unsigned int index{0};
  unsigned int direction{1}; // Hat bit; diagonals can operate two actions together.
  double rest{-1};
  double end{1}; // Axis direction and resting position for triggers/half axes.
  bool operator==(binding const &) const = default;
  bool assigned() const { return kind != source_kind::none; }
};
struct axis_binding {
  binding positive;
  binding negative;
  bool paired{false};
  bool inverted{false};
  double minimum{-1};
  double centre{0};
  double maximum{1};
  double deadzone{0.06};
  double response{1.5};
  bool operator==(axis_binding const &) const = default;
};
struct action {
  std::string_view id;
  std::string_view name;
  uint16_t scan;
};
inline constexpr std::array actions{
  action{"fire", "Fire weapon", 0x39},
  action{"weapon", "Select / arm weapon", 0x1c},
  action{"safe", "Weapons safe", 0x201c},
  action{"lock", "Lock target", 0x26},
  action{"next_target", "Next target", 0x0e},
  action{"previous_target", "Previous target", 0x400e},
  action{"chaff", "Release chaff", 0x2e},
  action{"flare", "Release flare", 0x21},
  action{"auto_countermeasures", "Auto countermeasures", 0x202e},
  action{"radar", "Radar mode", 0x13},
  action{"ecm", "Toggle ECM", 0x12},
  action{"hover", "Auto hover", 0x18},
  action{"collective_up", "Increase collective", 0x10},
  action{"collective_down", "Decrease collective", 0x1e},
  action{"rudder_left", "Left pedal", 0x2c},
  action{"rudder_right", "Right pedal", 0x2d},
  action{"pitch_up", "Cyclic back", 0x50},
  action{"pitch_down", "Cyclic forward", 0x48},
  action{"roll_left", "Cyclic left", 0x4b},
  action{"roll_right", "Cyclic right", 0x4d},
  action{"pilot", "Pilot cockpit", 0x3c},
  action{"gunner", "Gunner cockpit", 0x3d},
  action{"look_left", "Look left", 0x3e},
  action{"look_right", "Look right", 0x3f},
  action{"external", "External view", 0x40},
  action{"hud", "Full-screen HUD / MFDs", 0x23},
  action{"optics", "Target optics", 0x14},
  action{"missile", "Weapon camera", 0x41},
  action{"group", "Next group leader", 0x58},
  action{"orbit", "Hold to orbit camera", 0x38},
  action{"zoom_in", "Zoom in", 0x34},
  action{"zoom_out", "Zoom out", 0x33},
  action{"left_mfd", "Next left MFD page", 0x1a},
  action{"right_mfd", "Next right MFD page", 0x1b},
  action{"previous_left_mfd", "Previous left MFD page", 0x401a},
  action{"pnvs", "Night vision (PNVS)", 0x2f},
  action{"waypoint", "Next waypoint", 0x31},
  action{"pause", "Pause", 0x19},
  action{"time", "Time acceleration", 0x0f},
  action{"previous_right_mfd", "Previous right MFD page", 0x401b},
  action{"toggle_left_mfd", "Left MFD on / off", 0x201a},
  action{"toggle_right_mfd", "Right MFD on / off", 0x201b},
  action{"weapon_target", "Weapon target camera", 0x42},
  action{"target_camera", "Locked target camera", 0x43},
  action{"remote_camera", "Remote camera", 0x44},
  action{"spectator", "Spectator camera", 0x57},
  action{"auto_target", "Automatic targeting", 0x15},
  action{"radar_range", "Ground radar range", 0x22},
  action{"radar_freeze", "Freeze ground radar", 0x16},
  action{"tactical_range", "Tactical radar range", 0x25},
  action{"threat", "Target category", 0x32},
  action{"previous_waypoint", "Previous waypoint", 0x4031},
  action{"hud_colour", "HUD colour", 0x17},
};
inline constexpr std::array<std::string_view, 4> axis_names{"Collective", "Tail rotor / rudder", "Cyclic pitch", "Cyclic roll"};
struct controller_profile {
  std::string name{"Default"};
  bool enabled{false};
  std::array<axis_binding, 4> axes;
  std::array<binding, actions.size()> buttons;
  bool operator==(controller_profile const &) const = default;
};
struct controller_settings {
  size_t selected{0};
  std::vector<controller_profile> profiles{controller_profile{}};
  controller_profile &current() { return profiles.at(selected); }
  controller_profile const &current() const { return profiles.at(selected); }
  std::string serialise() const;
  static controller_settings parse(std::string_view text);
  bool operator==(controller_settings const &) const = default;
};
device_state const *find_device(binding const &source, std::span<device_state const> devices);
std::optional<double> raw_value(binding const &source, std::span<device_state const> devices);
double activation(binding const &source, std::span<device_state const> devices);
std::optional<double> axis_value(axis_binding const &axis, bool collective, std::span<device_state const> devices);
std::string describe(binding const &source);
controller_profile joystick_defaults(device_state const &device, bool gamepad);
// Freeze imported first-joystick references when the actual device is known.
bool resolve_imported_devices(controller_settings &settings, std::span<device_state const> devices);
struct mapped_input {
  std::array<std::optional<double>, 4> axes;
  bool absolute_collective{false};
  std::array<bool, actions.size()> down{};
  std::array<bool, actions.size()> pressed{};
};
class controller_mapper {
  std::array<bool, actions.size()> armed{};
  std::array<bool, actions.size()> previous{};
  std::optional<double> collective;
public:
  mapped_input sample(controller_profile const &profile, std::span<device_state const> devices, bool active);
  void reset();
};
// Learn from a deliberate change, ignoring parked throttles, resting triggers,
// held buttons and small amounts of stick noise at the start of capture.
class binding_capture {
  std::vector<device_state> baseline;
  bool axes_only{false};
public:
  void begin(std::span<device_state const> devices, bool only_axes);
  std::optional<binding> poll(std::span<device_state const> devices);
};
} // namespace apache::input
