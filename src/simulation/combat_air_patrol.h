#pragma once
#include "actor_targeting.h"

namespace apache::simulation {
struct patrol_contacts {
  std::array<std::vector<shared_target>, 2> visible;
  std::array<std::array<shared_target, 20>, 2> assigned{};
  int32_t last_scan{0};
  void insert(unsigned int side, shared_target target);
};
struct patrol_sector {
  std::array<bool, 2> present{};
  std::array<std::span<shared_target const>, 2> air;
  int32_t owner{0};
};
struct patrol_radar_callbacks {
  std::function<math::vector3(target_reference const &)> position;
  std::function<int32_t(math::vector3 const &, unsigned int)> floor;
  std::function<shared_target(math::vector3 const &, unsigned int)> radar;
  std::function<int32_t(target_reference const &)> status;
};
void scan_patrol_contacts(patrol_contacts &contacts, int32_t now, std::function<patrol_sector(int32_t, int32_t)> const &sector, std::function<int32_t(int32_t, int32_t, unsigned int)> const &radar_status, patrol_radar_callbacks const &radar);
shared_target select_patrol_target(patrol_contacts &contacts, actor_state const &actor, int32_t attention, flight_path const &path, unsigned int slot, int32_t now, math::trigonometry const &trig, std::function<void()> const &scan, std::function<math::vector3(target_reference const &)> const &position);
struct patrol_acquisition_callbacks {
  patrol_radar_callbacks radar;
  std::function<void()> basic;
  std::function<void()> released;
  std::function<shared_target()> select;
  std::function<int32_t()> weapon_range;
  std::function<int32_t(target_reference const &)> actor_class;
  std::function<shared_target(math::vector3 const &)> fixed;
};
void acquire_patrol_target(actor_state &actor, actor_armament &armament, shared_target const &assigned, int32_t now, patrol_acquisition_callbacks const &callbacks);
} // namespace apache::simulation
