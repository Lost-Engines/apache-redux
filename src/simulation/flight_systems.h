#pragma once
#include <array>
#include <cstdint>
#include <vector>

namespace apache::simulation {
enum class flight_event {
  fuel_exhausted,
  hover_on,
  hover_off,
  engine_overheat,
  left_engine_failed,
  right_engine_failed,
  stop_rotor,
  powered_rotor,
  windmilling_rotor,
  overtorque_on,
  overtorque_off,
  warning_sound,
  engines_overheated,
  overspeed_on,
  overspeed_off,
  excessive_sideways_acceleration,
  pull_up_on,
  pull_up_off,
  pull_up_sound,
  hard_impact,
  overspeed_fatal,
  mission_end_check,
  takeoff
};
using flight_events = std::vector<flight_event>;
struct powerplant {
  double fuel{0};
  double weight{0};
  double maximum_weight{20000};
  double maximum_fuel_consumption{0.05};
  double weight_factor{0};
  double fuel_consumption{0};
  double fuel_rate{0};
  double endurance{0};
  double collective{0};
  double engine_rpm{0};
  double rotor_rpm{0};
  double engine_torque{0};
  double rotor_torque{0};
  double descent_ratio{0};
  double engine_rpm_target{0};
  double torque_target{0};
  double rotor_rpm_target{0};
  double left_rpm{0};
  double right_rpm{0};
  double temperature{0};
  double left_temperature{0};
  double right_temperature{0};
  double overtorque_timer{0};
  double warning_timer{0};
  uint32_t faults{0};
  bool overheat_message{false};
  bool overtorque_message{false};
  unsigned int rotor_sound{0};
  void consume_fuel(double dt, int32_t airspeed, bool infinite, bool invulnerable, flight_events &events);
  int32_t update_engines(double dt, double altitude, int32_t ground_height, int32_t vertical_lift, int32_t maximum_lift);
  void update_temperature(double dt);
  void update_faults(double dt, int32_t delta_time, bool overtorque_enabled, flight_events &events);
};
struct flight_controls {
  // Collective, tail rotor, longitudinal cyclic, lateral cyclic.
  std::array<double, 4> values{};
  std::array<double, 4> previous{};
  bool changed{false};
  void update(std::array<int32_t, 4> const &input, bool on_carrier);
};
struct hover_control {
  bool active{false};
  bool previous{false};
  void toggle(double forward_speed);
  void update(double &forward_speed, double &sideways_speed, double vertical_speed, double pitch, bool user_input, double &collective, flight_events &events);
};
struct landing_result {
  unsigned int grade{0};
  unsigned int death{0};
  bool landed{false};
  bool award{false};
};
landing_result evaluate_landing(double forward_speed, double vertical_speed, unsigned int surface, bool on_carrier, unsigned int previous_death);
} // namespace apache::simulation
