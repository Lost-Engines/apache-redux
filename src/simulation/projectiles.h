#pragma once
#include "effects.h"
#include "projectile_launch.h"
#include "target_visibility.h"

namespace apache::simulation {
enum class target_slot : uint8_t {
  current,
  chaff,
  flare
};
struct projectile_target_binding {
  bool selected{false};
  std::shared_ptr<target_reference> target;
};
struct projectile_record {
  projectile_launch_state state;
  target_reference source;
  std::optional<target_reference> selected;
  std::optional<target_reference> designator;
  std::optional<uint32_t> mobile;
  projectile_target_binding adjusted;
};
struct projectile_request {
  unsigned int weapon{0};
  projectile_launch_inputs launch;
  target_reference source;
  std::optional<target_reference> selected;
  std::optional<target_reference> designator;
  bool guided_constructor{false};
};
struct projectile_aim {
  std::optional<movement_target> target;
  bool actor{false};
};
enum class projectile_event : unsigned int {
  notify,
  proximity,
  map_edge,
  mobile,
  scenery,
  expired,
  terrain,
  ballistic,
  removed
};
struct projectile_event_info {
  projectile_event kind;
  math::vector3 position{};
  std::optional<target_identity> target;
};
struct projectile_callbacks {
  std::function<projectile_aim(projectile_record &, int32_t)> guidance;
  std::function<void(size_t, projectile_record &, projectile_event_info const &)> event;
};
struct projectile_time {
  int32_t now{0};
  int32_t delta{0};
  double seconds{0};
  int32_t smoke_radius{256};
  int32_t smoke_lifetime{200};
  unsigned int smoke_density{0};
};
class projectiles {
  std::array<weapon_info, 55> weapons;
  resources::model_archive const &models;
  math::trigonometry const &trig;
  combat_tables const &tables;
  resources::terrain const &terrain;
  scenery_collisions const &scenery;
  render::mobile_scene &mobiles;
  effects &smoke;
  uint32_t &random_seed;
  projectile_callbacks callbacks;
  size_t cursor{0};
  size_t count{0};
  std::array<std::optional<projectile_record>, 256> rounds;
  void emit(size_t index, projectile_event kind, math::vector3 const &position, std::optional<target_identity> target = {});
  bool collide(size_t index, math::vector3 const &move);
public:
  uint32_t weapon_random{1};
  projectiles(resources::model_archive const &objects, math::trigonometry const &angles, combat_tables const &performance, resources::terrain const &ground, scenery_collisions const &structures, render::mobile_scene &scene, effects &puffs, uint32_t &seed, projectile_callbacks events = {});
  size_t active_count() const {
    return count;
  }
  auto const &entries() const {
    return rounds;
  }
  projectile_record &at(size_t index) {
    return rounds.at(index).value();
  }
  std::optional<size_t> construct(projectile_request const &request);
  void launch(size_t index, int32_t now);
  void remove(size_t index);
  void clear();
  void update(projectile_time const &time);
};
} // namespace apache::simulation
