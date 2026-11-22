#pragma once
#include <array>
#include <cstdint>
#include <string_view>

namespace apache::simulation {
struct weapon_info {
  int32_t movement_class{0};
  int32_t guidance{0};
  uint8_t flags{0};
  uint8_t alternate_penetration{0};
  uint8_t burst_size{0};
  uint8_t weapon_role{0};
  uint8_t damage{0};
  uint8_t decoy_denominator{0};
  int32_t motor_acceleration{0};
  int32_t motor_ticks{0};
  int32_t burst_pause_ticks{0};
  int32_t guidance_start_distance{0};
  int32_t blast_radius{0};
  int32_t minimum_pitch{0};
  int32_t maximum_pitch{0};
  int32_t heading_limit{0};
  int32_t penetration{0};
  int32_t minimum_range{0};
  int32_t maximum_range{0};
  int32_t minimum_speed{0};
  int32_t maximum_speed{0};
  int32_t launch_speed{0};
  int32_t model{0};
  int32_t target_box_half_size{0};
  int32_t fire_interval_ticks{0};
  int32_t lateral_acceleration{0};
  int32_t tracking_or_dispersion{0};
  int32_t unresolved_5c{0};
  double drag_coefficient{0};
  std::string_view name;
};
std::array<weapon_info, 55> weapon_definitions();
} // namespace apache::simulation
