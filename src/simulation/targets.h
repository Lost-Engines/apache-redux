#pragma once
#include "actor_movement.h"
#include "target_visibility.h"

namespace apache::simulation {
struct mobile_target_state {
  unsigned int type{1};
  int32_t classification{0}, speed{0}, player_airspeed{0}, destroyed{0};
  uint32_t conditions{0};
  uint8_t damage{0};
  bool player{false}, ground_radar{false}, air_radar{false};
  int32_t chaff_at{0}, flare_at{0};
  bool chaff_valid{false}, flare_valid{false};
  std::string_view user_name;
};
struct target_description {
  bool valid{false};
  movement_target movement;
  uint32_t model{0};
  int32_t status{1}, side{0}, damage{0};
  bool radar{false}, designated{false}, chaff{false}, flare{false};
  bool actor{false}, destroyed{false};
  std::string_view name{"No Target"}, description{"No Target"};
};
bool equivalent_targets(target_reference const &first, target_reference const &second);
bool within_target_sectors(math::vector3 const &first, math::vector3 const &second, int32_t range);
class target_world {
  resources::terrain const &terrain;
  scenery_collisions const &scenery;
  render::mobile_scene const &mobiles;
  resources::model_archive const &models;
  math::trigonometry const &trig;
  std::function<mobile_target_state(uint32_t)> mobile_state;
public:
  target_world(resources::terrain const &ground, scenery_collisions const &structures, render::mobile_scene const &objects, resources::model_archive const &definitions, math::trigonometry const &angles, std::function<mobile_target_state(uint32_t)> state);
  bool valid(target_reference const &target) const;
  math::vector3 position(target_reference const &target) const;
  target_description describe(target_reference const &target, int32_t now) const;
};
} // namespace apache::simulation
