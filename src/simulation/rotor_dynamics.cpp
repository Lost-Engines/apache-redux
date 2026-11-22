#include "rotor_dynamics.h"
#include <algorithm>
#include <cmath>
#include "math/fixed.h"

namespace apache::simulation {
namespace {
double constexpr pi{3.14159265359}, tau{6.28318530718}, half_pi{1.570796326795}, three_half_pi{4.712388980385};
double constexpr radians_per_degree{0.01745329251994444};
double wrap(double const angle) {
  return angle >= tau ? angle - tau : angle < 0 ? angle + tau : angle;
}
}
void rotor_dynamics::update_rates(double const dt, double const ground, bool const on_carrier, bool const landed, bool const absolute_throttle, unsigned int const input_type, int32_t const throttle, flight_events &events) {
  double const Jc{controls.values[0]};
  double const Jr{controls.values[1]};
  double const Jq{controls.values[2]};
  double const Jp{controls.values[3]};
  cos_theta = std::cos(theta);
  sin_theta = std::sin(theta);
  tan_theta = std::tan(theta);
  cos_phi = std::cos(phi);
  sin_phi = std::sin(phi);
  power.collective = std::clamp(!absolute_throttle || input_type == 1 ? Jc * 50 * dt + power.collective : 130 - static_cast<uint32_t>(throttle) * 2.328306437080797e-10 * 130, 1.0, 130.0);
  current_landing_pitch = power.engine_torque > 65 ? 0 : power.engine_torque < 50 ? 0.08975979010257143 : power.engine_torque * -0.08975979010257143 * 0.06666666666666667 + 0.3889590904444762;
  if(!landed && !on_carrier) {
    double const factor{u > 170 ? 0 : (1 - std::abs(u) * 0.0058823529411764705) * std::abs(std::sin(theta))};
    qmax = (1 - factor) * 70 * (1 - std::abs(u) * 0.001838235294117647) * radians_per_degree;
    qdem = Jq * qmax;
    if(qdem < 0) qdem = (1 - u * 0.0021) * qdem;
    q = (qdem - q) * 0.25 + q;
  } else {
    if(theta > 0.1072130826225159 && theta < 6.013905936872286) {
      events.push_back(flight_event::hard_impact);
      theta = 0;
    } else {
      if(theta >= 6.013905936872286 && theta < 6.193425517077428) {
        theta = dt * 0.314159265359 + theta;
      } else if(theta >= 6.193425517077428) {
        theta = dt * 0.1570796326795 + theta;
      } else if(theta >= 0 && theta < 0.1072130826225159) {
        theta = (current_landing_pitch - theta) * dt * 6 + theta;
      } else {
        theta = theta - (current_landing_pitch - theta) * dt * 4;
      }
      if(theta > tau || theta < 0) theta = 0;
    }
    p = q = qdem = pdem = r = 0;
  }
  if(z > ground) {
    pmax = u * 0.0064 + roll_d;
    pdem = Jp * pmax;
    p = (pdem - p) * 0.5 + p;
  } else {
    phi = 0;
    p = 0;
  }
  if(u > 100) {
    rmax = 0.08726646259972222;
  } else {
    rmax = (1 - std::abs(sin_theta) * 0.5) * (95 - std::abs(u) * 0.95) * radians_per_degree;
    if(z <= ground) rmax = (power.faults & 0x300000) ? 0 : rmax * 0.25;
  }
  rdem = -Jr * rmax;
  r = (rdem - r) * 0.5 + r;
  turnrate = u > 32 ? (sin_phi * 2048 * pi) / (u * 180) : u > 17 ? (u * 2.133 - 36.27) * sin_phi * pi * 0.005555555555555555 : 0;
  if(theta >= half_pi && theta <= three_half_pi) turnrate = -turnrate;
  double const combined{r * cos_phi + q * sin_phi};
  psidot = combined / cos_theta;
  phidot = p + combined * tan_theta;
  thetadot = std::abs(cos_phi) * q * cos_phi - r * sin_phi;
  phi = wrap(phidot * dt + phi);
  psi = wrap((turnrate * dt + psidot * dt) + psi);
  theta = wrap(thetadot * dt + theta);
  if(theta > half_pi && theta < three_half_pi && phi > half_pi && phi < three_half_pi) {
    theta = wrap(pi - theta);
    phi = wrap(phi + pi);
    psi = wrap(psi + pi);
  }
  if(std::abs(qdem) < 0.001 && std::abs(pdem) < 0.001 && std::abs(rdem) < 0.001 && z > ground) {
    if(theta < half_pi) {
      theta -= theta * 0.125 * dt;
    } else if(theta <= three_half_pi) {
      theta = (three_half_pi - theta) * 0.125 * dt + theta;
    } else {
      theta = (tau - theta) * 0.125 * dt + theta;
    }
    if(theta < 0.01) {
      theta -= theta * dt;
      if(theta < 0.001) theta = 0;
    } else if(theta >= 6.27318530718) {
      theta = (tau - theta) * dt + theta;
      if(theta > 6.28218530718) theta = 0;
    }
  }
  my_roll = phi <= pi ? phi : phi - tau;
  roll_a = u >= 100 ? 0.3 : 1.8 - std::abs(u) * 0.00015 * std::abs(u);
  roll_a = (1 - std::abs(Jr) * 0.75) * roll_a;
  roll_decay_ss = roll_a * std::sin(my_roll);
  roll_decay = (roll_decay_ss - roll_decay) * dt * roll_b + roll_decay;
  phi -= roll_decay * roll_c * dt;
}
void rotor_dynamics::update_aerodynamics(double const dt, int32_t const delta_time, double const ground, bool const on_carrier, unsigned int const random_sample, flight_events &events) {
  double const Jq{controls.values[2]};
  double const torque{power.rotor_torque};
  double const weight{power.weight};
  int32_t const ticks{math::multiply_q31(delta_time, 200)};
  cos_theta = std::cos(theta);
  sin_theta = std::sin(theta);
  tan_theta = std::tan(theta);
  cos_phi = std::cos(phi);
  sin_phi = std::sin(phi);
  rho = z > 10000 ? 0.942 - z * 0.00002036 : 1 - z * 0.0000261;
  kge = z - 48 <= ground ? std::min(1.0, u * 0.00244 + ((z - ground) * 0.122) * 0.020833333333333332 + 0.878) : 1;
  kdu = u < 0 ? 0.25 - u * 0.01 : zdot < 0 && theta > 0 && theta < half_pi ? sin_theta * 2 + 0.06934 : 0.06934;
  double const squared_speed{u * u + v * v};
  if(u > 102) {
    beta = std::max(0.0, (400 - std::abs(u)) * 0.003333333333333333);
    Tz = squared_speed * 0.000865 + 55;
  } else {
    Tz = 90 - squared_speed * 0.0025;
    beta = std::abs(u) * 0.00980392156862745;
  }
  Tzw = static_cast<int32_t>((weight / power.maximum_weight * Tz) * (kge / rho));
  if(power.faults & 0x300000) Tzw = static_cast<int32_t>(Tzw * 1.4);
  if(cos_phi < 0) Tzw = static_cast<int32_t>(Tzw * (1 - cos_phi * 3));
  double const excess{torque - Tzw};
  zdotT = static_cast<int32_t>(std::abs(cos_theta * cos_phi) * static_cast<int32_t>(excess * eta));
  zdotss = (u * sin_theta + std::abs(cos_theta) * zdotT) - v * sin_phi;
  zdot = (zdotss - zdot) * 0.25 + zdot;
  zdotTmax = static_cast<int32_t>((cos_theta * cos_phi - Tzw) * eta);
  if(z > ground) {
    if(!on_carrier) {
      Kt = std::abs(u) * 0.1938 * ((excess * beta) / Tzw + 1);
      if(torque < Tzw) Kt *= 4;
      double const effective_torque{torque >= 100 ? torque * 0.16666666666666666 + 83.33 : torque};
      udot = (((std::abs(cos_theta * cos_phi) * (effective_torque - Tzw) * Kt - weight * sin_theta) - std::abs(sin_theta) * std::abs(u) * u * kdu) * 32) / weight;
      if(u < 0 && udot > 0) udot *= 4;
      udot = v * r + udot;
      u = std::clamp(udot * 1.5 * dt + u, -100.0, 395.0);
      if(u <= 340) {
        if(max_speed_timer > 0) max_speed_timer -= ticks;
        if(max_speed_message) {
          events.push_back(flight_event::overspeed_off);
          max_speed_message = false;
        }
      } else {
        if(u > 394) events.push_back(flight_event::overspeed_fatal);
        max_speed_timer += ticks;
        if(max_speed_timer > 2000) events.push_back(flight_event::overspeed_fatal);
        if(!max_speed_message) {
          events.push_back(flight_event::overspeed_on);
          max_speed_message = true;
        }
        if(power.warning_timer > 50) {
          events.push_back(flight_event::warning_sound);
          power.warning_timer = 0;
        }
        power.warning_timer += ticks;
      }
    } else {
      u = v = udot = vdot = 0;
    }
  } else {
    if(zdot < 0) zdot = 0;
    udot = -Jq * (torque - 1) * 0.25 - (std::abs(u) * u * kdu) / (std::abs(Jq) * 90 + 10);
    v *= 0.25;
    u = udot * dt + u;
    if(v <= 0.1) v = 0;
    u = (power.faults & 0x300000) ? u * 0.75 : std::clamp(u, -9.0, 102.0);
    z = ground;
  }
  if(z > ground && !on_carrier) {
    abs_v = std::abs(v);
    Kdv = std::abs(u) * 0.15625 + 1;
    vdot = (g * sin_phi + (-(Kdv * abs_v * v) * 32) / weight) - u * r;
    if(std::abs(vdot) > 1000) {
      vdot = 0;
      events.push_back(flight_event::excessive_sideways_acceleration);
    }
    v = std::clamp(vdot * dt + v, -100.0, 100.0);
  }
  eta = torque >= Tzw ? (u >= 102 ? 1.198 - u * 0.003 : 1 - u * 0.00105) : (u >= 102 ? 1.013 - u * 0.0025335 : 1 - u * 0.002402);
  if(random_sample % 100 < 15 && !wind_gust) {
    wind_gust = true;
    wind_strength = math::add(wind_strength, wind_strength);
  } else if(wind_gust) {
    wind_gust = false;
    wind_strength /= 2;
  }
}
} // namespace apache::simulation
