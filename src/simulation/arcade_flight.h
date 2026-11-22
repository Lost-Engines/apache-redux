#pragma once
#include "realistic_flight.h"

namespace apache::simulation {
struct arcade_flight;
struct arcade_environment {
  std::function<int32_t(int32_t, int32_t)> height;
  std::function<unsigned int(int32_t, int32_t)> surface;
  std::function<math::pose(math::pose const &)> place;
  std::function<void(arcade_flight &)> leave_carrier;
  std::function<void()> landing_award;
};
struct arcade_flight {
  powerplant power;
  flight_controls controls;
  hover_control hover;
  flight_readings readings;
  double x{0};
  double y{0};
  double z{0};
  double heading{0};
  double pitch{0};
  double roll{0};
  double delta_heading{0};
  double delta_pitch{0};
  double delta_roll{0};
  double delta_throttle{0};
  double forward_velocity{0};
  double lateral_velocity{0};
  double vertical_velocity{0};
  double delta_forward_velocity{0};
  double delta_lateral_velocity{0};
  double delta_vertical_velocity{0};
  double lateral_ss{0};
  double lateral_vmax{0};
  double max_heading{0};
  double height{0};
  double last_frame_ground_height{0};
  double dx{0};
  double dy{0};
  double dz{0};
  double preset_altitude{0};
  double d_height{0};
  double true_dz{0};
  std::array<double, 9> attitude{};
  bool landed{false};
  bool on_carrier{false};
  bool reset_altitude{true};
  bool infinite_fuel{false};
  bool invulnerable{false};
  bool no_crashes{false};
  unsigned int death{0};
  std::function<void()> died;
  void request_death(unsigned int cause);
  void initialise(flight_start const &start, math::trigonometry const &trig, arcade_environment const &environment = {});
  flight_step_result advance(double dt, flight_input const &input, arcade_environment const &environment, math::trigonometry const &trig);
  void transfer_from_carrier(int32_t speed, uint32_t carrier_heading, math::trigonometry const &trig);
  void update_inputs(std::array<int32_t, 4> const &input);
  void update_rates(double dt, flight_events &events);
  void update_motion(double dt, bool absolute_throttle, unsigned int input_type, int32_t throttle, flight_events &events);
  void consume_fuel(double dt, flight_events &events);
  void update_hover(double dt, flight_events &events);
  void toggle_hover();
};
} // namespace apache::simulation
