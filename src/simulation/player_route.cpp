#include "player_route.h"

namespace apache::simulation {
void player_route::cycle(std::span<resources::route_waypoint const> const route, std::optional<size_t> &current, bool const forward, uint16_t const key, unsigned int const mode, route_events const &events) {
  if(mode == 1 || (!mode && events.key_event && events.key_event(key)) || !current || *current >= route.size()) return;
  *current = (*current + (forward ? 1 : route.size() - 1)) % route.size();
}
void player_route::update(std::span<resources::route_waypoint const> const route, std::optional<size_t> &current, math::vector3 const &position, route_events const &events) {
  if(!current || *current >= route.size()) return;
  auto const &point{route[*current]};
  int32_t const x{math::subtract(point.position[0], position[0])};
  int32_t const y{math::subtract(point.position[1], position[1])};
  int32_t const distance{std::bit_cast<int32_t>(math::integer_sqrt(math::product_bits(x, x) + math::product_bits(y, y)))};
  if(distance >= 0x14a000) return;
  if(distance <= last_distance) {
    last_distance = distance;
    return;
  }
  last_distance = INT32_MAX;
  if(*current + 1 < route.size()) {
    if(events.achieved) events.achieved(*current);
    if(point.type == 6 && events.sound) events.sound(69);
    ++*current;
  } else if(!landing_message) {
    if((!events.surface || events.surface(point.position[0], point.position[1]) != 3) && events.sound) events.sound(26);
    landing_message = true;
  }
}
} // namespace apache::simulation
