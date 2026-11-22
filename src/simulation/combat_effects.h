#pragma once
#include <functional>
#include <optional>
#include "math/trigonometry.h"
#include "craters.h"
#include "effects.h"
#include "resources/models.h"
#include "weapon_rules.h"

namespace apache::simulation {
struct explosion_audio {
  int32_t volume{0}, pan{32768};
};
class combat_effects {
  resources::model_archive const &models;
  resources::terrain const &terrain;
  math::trigonometry const &trig;
  effects &visuals;
  craters &marks;
  combat_random &random;
  std::function<uint8_t(int32_t, int32_t)> surface;
  void single(math::vector3 const &position, int32_t radius, uint32_t heading);
  void sound(math::vector3 const &position, int32_t radius) const;
public:
  math::vector3 listener{0, 0, 0};
  uint32_t listener_heading{0};
  std::function<void(explosion_audio const &)> play_sound;
  combat_effects(resources::model_archive const &models, resources::terrain const &terrain, math::trigonometry const &trig, effects &visuals, craters &marks, combat_random &random, std::function<uint8_t(int32_t, int32_t)> surface);
  math::vector3 random_point(math::vector3 const &origin, int32_t radius);
  math::vector3 random_point_in_model(math::vector3 const &origin, uint32_t model, uint32_t heading);
  explosion_audio audio(math::vector3 const &position, int32_t radius) const;
  void crater(math::vector3 const &position, int32_t radius, uint32_t heading);
  void cluster(math::vector3 const &position, int32_t radius, uint32_t model, uint32_t heading);
  void ground(resources::scenery_placement &object, math::vector3 const &position, bool partial);
  void mobile(uint32_t model, math::pose const &position, bool partial);
  void weapon(unsigned int id, math::pose const &position, bool expired, bool skip_crater, std::optional<math::vector3> const &impact = std::nullopt);
};
} // namespace apache::simulation
