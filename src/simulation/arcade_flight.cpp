#include "arcade_flight.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace apache::simulation {
namespace {
double constexpr pi{3.14159265359}, tau{6.28318530718}, half_pi{1.570796326795}, three_half_pi{4.712388980385};
int32_t integer(double const value) {
  if(!std::isfinite(value) || value < -0x1p63 || value >= 0x1p63) throw std::runtime_error{"Invalid Arcade flight conversion"};
  return std::bit_cast<int32_t>(static_cast<uint32_t>(static_cast<int64_t>(value)));
}
double angle(uint32_t const value) {
  return static_cast<double>(std::bit_cast<int32_t>(value)) * tau * 2.328306437080797e-10;
}
double wrap(double const value) {
  return value >= tau ? value - tau : value < 0 ? value + tau : value;
}
}
void arcade_flight::initialise(flight_start const &start, math::trigonometry const &trig, arcade_environment const &environment) {
  *this = arcade_flight{};
  readings.engines.fill(0);
  power.rotor_sound = 1;
  power.fuel = start.fuel;
  power.weight = 14058 + start.fuel + start.fuel;
  power.faults = start.faults;
  if(start.fuel > 0) {
    power.faults &= ~0x300000u;
  } else if(start.fuel == 0) power.faults |= 0x300000;
  x = start.position[0];
  y = start.position[1];
  z = start.quick_flight ? 25600 : start.position[2];
  if(start.quick_flight) forward_velocity = 60480;
  height = last_frame_ground_height = start.ground_height;
  on_carrier = start.on_carrier;
  landed = z - height <= 0 || on_carrier;
  readings.position.position = {integer(x), integer(y), integer(z + 2304)};
  if(environment.place) readings.position = environment.place(readings.position);
  heading = angle(start.heading);
  readings.position.rotation = trig.transformation(start.object_heading, 0, 0);
}
void arcade_flight::transfer_from_carrier(int32_t const speed, uint32_t const carrier_heading, math::trigonometry const &trig) {
  readings.angles = trig.angles(readings.position.rotation);
  heading = angle(readings.angles[0]);
  pitch = angle(readings.angles[1]);
  roll = angle(readings.angles[2]);
  double const relative{static_cast<double>(integer(integer(angle(carrier_heading)) - heading))};
  forward_velocity = std::cos(relative) * speed;
  lateral_velocity = std::sin(relative) * speed;
  delta_heading = delta_pitch = delta_roll = 0;
  x = readings.position.position[0];
  y = readings.position.position[1];
  z = math::subtract(readings.position.position[2], 2304);
}
flight_step_result arcade_flight::advance(double const dt, flight_input const &input, arcade_environment const &environment, math::trigonometry const &trig) {
  if(dt <= 0 || !std::isfinite(dt)) throw std::runtime_error{"Invalid Arcade timestep"};
  flight_step_result result;
  auto &events{result.events};
  height = environment.height(integer(x), integer(y));
  update_inputs(input.controls);
  update_rates(dt, events);
  update_motion(dt, input.absolute_throttle, input.type, input.controls[0], events);
  attitude = flight_attitude(heading, pitch, roll);
  for(size_t i{0}; i != 9; ++i) readings.position.rotation[i] = integer(attitude[i] * 2147483647.0);
  dz = vertical_velocity * dt;
  dx = attitude[0] * lateral_velocity * dt + dt * attitude[3] * forward_velocity;
  dy = attitude[1] * lateral_velocity * dt + dt * attitude[4] * forward_velocity;
  if(dz == 0 && !(power.faults & 0x300000)) {
    if(reset_altitude) {
      preset_altitude = z - height;
      reset_altitude = false;
    }
    dz = (preset_altitude - (z - height)) * dt;
  } else {
    reset_altitude = true;
  }
  x = std::clamp(dx + x, 0.0, 268435455.0);
  y = std::clamp(dy + y, 0.0, 268435455.0);
  if(z <= height && !landed) {
    d_height = environment.height(integer(x), integer(y)) - last_frame_ground_height;
    true_dz = delta_vertical_velocity * 10240 - d_height;
    if(true_dz <= 0) {
      true_dz *= 0.0025;
      auto const landing{evaluate_landing(readings.airspeed, true_dz, environment.surface(readings.position.position[0], readings.position.position[1]), on_carrier, death)};
      if(landing.grade) {
        result.landing = landing;
        landed = true;
        if(landing.death) request_death(landing.death);
        if(landing.award && environment.landing_award) environment.landing_award();
      }
    }
  }
  if(!landed) {
    z += dz;
  } else if(dz > 0) {
    events.push_back(flight_event::takeoff);
    if(on_carrier && environment.leave_carrier) environment.leave_carrier(*this);
    landed = false;
    on_carrier = false;
  }
  readings.position.position = {integer(x), integer(y), math::add(integer(z), 2304)};
  if(environment.place) readings.position = environment.place(readings.position);
  readings.angles = trig.angles(readings.position.rotation);
  readings.climb = integer(vertical_velocity);
  readings.airspeed = integer(forward_velocity * 0.002297794117647059);
  readings.sidespeed = integer(lateral_velocity * 0.002297794117647059);
  if((power.faults & 0x300000) == 0x300000) {
    if(power.rotor_sound != 2) {
      events.push_back(flight_event::stop_rotor);
      events.push_back(flight_event::windmilling_rotor);
      power.rotor_sound = 2;
    }
  } else if(!(power.faults & 0x300000) && power.rotor_sound != 1) {
    events.push_back(flight_event::stop_rotor);
    events.push_back(flight_event::powered_rotor);
  }
  for(unsigned int side{0}; side != 2; ++side) {
    if(!(power.faults & (0x100000u << side))) {
      readings.engines[side] = integer((delta_vertical_velocity + 1) * 50);
      readings.engines[side + 2] = side ? power.right_temperature : power.left_temperature;
      readings.engines[side + 4] = integer(power.engine_rpm);
    } else {
      if(readings.engines[side] > 0) readings.engines[side] -= 1;
      if(readings.engines[side + 2] > 0) readings.engines[side + 2] -= 5;
      if(readings.engines[side + 4] > 0) readings.engines[side + 4] -= 1;
    }
  }
  readings.torque = integer(std::max(readings.engines[0], readings.engines[1]));
  readings.fuel = integer(power.fuel);
  readings.engines[6] = integer(power.rotor_rpm);
  readings.engines[7] = std::clamp(integer((delta_vertical_velocity + 1) * 50), 0, 100);
  last_frame_ground_height = height;
  return result;
}
void arcade_flight::update_inputs(std::array<int32_t, 4> const &input) {
  for(size_t i{0}; i != 4; ++i) controls.values[i] = input[i] * 4.656612875245797e-10;
  controls.values[1] = -controls.values[1];
  delta_throttle = controls.values[0] * 0.3333333333333333;
  delta_roll = controls.values[3] * 1.5;
  delta_pitch = controls.values[2] * std::abs(controls.values[2]);
  delta_heading = controls.values[1] * std::abs(controls.values[1]);
  controls.changed = controls.values != controls.previous;
  if(controls.changed) controls.previous = controls.values;
}
void arcade_flight::update_rates(double const dt, flight_events &events) {
  heading = wrap(heading);
  pitch = wrap(pitch);
  roll = wrap(roll);
  if(pitch > half_pi && pitch < three_half_pi && roll > half_pi && roll < three_half_pi) {
    pitch = wrap(pi - pitch);
    heading = wrap(heading + pi);
    roll += pi;
    roll = roll >= tau ? roll - tau : roll < 0 ? roll + tau : roll;
  }
  if(((power.faults & 0x400000) && !(power.faults & 0x100000)) || ((power.faults & 0x800000) && !(power.faults & 0x200000))) {
    if(!invulnerable) {
      if(!power.overheat_message) {
        events.push_back(flight_event::engine_overheat);
        power.overheat_message = true;
      }
      if(power.faults & 0x400000) {
        power.left_temperature += dt * 20;
        if(power.left_temperature > 950) {
          power.faults |= 0x100000;
          events.push_back(flight_event::left_engine_failed);
        }
      }
      if(power.faults & 0x800000) {
        power.right_temperature += dt * 20;
        if(power.right_temperature > 950) {
          power.faults |= 0x200000;
          events.push_back(flight_event::right_engine_failed);
        }
      }
    }
  } else {
    power.left_temperature = power.right_temperature = 800;
  }
  power.left_temperature = std::clamp(power.left_temperature, 0.0, 1000.0);
  power.right_temperature = std::clamp(power.right_temperature, 0.0, 1000.0);
}
void arcade_flight::update_motion(double const dt, bool const absolute_throttle, unsigned int const input_type, int32_t const throttle, flight_events &events) {
  pitch = delta_pitch * dt + pitch;
  roll = delta_roll * 2 * dt + roll;
  if(pitch > pi) pitch -= tau;
  if(roll > pi) roll -= tau;
  pitch = std::clamp(pitch, -0.2617993877991666, 0.2617993877991666);
  if(std::abs(delta_pitch) < 0.001) pitch -= pitch * dt;
  if(std::abs(delta_roll) < 0.001) roll -= roll * dt * 2;
  roll = std::clamp(roll, -1.047197551196667, 1.047197551196667);
  lateral_ss = roll * 85 * 256;
  delta_forward_velocity = -(pitch * 3) * 80 * 256;
  lateral_vmax = std::max(0.0, (1 - std::abs(forward_velocity) * 3.90625e-5) * 19456);
  lateral_velocity = std::clamp(lateral_velocity, -lateral_vmax, lateral_vmax);
  max_heading = std::max(0.0, (43520 - forward_velocity) * 2.2977941176470588e-5 * half_pi);
  delta_heading = std::clamp(delta_heading * half_pi, -max_heading, max_heading);
  if(forward_velocity >= 2160) {
    double const turn{forward_velocity >= 25920 ? forward_velocity * 0.0023148148148148147 * -0.388 + 83.33 : forward_velocity * 0.0023148148148148147 - 5};
    delta_heading = turn * 0.01 * (roll * 1.9098593171026184) * 1.5 + delta_heading;
  }
  bool const both_failed{(power.faults & 0x300000) == 0x300000};
  if(!both_failed) delta_vertical_velocity = !absolute_throttle || input_type == 1 ? delta_throttle + delta_vertical_velocity : -(static_cast<uint32_t>(throttle) * 4.656612875245797e-10 - 1);
  if(std::abs(delta_vertical_velocity) < 0.1) delta_vertical_velocity = 0;
  if(!landed) {
    forward_velocity = std::clamp(delta_forward_velocity * dt + forward_velocity, -19456.0, 65280.0);
    heading = delta_heading * dt + heading;
    lateral_velocity = (lateral_ss - lateral_velocity) * dt + lateral_velocity;
    // This integer cast is present in the original's low-speed damping test.
    if(std::abs(static_cast<int>(pitch)) < 0.0001 && std::abs(forward_velocity) < 1280) forward_velocity *= 0.95;
  } else {
    delta_heading *= 0.25;
    forward_velocity = forward_velocity * dt * 0.5;
    lateral_velocity = lateral_velocity * dt * 0.5;
    roll = delta_roll = controls.values[3] = pitch = delta_pitch = controls.values[2] = 0;
    if(delta_vertical_velocity < 0) delta_vertical_velocity = 0;
  }
  delta_vertical_velocity = std::clamp(delta_vertical_velocity, -1.0, 1.0);
  vertical_velocity = z < 2304000 || delta_vertical_velocity < 0 ? delta_vertical_velocity * 10240 : (delta_vertical_velocity * 10240) * ((2560000 - z) * 3.90625e-6);
  vertical_velocity = std::clamp(vertical_velocity, -10240.0, (power.faults & 0x300000) ? 5120.0 : 10240.0);
  if(!both_failed) {
    power.rotor_rpm = power.engine_rpm = 100;
  } else {
    if(power.engine_rpm > 0) power.engine_rpm -= 1;
    if(power.rotor_rpm > 0) power.rotor_rpm -= 1;
    delta_vertical_velocity = landed ? 0 : readings.airspeed * pitch * 2 * 0.006666666666666666 - 1;
    readings.engines[0] = readings.engines[1] = 0;
    power.engine_rpm = std::clamp(power.engine_rpm, 0.0, 110.0);
  }
  power.rotor_rpm = std::clamp(power.rotor_rpm, 0.0, 110.0);
  consume_fuel(dt, events);
  update_hover(dt, events);
}
void arcade_flight::consume_fuel(double const dt, flight_events &events) {
  if(infinite_fuel || invulnerable) {
    power.fuel = 3000;
    return;
  }
  if(power.fuel <= 0) {
    if((power.faults & 0x300000) != 0x300000) events.push_back(flight_event::fuel_exhausted);
    power.endurance = power.fuel = 0;
    power.faults |= 0x300000;
    return;
  }
  power.weight_factor = power.weight * 5e-5;
  power.fuel_consumption = power.weight * 5e-5 + (power.maximum_fuel_consumption * 0.007692307692307693 * delta_vertical_velocity + 0.4);
  power.fuel_rate = readings.airspeed < 61 ? power.fuel_consumption * 0.7 - (power.fuel_consumption * 0.003333333333333333) * readings.airspeed : power.fuel_consumption * 3 * 0.05555555555555555 + (power.fuel_consumption * 0.005555555555555555) * readings.airspeed;
  power.fuel -= power.fuel_rate * dt;
  power.endurance = power.fuel / power.fuel_rate;
  power.weight -= power.fuel_rate * dt;
  if(power.endurance < 360) {
    power.faults |= 0x400;
  } else {
    power.faults &= ~0x400u;
  }
}
void arcade_flight::update_hover(double const dt, flight_events &events) {
  if(hover.active != hover.previous) {
    hover.previous = hover.active;
    events.push_back(hover.active ? flight_event::hover_on : flight_event::hover_off);
  }
  if(std::abs(static_cast<int>(forward_velocity)) >= 4321) {
    hover.active = false;
    return;
  }
  if(!hover.active || controls.changed) return;
  if(std::abs(pitch) < 0.0001) {
    if(std::abs(forward_velocity) > 43.2) forward_velocity *= 0.85;
    if(std::abs(lateral_velocity) > 43.2) lateral_velocity *= 0.85;
  }
  if(delta_vertical_velocity < 0) {
    delta_vertical_velocity += dt * 0.3;
  } else if(delta_vertical_velocity > 0) delta_vertical_velocity -= dt * 0.3;
}
void arcade_flight::request_death(unsigned int const cause) {
  if(death || invulnerable || (no_crashes && (cause == 2 || cause == 3 || cause == 12))) return;
  death = cause;
  if(died) died();
}
void arcade_flight::toggle_hover() {
  hover.active = std::abs(static_cast<int>(forward_velocity)) < 4320 && !hover.active;
}
} // namespace apache::simulation
