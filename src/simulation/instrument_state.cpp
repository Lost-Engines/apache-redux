#include "instrument_state.h"
#include <algorithm>
#include <cstdlib>
#include "math/geometry.h"

namespace apache::simulation {
instrument_state instrument_values(instrument_inputs const &input, math::trigonometry const &trig) {
  instrument_state result;
  result.heading = input.heading;
  result.pitch = input.pitch;
  result.roll = input.roll;
  result.barometric_altitude = math::subtract(input.position[2], 0x900);
  result.clock_time = math::add(input.mission_time, input.game_time) / 200;
  result.radar_altitude = std::max(0, math::subtract(result.barometric_altitude, input.terrain_height));
  result.climb_rate = input.climb_rate;
  if(input.waypoint) {
    int32_t const x{math::subtract(input.waypoint->x, input.position[0])};
    int32_t const y{math::subtract(input.waypoint->y, input.position[1])};
    result.command_heading = trig.arctangent(x, y);
    result.waypoint_letter = input.waypoint->letter;
    result.waypoint_distance = std::bit_cast<int32_t>(math::integer_sqrt(math::product_bits(x, x) + math::product_bits(y, y)));
    int32_t const speed{static_cast<int32_t>(std::clamp(static_cast<double>(input.airspeed) * 1.7, static_cast<double>(INT32_MIN), static_cast<double>(INT32_MAX)))};
    int64_t const seconds{int64_t{result.waypoint_distance / 256} / (std::abs(int64_t{speed}) + 1)};
    result.waypoint_time = std::bit_cast<int32_t>(static_cast<uint32_t>(seconds) * 200u);
  } else {
    result.command_heading = 0xa0000000;
    result.waypoint_letter = 3;
    result.waypoint_distance = 0x802865;
    result.waypoint_time = 0x24b80;
  }
  result.airspeed = input.airspeed;
  result.sidespeed = input.sidespeed;
  result.torque = input.torque;
  return result;
}
} // namespace apache::simulation
