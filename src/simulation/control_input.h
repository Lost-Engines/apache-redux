#pragma once
#include "realistic_flight.h"

namespace apache::simulation {
struct control_configuration {
  unsigned int cyclic{0};
  unsigned int collective{0};
  unsigned int tail_rotor{0};
  unsigned int input_type{1};
  unsigned int button_mode{0};
  bool absolute_throttle{false};
  bool rudder{false};
  bool analogue_hat{false};
  void resolve();
};
struct joystick_axis {
  int32_t minimum{0};
  int32_t centre{128};
  int32_t maximum{255};
  int32_t value{128};
};
class joystick_curve {
public:
  std::array<int32_t, 256> values{};
  explicit joystick_curve(unsigned int deadband = 10);
  int32_t sample(unsigned int mode, joystick_axis const &axis) const;
};
struct joystick_commands {
  std::array<uint32_t, 6> keys{};
  size_t count{0};
  bool alternate{false};
};
joystick_commands decode_joystick_buttons(unsigned int mode, unsigned int buttons, bool analogue_hat, unsigned int hat_position);
class control_input {
public:
  control_configuration configuration;
  std::array<bool, 128> held{};
  std::array<uint32_t, 128> counts{};
  std::array<int32_t, 6> keyboard{};
  flight_input flight;
  std::array<int32_t, 4> external{};
  std::array<joystick_axis, 4> joystick;
  joystick_curve curve;
  bool joystick_present{false};
  bool hat_present{false};
  unsigned int axis_count{0};
  unsigned int buttons{0};
  unsigned int hat{0};
  joystick_commands button_commands() const;
  void key(unsigned int scan, bool down);
  void timer(uint32_t ticks);
  void update(unsigned int view_moves, bool gunner, bool joystick_alt = false, bool throttle_reversed = false);
};
} // namespace apache::simulation
