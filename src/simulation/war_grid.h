#pragma once
#include "target_search.h"

namespace apache::simulation {
struct war_sector {
  shared_target radar;
  std::array<int32_t, 9> floor;
  contact_bucket air;
  contact_bucket ground;
  war_sector() {
    floor.fill(INT32_MAX);
  }
  bool empty() const {
    return !radar && !air.count && !ground.count;
  }
};
struct war_contact {
  shared_target target;
  uint32_t flags{0};
  int32_t classification{0};
  int32_t task{0};
  bool player{false};
};
bool radar_model(uint32_t model);
int32_t radar_range(uint32_t model);
int32_t radar_floor(uint32_t model);
class war_grid {
  std::array<std::array<std::unique_ptr<war_sector>, 2>, 4096> cells;
  std::array<size_t, 2> used{};
  std::array<size_t, 2> maximum{};
  war_sector *ensure(int32_t x, int32_t y, unsigned int side);
  void radar_patch(unsigned int side, shared_target const &target, uint32_t model, std::array<int32_t, 2> const &source, std::array<int32_t, 2> const &destination, std::function<int32_t(int32_t, int32_t)> const &height);
public:
  war_sector const *sector(int32_t x, int32_t y, unsigned int side) const;
  bool insert(int32_t x, int32_t y, war_contact const &contact);
  void remove(int32_t x, int32_t y, war_contact const &contact, math::vector3 const &position, std::function<std::vector<war_contact>(int32_t, int32_t)> const &rescan);
  void insert_radar(int32_t x, int32_t y, unsigned int side, shared_target const &target, uint32_t model, math::vector3 const &position, std::function<int32_t(int32_t, int32_t)> const &height);
  shared_target radar(int32_t x, int32_t y, unsigned int side) const;
  int32_t floor(int32_t x, int32_t y, unsigned int side) const;
  int32_t status(int32_t sector_x, int32_t sector_y, unsigned int side, std::function<int32_t(target_reference const &)> const &target_status) const;
  std::array<size_t, 2> const &counts() const {
    return used;
  }
  std::array<size_t, 2> const &peak_counts() const {
    return maximum;
  }
};
} // namespace apache::simulation
