#pragma once
#include "arcade_flight.h"
#include "mission_spatial.h"

namespace apache::simulation {
struct mission_flight_settings {
  bool arcade{false};
  int32_t flow{0};
  double fuel{2442};
  uint32_t faults{0};
  std::array<uint32_t, 24> pods{};
  unsigned int game_mode{0};
};
// Binds the flight model to the mission's player object, carrier and spatial lists.
class mission_flight {
  resources::mission_definition const &mission;
  mission_actors &actors;
  mission_spatial &spatial;
  resources::model_archive const &models;
  resources::terrain const &terrain;
  math::trigonometry const &trig;
  std::function<unsigned int(int32_t, int32_t)> surface;
  size_t player;
  bool arcade_mode{false};
  math::pose carrier_relative;
  flight_environment realistic_world;
  arcade_environment arcade_world;
  void swap_slots(size_t other);
  void prepare_carrier(unsigned int game_mode);
  void land_on_carrier(size_t owner, render::model_instance const &object);
  math::pose place(math::pose const &position);
  void leave_carrier();
  void synchronize();
public:
  std::function<void()> died;
  std::function<void()> mobile_collision;
  std::function<void()> landing_award;
  uint32_t collision_model{0};
  int32_t model_weight{14058};
  std::optional<size_t> hit_actor;
  realistic_flight realistic;
  arcade_flight arcade;
  mission_flight(resources::mission_definition const &mission, mission_actors &actors, mission_spatial &spatial, resources::model_archive const &models, resources::terrain const &terrain, math::trigonometry const &trig, std::function<unsigned int(int32_t, int32_t)> surface, size_t player, mission_flight_settings const &settings);
  mission_flight(mission_flight const &) = delete;
  mission_flight &operator=(mission_flight const &) = delete;
  flight_readings const &readings() const {
    return arcade_mode ? arcade.readings : realistic.readings;
  }
  powerplant const &power() const {
    return arcade_mode ? arcade.power : realistic.dynamics.power;
  }
  void receive_shared(flight_readings const &state, bool landed, double endurance, int32_t weight);
  bool on_carrier() const {
    return (actors.units[player].conditions & 0x400) != 0;
  }
  bool uses_arcade() const {
    return arcade_mode;
  }
  bool landed() const {
    return arcade_mode ? arcade.landed : realistic.landed;
  }
  uint32_t faults() const {
    return arcade_mode ? arcade.power.faults : realistic.dynamics.power.faults;
  }
  void set_faults(uint32_t value) {
    if(arcade_mode) {
      arcade.power.faults = value;
    } else {
      realistic.dynamics.power.faults = value;
    }
  }
  bool invulnerable() const {
    return arcade_mode ? arcade.invulnerable : realistic.invulnerable;
  }
  unsigned int death() const {
    return arcade_mode ? arcade.death : realistic.death;
  }
  int32_t deck() const;
  void request_death(unsigned int cause);
  void reposition(math::vector3 const &position);
  void restart(int flow, double fuel = 2442);
  void refuel(double fuel = 2442);
  std::optional<landing_result> collide(math::vector3 const &previous, scenery_collisions const &scenery, render::mobile_scene const &mobiles, craters const &marks, std::function<void(target_identity const &)> const &destroy);
  flight_step_result advance(double dt, int32_t delta, flight_input const &input, unsigned int random_sample);
};
} // namespace apache::simulation
