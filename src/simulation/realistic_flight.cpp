#include "realistic_flight.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace apache::simulation {
namespace {
double constexpr tau{6.28318530718}, radians_per_unit{2.328306437080797e-10};
int32_t integer(double const value) {
  if(!std::isfinite(value) || value < -0x1p63 || value >= 0x1p63) throw std::runtime_error{"Invalid flight-model conversion"};
  return std::bit_cast<int32_t>(static_cast<uint32_t>(static_cast<int64_t>(value)));
}
math::matrix3 fixed_attitude(std::array<double, 9> const &matrix) {
  math::matrix3 result;
  for(size_t i{0}; i != result.size(); ++i) result[i] = integer(matrix[i] * 2147483647.0);
  return result;
}
} // anonymous namespace
std::array<double, 9> flight_attitude(double const heading, double const pitch, double const roll) {
  double const sh{std::sin(heading)}, ch{std::cos(heading)}, sp{std::sin(pitch)}, cp{std::cos(pitch)}, sr{std::sin(roll)}, cr{std::cos(roll)};
  return {cr * ch + sr * sp * sh, sr * sp * ch + -cr * sh, -sr * cp, cp * sh, cp * ch, sp, sr * ch - cr * sp * sh, -sr * sh - cr * sp * ch, cr * cp};
}
void realistic_flight::initialise(flight_start const &start, math::trigonometry const &trig, flight_environment const &environment) {
  *this = realistic_flight{};
  auto &d{dynamics}; auto &power{d.power};
  d.x = start.position[0] / 256; d.y = start.position[1] / 256; d.z = start.position[2] / 256;
  power.fuel = start.fuel; power.weight = start.dry_weight + start.fuel;
  power.faults = start.faults;
  if(start.fuel > 0) power.faults &= ~0x300000u;
  else if(start.fuel == 0) power.faults |= 0x300000;
  power.engine_rpm = power.rotor_rpm = power.engine_rpm_target = power.rotor_rpm_target = power.left_rpm = power.right_rpm = 100;
  power.rotor_torque = 74;
  d.current_landing_pitch = 0.08975979010257143;
  auto const angle{[](uint32_t const value){ return static_cast<double>(std::bit_cast<int32_t>(value)) * tau * radians_per_unit; }};
  d.phi = angle(start.roll); d.theta = angle(start.pitch); d.psi = angle(start.heading);
  on_carrier = start.on_carrier; d.wind_strength = start.wind_strength; wind_direction = start.wind_direction;
  landed = landing_question_asked = d.z <= start.ground_height / 256;
  power.collective = landed ? 50 : 73;
  if(landed) d.theta = d.current_landing_pitch;
  readings.angles = {start.heading, landed ? 0x038e38e5u : 0u, 0};
  readings.position.position = {integer(d.x * 256), integer(d.y * 256), integer(d.z * 256 + 2304)};
  if(environment.place) readings.position = environment.place(readings.position);
  if(start.quick_flight) { d.z = 100; power.collective = 90; d.u = 238; }
  attitude = flight_attitude(d.psi, d.theta, d.phi);
  readings.position.rotation = trig.transformation(readings.angles[0], readings.angles[1], readings.angles[2]);
  d.Kdv = 0x1.c189374bc6a7fp-2;
  d.cos_theta = std::cos(d.theta); d.sin_theta = std::sin(d.theta);
}
void realistic_flight::transfer_from_carrier(int32_t const speed, uint32_t const heading, math::trigonometry const &trig) {
  auto &d{dynamics};
  auto const angle{[](uint32_t const value){ return static_cast<double>(std::bit_cast<int32_t>(value)) * tau * radians_per_unit; }};
  readings.position.rotation = trig.transformation(readings.angles[0], readings.angles[1], readings.angles[2]);
  readings.angles = trig.angles(readings.position.rotation);
  d.psi = angle(readings.angles[0]); d.theta = angle(readings.angles[1]); d.phi = angle(readings.angles[2]);
  // The original truncates the carrier heading and relative angle to whole radians.
  double const relative{static_cast<double>(integer(integer(angle(heading)) - d.psi))};
  d.u = std::cos(relative) * (speed / 256); d.v = std::sin(relative) * (speed / 256);
  d.x = readings.position.position[0] * 0.00390625; d.y = readings.position.position[1] * 0.00390625;
  d.z = math::subtract(readings.position.position[2], 2304) * 0.00390625;
}
void realistic_flight::request_death(unsigned int const cause) {
  if(death || invulnerable || ((cause == 2 || cause == 3 || cause == 12) && no_crashes)) return;
  death = cause;
  if(died) died();
}
flight_step_result realistic_flight::advance(double const dt, int32_t const delta_time, flight_input const &input, flight_environment const &environment, math::trigonometry const &trig, unsigned int const random_sample) {
  if(dt <= 0 || !std::isfinite(dt) || delta_time <= 0) throw std::runtime_error{"Invalid flight timestep"};
  flight_step_result result;
  auto &d{dynamics}; auto &power{d.power}; auto &events{result.events};
  auto const relative_height{[&]{ return environment.height(integer(d.x * 256), integer(d.y * 256)); }};
  d.controls.update(input.controls, on_carrier);
  d.update_rates(dt, relative_height() / 256, on_carrier, landed, input.absolute_throttle, input.type, input.controls[0], events);
  d.update_aerodynamics(dt, delta_time, relative_height() / 256, on_carrier, random_sample, events);
  for(auto const event : events) {
    if(event == flight_event::hard_impact) request_death(3);
    else if(event == flight_event::overspeed_fatal) request_death(11);
  }
  d.hover.update(d.u, d.v, d.zdot, d.theta, d.controls.changed, power.collective, events);
  if(!invulnerable) power.update_faults(dt, delta_time, overtorque_enabled, events);
  power.update_temperature(dt);
  result.rotor_rate = power.update_engines(dt, d.z, relative_height(), d.zdotT, d.zdotTmax);
  power.consume_fuel(dt, readings.airspeed, infinite_fuel, invulnerable, events);
  attitude = flight_attitude(d.psi, d.theta, d.phi);
  readings.position.rotation = fixed_attitude(attitude);
  double const forward{overtorque_enabled ? d.u : d.u * acceleration}, sideways{overtorque_enabled ? d.v : d.v * acceleration};
  double vertical{overtorque_enabled ? d.zdot : d.zdot * acceleration};
  dx = dt * attitude[3] * forward; dy = dt * attitude[4] * forward;
  dx = attitude[0] * sideways * dt + dx; dy = attitude[1] * sideways * dt + dy;
  if(!landed) {
    double const wind_angle{(std::bit_cast<int32_t>(wind_direction) / 0xb60b61) * tau * 0.0027777777777777775};
    dy -= std::cos(wind_angle) * d.wind_strength * dt;
    dx -= std::sin(wind_angle) * d.wind_strength * dt;
  }
  if(!terrain_follow) reset_altitude = true;
  else {
    if(reset_altitude) { preset_altitude = d.z - ground_height * 0.00390625; reset_altitude = false; }
    vertical = preset_altitude - (d.z - ground_height);
  }
  dz = vertical * dt;
  d.x += dx; d.y += dy; d.z += dz;
  if(on_carrier && d.z < environment.carrier_deck / 256) d.z = environment.carrier_deck / 256;
  d.z = std::clamp(d.z, 0.0, 12800000.0); d.x = std::clamp(d.x, 0.0, 1048575.0); d.y = std::clamp(d.y, 0.0, 1048575.0);
  ground_height = relative_height() / 256;
  if(d.z <= ground_height && dz != 0 && !landed) {
    auto const landing{evaluate_landing(d.u, d.zdot, environment.surface(readings.position.position[0], readings.position.position[1]), on_carrier, death)};
    if(landing.grade) { result.landing = landing; landed = true; if(landing.death) request_death(landing.death); if(landing.award && environment.landing_award) environment.landing_award(); }
  }
  bool const pull_up{ground_height + 250 >= d.z && d.zdot < 0 && !landed && -d.zdot * 0.045454545454545456 > std::max(1.0, (d.z - ground_height) * 0.02) * 0.8};
  if(pull_up) {
    if(!pull_up_message) { events.push_back(flight_event::pull_up_on); pull_up_message = true; }
    if(pull_up_timer > 50) { pull_up_timer = 0; events.push_back(flight_event::pull_up_sound); }
    pull_up_timer += math::multiply_q31(delta_time, 200);
  } else if(pull_up_message) { events.push_back(flight_event::pull_up_off); pull_up_message = false; }
  if(d.z <= ground_height && dz <= 0 && !landing_question_asked && game_mode == 0) {
    if(d.current_landing_pitch == 0.08975979010257143) { events.push_back(flight_event::mission_end_check); landing_question_asked = true; }
    if(on_carrier) { events.push_back(flight_event::mission_end_check); landing_question_asked = true; }
  }
  if(landed && dz > 0 && power.collective > 60) {
    events.push_back(flight_event::takeoff);
    if(on_carrier && environment.leave_carrier) environment.leave_carrier(*this);
    landed = false; on_carrier = false; landing_question_asked = false;
  }
  readings.position.position = {integer(d.x * 256), integer(d.y * 256), integer(d.z * 256 + 2304)};
  if(environment.place) readings.position = environment.place(readings.position);
  readings.angles = trig.angles(readings.position.rotation);
  readings.climb = integer(d.zdot * 256);
  readings.sidespeed = integer((overtorque_enabled ? d.v : d.v * acceleration) * 0.5882352941176471);
  readings.airspeed = integer((overtorque_enabled ? d.u : d.u * acceleration) * 0.5882352941176471);
  auto const gauge{[](double const value){ return std::clamp(integer(value), 0, 130); }};
  readings.engines = {static_cast<double>(gauge(power.engine_torque)), static_cast<double>(gauge(power.engine_torque)), power.left_temperature, power.right_temperature, static_cast<double>(gauge(power.left_rpm)), static_cast<double>(gauge(power.right_rpm)), static_cast<double>(gauge(power.rotor_rpm)), static_cast<double>(gauge(power.collective))};
  readings.torque = gauge(power.engine_torque); readings.fuel = integer(power.fuel);
  return result;
}
} // namespace apache::simulation
