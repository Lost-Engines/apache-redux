#pragma once
#include <functional>
#include <span>
#include "resources/missions.h"

namespace apache::simulation {
struct route_events {
  std::function<bool(uint16_t)> key_event;
  std::function<void(size_t)> achieved;
  std::function<void(unsigned int)> sound;
  std::function<unsigned int(int32_t, int32_t)> surface;
};
class player_route {
public:
  int32_t last_distance{INT32_MAX};
  bool landing_message{false};
  void cycle(std::span<resources::route_waypoint const> route, std::optional<size_t> &current, bool forward, uint16_t key, unsigned int control_mode, route_events const &events = {});
  void update(std::span<resources::route_waypoint const> route, std::optional<size_t> &current, math::vector3 const &position, route_events const &events = {});
};
} // namespace apache::simulation
