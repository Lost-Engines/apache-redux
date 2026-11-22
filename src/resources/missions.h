#pragma once
#include <array>
#include <map>
#include <optional>
#include "math/geometry.h"
#include "executable.h"

namespace apache::resources {
struct tagged_section {
  std::string tag;
  std::vector<std::string> lines;
};
std::vector<tagged_section> tagged_text(std::string_view text);
struct mission_briefing {
  int32_t id{0}, flow{0}, zone{0}, level{0};
  std::string title;
  std::vector<tagged_section> text;
};
std::vector<mission_briefing> read_mission_briefings(std::span<uint8_t const> bytes);
struct route_waypoint {
  int32_t type{0};
  math::vector3 position{0, 0, 0};
  int32_t time{0}, speed{0}, interval{0}, distance{0};
};
struct mission_actor {
  uint32_t model{34}, formation{0};
  int32_t start_time{0};
  uint32_t start_waypoint{0}, conditions{0};
  std::optional<std::array<int32_t, 7>> initial_state;
  std::vector<route_waypoint> waypoints;
};
struct mission_task {
  int32_t side{0}, type{0}, battle_cell{0};
  bool user{false};
  std::vector<mission_actor> actors;
};
struct mission_definition {
  std::vector<mission_task> tasks;
  std::map<std::string, std::vector<std::vector<int32_t>>, std::less<>> settings;
  int32_t wind_strength{0}, weather{0}, time_of_day{0}, laser_timeout{0};
  uint32_t wind_direction{0};
  std::array<int32_t, 9> weapons{};
  int32_t gun{0}, failure_count{0}, success_count{0}, outstanding_count{0};
  bool infinite_weapons{false};
  std::vector<std::array<int32_t, 2>> destroy_markers;
};
mission_definition read_mission_definition(std::string_view text);
struct formation_member {
  math::vector3 offset;
  uint32_t model;
};
std::vector<std::vector<formation_member>> read_formations(executable_data const &executable);
} // namespace apache::resources
