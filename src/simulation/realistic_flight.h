#pragma once
#include <functional>
#include <optional>
#include "math/geometry.h"
#include "math/trigonometry.h"
#include "rotor_dynamics.h"

namespace apache::simulation {
struct flight_readings {
  math::pose position;
  std::array<uint32_t, 3> angles{};
  int32_t airspeed{0}, sidespeed{0}, climb{0}, torque{0}, fuel{0};
  // Left/right torque, left/right temperature, left/right RPM, rotor RPM, collective.
  std::array<double, 8> engines{0, 0, 0, 0, 100, 100, 100, 0};
};
struct flight_start {
  math::vector3 position{0, 0, 0};
  uint32_t heading{0}, pitch{0}, roll{0};
  uint32_t object_heading{0};
  int32_t ground_height{0};
  int32_t dry_weight{0};
  double fuel{0};
  uint32_t faults{0};
  bool quick_flight{false};
  bool on_carrier{false};
  int32_t wind_strength{0};
  uint32_t wind_direction{0};
};
struct flight_input {
  std::array<int32_t, 4> controls{};
  bool absolute_throttle{false};
  unsigned int type{0};
};
struct realistic_flight;
struct flight_environment {
  std::function<int32_t(int32_t, int32_t)> height;
  std::function<unsigned int(int32_t, int32_t)> surface;
  // Scene services supply carrier motion and update the mobile-object lists.
  std::function<math::pose(math::pose const&)> place;
  std::function<void(realistic_flight&)> leave_carrier;
  int32_t carrier_deck{0};
  std::function<void()> landing_award;
};
struct flight_step_result {
  flight_events events;
  std::optional<landing_result> landing;
  int32_t rotor_rate{0};
};
struct realistic_flight {
  rotor_dynamics dynamics;
  flight_readings readings;
  std::array<double, 9> attitude{};
  double dx{0}, dy{0}, dz{10}, ground_height{0}, preset_altitude{0}, pull_up_timer{0};
  bool landed{true}, on_carrier{false}, landing_question_asked{true};
  bool terrain_follow{false}, reset_altitude{true}, pull_up_message{false};
  bool overtorque_enabled{true}, infinite_fuel{false}, invulnerable{false}, no_crashes{false};
  unsigned int game_mode{0}, death{0};
  std::function<void()> died;
  int32_t acceleration{1};
  uint32_t wind_direction{0};
  void initialise(flight_start const &start, math::trigonometry const &trig, flight_environment const &environment = {});
  void transfer_from_carrier(int32_t speed, uint32_t heading, math::trigonometry const &trig);
  flight_step_result advance(double dt, int32_t delta_time, flight_input const &input, flight_environment const &environment, math::trigonometry const &trig, unsigned int random_sample);
  void request_death(unsigned int cause);
};
std::array<double, 9> flight_attitude(double heading, double pitch, double roll);
} // namespace apache::simulation
