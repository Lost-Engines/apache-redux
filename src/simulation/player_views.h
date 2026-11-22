#pragma once
#include "camera_geometry.h"
#include "player_targeting.h"
#include "view_modes.h"

namespace apache::simulation {
struct view_subject {
  math::pose position;
  uint32_t model{0};
  int32_t radius{0};
};
struct view_target {
  math::vector3 position{};
  int32_t height{0};
};
struct view_sources {
  view_subject player;
  std::optional<view_subject> drone;
  std::optional<view_target> target;
  std::function<std::optional<view_subject>(size_t)> weapon;
  std::function<std::optional<view_target>(size_t)> weapon_target;
  uint32_t faults{0};
};
struct view_frame {
  math::pose camera;
  bool sight{false};
  bool infrared{false};
  bool finished{false};
  int32_t fade{0};
};
// View selection and camera memory are independent of drawing and simulation time.
class player_views {
  math::trigonometry const &trig;
  std::function<int32_t(int32_t, int32_t)> height;
  player_targeting &targeting;
  camera_geometry geometry;
  void remember(math::pose const &camera);
  void leave(unsigned int mode);
  void enter(unsigned int mode, view_sources const &sources, uint32_t &random_seed);
public:
  view_modes modes;
  optical_devices optics;
  view_context context;
  orbit_camera tracking;
  orbit_camera spectator;
  orbit_camera crash;
  look_camera helmet;
  look_camera goggles;
  math::pose spectator_pose;
  math::vector3 remote_position{};
  math::vector3 crash_position{};
  unsigned int helmet_previous{0};
  unsigned int goggles_previous{0};
  std::array<bool, 2> helmet_targeting{};
  std::array<bool, 2> goggles_targeting{};
  bool air_crash{false};
  std::function<void(bool)> crashed;
  player_views(resources::executable_data const &executable, math::trigonometry const &trig, std::function<int32_t(int32_t, int32_t)> height, player_targeting &targeting, unsigned int control_mode = 2);
  bool select(unsigned int mode, view_sources const &sources, uint32_t &random_seed);
  bool select_optics(view_sources const &sources, uint32_t &random_seed);
  void removed(size_t round);
  view_frame update(view_sources const &sources, camera_input const &input, uint32_t &random_seed);
};
} // namespace apache::simulation
