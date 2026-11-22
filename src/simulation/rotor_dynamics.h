#pragma once
#include "flight_systems.h"

namespace apache::simulation {
struct rotor_dynamics {
  powerplant power;
  flight_controls controls;
  hover_control hover;
  // Positions in feet; body-forward, body-sideways and world-vertical speeds in ft/s.
  double x{0};
  double y{0};
  double z{0};
  double u{0};
  double v{0};
  double zdot{0};
  // Pitch, roll and heading in radians; body rates about the corresponding axes.
  double theta{0};
  double phi{0};
  double psi{0};
  double p{0};
  double q{0};
  double r{0};
  double cos_theta{1};
  double sin_theta{0};
  double tan_theta{0};
  double cos_phi{1};
  double sin_phi{0};
  double current_landing_pitch{0};
  double qmax{0};
  double qdem{0};
  double pmax{0};
  double pdem{0};
  double rmax{0};
  double rdem{0};
  double turnrate{0};
  double psidot{0};
  double phidot{0};
  double thetadot{0};
  double my_roll{0};
  double roll_a{2};
  double roll_b{1.5};
  double roll_c{1};
  double roll_d{2.2};
  double roll_decay_ss{0};
  double roll_decay{0};
  double rho{0};
  double kge{1};
  double kdu{0};
  double beta{0};
  double Tz{0};
  double eta{1};
  double zdotss{0};
  double Kt{0};
  double udot{0};
  double vdot{0};
  double abs_v{0};
  double Kdv{0};
  double g{32.2};
  double max_speed_timer{0};
  int32_t Tzw{0};
  int32_t zdotT{0};
  int32_t zdotTmax{80};
  int32_t wind_strength{0};
  bool wind_gust{false};
  bool max_speed_message{false};
  void update_rates(double dt, double ground, bool on_carrier, bool landed, bool absolute_throttle, unsigned int input_type, int32_t throttle, flight_events &events);
  void update_aerodynamics(double dt, int32_t delta_time, double ground, bool on_carrier, unsigned int random_sample, flight_events &events);
};
} // namespace apache::simulation
