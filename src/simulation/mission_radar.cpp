#include "mission_radar.h"
#include <stdexcept>

namespace apache::simulation {
void initialise_mission_radars(war_grid &grid, mission_scenery const &scenery, mission_actors const &actors, std::span<shared_target const> const targets, resources::terrain const &terrain) {
  if(targets.size() != actors.units.size()) throw std::runtime_error{"Actor radar references do not match the mission"};
  auto const height{[&](int32_t x, int32_t y){return terrain.approximate_elevation(x, y);}};
  std::array<std::vector<std::pair<unsigned int, size_t>>, 4096> sectors;
  for(unsigned int layer{0}; layer != 2; ++layer) {
    auto const &placements{layer ? scenery.extra : scenery.base};
    for(size_t i{0}; i != placements.size(); ++i) {
      if(radar_model(placements[i].model)) sectors[placements[i].sector].emplace_back(layer, i);
    }
  }
  for(size_t cell{0}; cell != sectors.size(); ++cell) {
    for(auto const &[layer, index] : sectors[cell]) {
      auto const &p{(layer ? scenery.extra : scenery.base)[index]};
      int32_t const x{static_cast<int32_t>(cell / 64)};
      int32_t const y{static_cast<int32_t>(cell % 64)};
      // The original radar initializer adds the packed local coordinates without
      // the scenery loader's factor of 256. Targets retain these cached positions.
      math::vector3 const position{x * 0x400000 + (p.x - x * 0x400000) / 256, y * 0x400000 + (p.y - y * 0x400000) / 256, 0};
      auto target{std::make_shared<target_reference>(target_reference{{target_kind::scenery, layer, static_cast<uint32_t>(index)}, position, 0})};
      grid.insert_radar(x, y, static_cast<unsigned int>(scenery.owner(x, y)), target, p.model, position, height);
    }
  }
  // Coverage is built before initialise_drones_ sets the leaders' positions.
  // Their route chooses the allegiance cell, but their zero pose is the origin
  // used by radar propagation. Later movement does not rebuild that coverage.
  for(auto const &group : actors.groups) {
    if(!group.units.empty() && !group.definition.waypoints.empty()) {
      size_t const leader{group.units.front()};
      auto const &actor{actors.units[leader]};
      if(!radar_model(actor.object.id)) continue;
      auto const &waypoint{group.definition.waypoints.front().position};
      int32_t const x{waypoint[0] >> 22};
      int32_t const y{waypoint[1] >> 22};
      grid.insert_radar(x, y, static_cast<unsigned int>(scenery.owner(x, y)), targets[leader], actor.object.id, {}, height);
    }
  }
}
} // namespace apache::simulation
