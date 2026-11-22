#pragma once
#include "projectile_dynamics.h"

namespace apache::simulation {
struct projectile_launch_state {
  unsigned int weapon{0};
  projectile_state body;
  uint32_t random{0};
  int32_t launched_at{0};
  int32_t impact_at{0};
  int32_t simple_value{0};
  math::vector3 impact{};
  projectile_launch_state() {
    body.seeker.flags = 0;
    body.motion.flags = 0;
  }
};
struct projectile_launch_inputs {
  math::pose pose;
  movement_target source;
  bool has_target{false};
  int32_t simple_value{0};
};
void construct_projectile(projectile_launch_state &round, unsigned int id, weapon_info const &weapon, projectile_launch_inputs const &input, math::trigonometry const &trig, std::function<int32_t(int32_t, int32_t)> const &height, uint32_t &global_random, uint32_t &weapon_random);
render::model_instance projectile_model(projectile_launch_state const &round, weapon_info const &weapon, resources::model_archive const &models);
void launch_projectile(projectile_launch_state &round, int32_t now);
} // namespace apache::simulation
