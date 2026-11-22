#pragma once
#include "actor_movement.h"
#include "player_weapons.h"

namespace apache::simulation {
std::array<uint32_t, 24> player_pod_models(std::array<player_pylon, 9> const &pylons, resources::model_archive const &models);
class player_weapon_mounts {
  math::trigonometry const &trig;
  std::optional<int32_t> gun_time, hydra_time;
public:
  int32_t hydra_elevation{0};
  bool hydras_locked{false};
  unsigned int gun_status{0};
  math::vector3 intercept{};
  explicit player_weapon_mounts(math::trigonometry const &angles) : trig{angles} {}
  void update_hydra(int32_t now, int32_t delta, int32_t input, math::pose const &body, std::optional<math::vector3> target, int32_t target_height);
  unsigned int position_gun(int32_t now, unsigned int weapon, render::model_instance &body, std::optional<movement_target> const &target, math::vector3 const &helmet);
  math::pose launch_pose(unsigned int weapon, size_t slot, int32_t ammunition, render::model_instance &body) const;
  static render::model_instance *pylon(render::model_instance &body, size_t slot);
  static void set_gun_flash(render::model_instance &body, resources::model_archive const &models, bool active);
  static void remove_missile(render::model_instance &body, size_t slot, int32_t ammunition);
};
} // namespace apache::simulation
