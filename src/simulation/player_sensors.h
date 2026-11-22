#pragma once
#include "mission_targets.h"
#include "player_targeting.h"
#include "render/projection.h"

namespace apache::simulation {
struct sensor_contact {
  target_reference target;
  std::array<int32_t, 2> sector{};
  math::vector3 position{};
};
struct radar_return {
  target_reference target;
  math::vector3 display{};
  int32_t priority{0};
};
class player_sensors {
  resources::terrain const &terrain;
  resources::model_archive const &models;
  mission_scenery const &scenery;
  render::mobile_scene const &mobiles;
  mission_targets const &targets;
  math::trigonometry const &trig;
  std::array<std::array<std::vector<size_t>, 4096>, 2> sectors;
  void insert(radar_return const &contact, math::pose const &body, uint32_t heading);
  uint32_t model(target_reference const &target) const;
public:
  player_targeting state;
  std::vector<sensor_contact> ground;
  std::vector<sensor_contact> mobile;
  std::vector<sensor_contact> missiles;
  std::vector<radar_return> ground_radar;
  std::vector<radar_return> air_radar;
  std::vector<radar_return> tactical;
  std::vector<radar_return> ranked;
  target_reference optical;
  math::vector3 current_display{};
  std::optional<std::array<int32_t, 2>> optical_symbol;
  std::function<void(std::string_view)> message;
  player_sensors(resources::terrain const &terrain, resources::model_archive const &models, mission_scenery const &scenery, render::mobile_scene const &mobiles, mission_targets const &targets, math::trigonometry const &trig, unsigned int game_mode = 0);
  void scan_contacts(math::pose const &body, std::optional<uint32_t> player);
  void scan_radars(math::pose const &body, uint32_t heading, uint32_t faults);
  void update(math::pose const &body, uint32_t heading, std::optional<uint32_t> player, unsigned int control_mode, uint32_t faults);
  void pick_optical(math::pose const &view, uint32_t heading, render::projection const &projection, unsigned int control_mode, uint32_t faults);
};
} // namespace apache::simulation
