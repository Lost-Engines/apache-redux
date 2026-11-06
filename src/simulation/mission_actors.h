#pragma once
#include <functional>
#include "actor_types.h"
#include "math/trigonometry.h"
#include "render/model_renderer.h"
#include "resources/missions.h"

namespace apache::simulation {
struct actor_location {
  size_t task{0}, group{0}, member{0};
};
struct actor_state {
  actor_location location;
  render::model_instance object;
  uint32_t classification{0}, conditions{0x30004};
  int32_t serial{0};
  std::array<uint32_t, 3> angles{};
  math::vector3 velocity{};
  int32_t speed{0}, desired_speed{0}, route_distance{0};
  int32_t destroyed{0}, network{-1}, speed_ramp_target{0}, retreat_until{0};
  std::array<int32_t, 5> ammunition{};
  int32_t weapon{0}, weapon_slot{-1};
  uint32_t random{0};
  std::optional<size_t> carrier, carrier_slot, waypoint;
};
struct actor_group {
  size_t task{0}, index{0};
  resources::mission_actor definition;
  std::vector<size_t> units;
};
struct carrier_slot {
  size_t attachment{0};
  math::pose relative;
  std::optional<size_t> occupant;
  std::optional<size_t> reserved;
};
struct carrier_state {
  size_t actor{0}, available{0};
  std::vector<carrier_slot> slots;
};
struct actor_environment {
  std::function<int32_t(int32_t, int32_t)> height;
  std::function<unsigned int(int32_t, int32_t)> surface;
  std::function<void(actor_state &, math::vector3 const &)> relocate;
  void set_position(actor_state &actor, math::vector3 const &position) const { if(relocate) relocate(actor, position); else actor.object.world.position = position; }
};
class mission_actors {
  resources::mission_definition const &mission;
  resources::model_archive const &models;
  actor_environment const &environment;
  std::array<actor_type, 1315> types;
  void update_landing_gear(size_t index);
  void register_ship(size_t index, resources::model_archive const &models);
  bool register_aircraft(size_t index);
public:
  std::vector<actor_group> groups;
  std::vector<actor_state> units;
  std::vector<carrier_state> carriers;
  uint32_t random_seed{0};
  std::optional<std::array<size_t, 2>> selected_waypoint;
  bool on_ground(size_t index) const;
  void synchronize_models(bool maintain_carriers = true);
  mission_actors(resources::mission_definition const &mission, std::span<std::vector<resources::formation_member> const> formations, resources::model_archive const &models, math::trigonometry const &trig, actor_environment const &environment, unsigned int map, uint32_t seed = 1);
};
} // namespace apache::simulation
