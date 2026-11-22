#pragma once
#include "projectiles.h"

namespace apache::simulation {
enum class remote_fire_kind {
  unguided,
  guided,
  simple,
  chaff,
  flare
};
struct remote_fire_input {
  unsigned int weapon{0};
  uint32_t seed{0};
  math::pose aircraft;
  math::vector3 gun_direction{};
  target_reference source;
  movement_target movement;
  std::optional<target_reference> target;
};
struct remote_shot {
  remote_fire_kind kind;
  projectile_request request;
  uint32_t seed{0};
};
std::vector<remote_shot> remote_weapon_shots(remote_fire_input const &input, std::function<int32_t(int32_t, int32_t)> const &height);
} // namespace apache::simulation
