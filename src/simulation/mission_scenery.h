#pragma once
#include <array>
#include "combat_update.h"

namespace apache::simulation {
struct gameplay_sector {
  int32_t side{3};
  std::array<std::optional<size_t>, 23> objects;
};
struct gameplay_object {
  unsigned int kind{0};
  ground_defence combat;
  std::optional<size_t> scenery;
};
class mission_scenery {
  std::array<std::vector<size_t>, 4096> base_sectors, extra_sectors;
  void mark_laser_objects();
  void calculate_front();
public:
  std::vector<resources::scenery_placement> base, extra;
  std::array<gameplay_sector, 4096> sectors;
  std::array<uint8_t, 4096> front_distance;
  std::vector<gameplay_object> objects;
  mission_scenery(resources::mission_definition const &mission, std::vector<resources::scenery_placement> scenery, resources::terrain const &terrain, resources::model_archive const &models, unsigned int map);
  static uint32_t model(unsigned int kind, int32_t side, unsigned int map);
  int32_t owner(int32_t x, int32_t y) const;
};
} // namespace apache::simulation
