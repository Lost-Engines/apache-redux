#pragma once
#include "actor_navigation.h"
#include "weapon_rules.h"

namespace apache::simulation {
struct movement_target {
  math::vector3 position{};
  math::vector3 direction{};
  int32_t speed{0};
  int32_t classification{0};
};
math::vector3 intercept_point(math::vector3 const &origin, int32_t speed, movement_target const &target, unsigned int iterations);
struct movement_callbacks {
  std::function<void(size_t)> check_air_target;
  std::function<std::optional<movement_target>(size_t)> target;
  std::function<void(size_t)> kill;
  std::function<void(size_t)> fire_at_waypoint;
  std::function<void(size_t)> place;
  std::function<void(size_t)> unplace;
  std::function<int32_t(size_t)> weapon;
};
struct movement_time {
  int32_t now{0};
  int32_t delta{0};
  int32_t game_flow{0};
  bool arcade{false};
  std::optional<size_t> player;
};
class actor_movement {
  mission_actors &actors;
  resources::mission_definition const &mission;
  std::span<std::vector<resources::formation_member> const> formations;
  resources::model_archive const &models;
  math::trigonometry const &trig;
  actor_environment const &ground;
  navigation_environment const &world;
  navigation_tables const &tables;
  std::array<actor_type, 1315> types;
  std::array<weapon_info, 55> weapons;
  combat_random &random;
  movement_callbacks callbacks;
  void take_off(size_t index, actor_group const &group);
  void land(size_t index);
  math::vector3 landing_point(actor_state const &unit) const;
  math::vector3 fly_to(size_t index, actor_group const &group, std::optional<movement_target> const &target);
  void retreat(actor_state const &unit, math::vector3 &target);
  void move_air_or_ship(size_t index, actor_group const &group, movement_time const &time);
public:
  actor_movement(mission_actors &units, resources::mission_definition const &definition, std::span<std::vector<resources::formation_member> const> shapes, resources::model_archive const &objects, math::trigonometry const &angles, actor_environment const &terrain, navigation_environment const &scenery, navigation_tables const &performance, combat_random &random_state, movement_callbacks events = {});
  void release_carrier(size_t actor, bool place_at_origin = false);
  void update(movement_time const &time);
};
} // namespace apache::simulation
