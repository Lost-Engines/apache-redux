#include "flight_systems.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include "math/fixed.h"

namespace apache::simulation {
void powerplant::consume_fuel(double const dt, int32_t const airspeed, bool const infinite, bool const invulnerable, flight_events &events) {
  if(infinite || invulnerable) {
    fuel = 3000;
    return;
  }
  if(fuel <= 0) {
    if(!(faults & 0x300000)) events.push_back(flight_event::fuel_exhausted);
    endurance = fuel = 0;
    faults |= 0x300000;
    return;
  }
  weight_factor = weight / maximum_weight;
  fuel_consumption = (maximum_fuel_consumption * (1.0 / 130) * rotor_torque + 0.3) + weight_factor;
  if(airspeed <= 60) {
    fuel_rate = fuel_consumption * 0.7 - (fuel_consumption * 0.003333333333333333) * airspeed;
  } else {
    fuel_rate = (fuel_consumption * 3) * (1.0 / 18) + (fuel_consumption * 0.005555555555555555) * airspeed;
  }
  fuel -= fuel_rate * dt;
  endurance = fuel / fuel_rate;
  weight -= fuel_rate * dt;
  if(endurance < 360) {
    faults |= 0x400;
  } else {
    faults &= ~0x400u;
  }
}
int32_t powerplant::update_engines(double const dt, double const altitude, int32_t const ground_height, int32_t const vertical_lift, int32_t const maximum_lift) {
  engine_torque = std::clamp(engine_torque, 1.0, 130.0);
  descent_ratio = vertical_lift <= 0 && altitude > ground_height / 256 ? std::abs(static_cast<double>(vertical_lift) / maximum_lift) : 0;
  engine_rpm_target = (faults & 0x300000) == 0x300000 ? 0 : 100 - descent_ratio * 40;
  torque_target = collective * std::max(0.0, engine_rpm - 60) * 0.025;
  if(engine_rpm >= 60) {
    rotor_rpm_target = engine_rpm + descent_ratio * 40;
  } else {
    rotor_rpm_target = descent_ratio * 200 * (0.5 - collective * (1.0 / 130));
    if(rotor_rpm_target < 0 || rotor_rpm < 10) rotor_rpm_target = 0;
  }
  engine_rpm = (engine_rpm_target - engine_rpm) * 2 * dt + engine_rpm;
  left_rpm = engine_rpm;
  right_rpm = engine_rpm;
  if(faults & 0x100000) {
    left_rpm = 1;
    left_temperature = 0;
  }
  if(faults & 0x200000) {
    right_rpm = 1;
    right_temperature = 0;
  }
  engine_torque = (torque_target - engine_torque) * 2 * dt + engine_torque;
  double difference{rotor_rpm_target - rotor_rpm};
  if(rotor_rpm_target <= rotor_rpm) difference *= 0.1;
  rotor_rpm = difference * dt + rotor_rpm;
  rotor_torque = (rotor_rpm * 0.01) * collective;
  double const rate{rotor_rpm * 12000 * 0.01};
  if(!std::isfinite(rate) || rate < INT32_MIN || rate > INT32_MAX) throw std::runtime_error{"Invalid rotor speed"};
  return static_cast<int32_t>(rate);
}
void powerplant::update_temperature(double const dt) {
  temperature = std::clamp((std::trunc(engine_rpm * 8) - temperature) * 0.25 * dt + temperature, 0.0, 1000.0);
  if(!(faults & 0x400000)) left_temperature = temperature;
  if(!(faults & 0x800000)) right_temperature = temperature;
}
void powerplant::update_faults(double const dt, int32_t const delta_time, bool const overtorque_enabled, flight_events &events) {
  using enum flight_event;
  if(((faults & 0x400000) && !(faults & 0x100000)) || ((faults & 0x800000) && !(faults & 0x200000))) {
    if(!overheat_message) {
      events.push_back(engine_overheat);
      overheat_message = true;
    }
    if(faults & 0x400000) {
      left_temperature += dt * 20;
      if(left_temperature > 950) {
        faults |= 0x100000;
        events.push_back(left_engine_failed);
        overheat_message = false;
      }
    }
    if(faults & 0x800000) {
      right_temperature += dt * 20;
      if(right_temperature > 950) {
        faults |= 0x200000;
        events.push_back(right_engine_failed);
        overheat_message = false;
      }
    }
  }
  left_temperature = std::clamp(left_temperature, 0.0, 1000.0);
  right_temperature = std::clamp(right_temperature, 0.0, 1000.0);
  if((faults & 0x300000) == 0x300000) {
    if(rotor_rpm > 20 && rotor_sound != 2) {
      events.push_back(stop_rotor);
      events.push_back(windmilling_rotor);
      rotor_sound = 2;
    }
    if(rotor_rpm <= 20 && rotor_sound) {
      events.push_back(stop_rotor);
      rotor_sound = 0;
    }
    engine_rpm_target = 0;
  } else if(rotor_sound != 1) {
    events.push_back(stop_rotor);
    events.push_back(powered_rotor);
    rotor_sound = 1;
  }
  int32_t const ticks{math::multiply_q31(delta_time, 200)};
  if(overtorque_enabled) {
    if(torque_target <= 100) {
      if(overtorque_message) {
        events.push_back(overtorque_off);
        overtorque_message = false;
      }
      overtorque_timer -= ticks;
    } else {
      left_temperature += dt * 5;
      right_temperature += dt * 5;
      if(!overtorque_message) {
        events.push_back(overtorque_on);
        overtorque_message = true;
      }
      if(warning_timer > 200) {
        events.push_back(warning_sound);
        warning_timer = 0;
      }
      warning_timer += ticks;
      overtorque_timer += ticks * ((torque_target - 100) * 0.03333333333333333);
    }
  }
  overtorque_timer = std::clamp(overtorque_timer, 0.0, 6000.0);
  if(overtorque_timer > 4000) {
    events.push_back(engines_overheated);
    overtorque_timer = 0;
    faults |= 0xc00000;
  }
}
void flight_controls::update(std::array<int32_t, 4> const &input, bool const on_carrier) {
  double constexpr normalise{4.656612875245797e-10};
  values[0] = input[0] * normalise;
  if(on_carrier) {
    values[1] = values[2] = values[3] = 0;
    return;
  }
  for(size_t i{1}; i != values.size(); ++i) values[i] = input[i] * normalise;
  changed = values != previous;
  if(changed) previous = values;
}
void hover_control::toggle(double const forward_speed) {
  active = std::abs(std::trunc(forward_speed)) < 17 ? !active : false;
}
void hover_control::update(double &forward_speed, double &sideways_speed, double const vertical_speed, double const pitch, bool const user_input, double &collective, flight_events &events) {
  if(active != previous) {
    previous = active;
    events.push_back(active ? flight_event::hover_on : flight_event::hover_off);
  }
  if(std::abs(std::trunc(forward_speed)) >= 18) {
    active = false;
    return;
  }
  if(!active || user_input) return;
  if(pitch == 0) {
    if(std::abs(forward_speed) > 0.1) forward_speed *= 0.85;
    if(std::abs(sideways_speed) > 0.1) sideways_speed *= 0.85;
  }
  if(std::trunc(vertical_speed) != 0) collective -= vertical_speed * 0.5;
}
landing_result evaluate_landing(double const forward_speed, double const vertical_speed, unsigned int const surface, bool const on_carrier, unsigned int const previous_death) {
  if(previous_death) return {.death{previous_death}};
  if(!on_carrier && surface >= 1 && surface <= 3) return {6, 12, true, false};
  double const severity{std::abs(forward_speed) * 0.01 - vertical_speed * 0.045454545454545456};
  if(severity > 0.9) return {5, 3, true, false};
  return {severity > 0.8 ? 4u : severity > 0.6 ? 3u : severity > 0.3 ? 2u : 1u, 0, true, true};
}
} // namespace apache::simulation
