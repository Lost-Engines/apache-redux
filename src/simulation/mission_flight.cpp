#include "mission_flight.h"
#include <algorithm>
#include <stdexcept>

namespace apache::simulation {
mission_flight::mission_flight(resources::mission_definition const &definition, mission_actors &units, mission_spatial &locations, resources::model_archive const &objects, resources::terrain const &land, math::trigonometry const &angles, std::function<unsigned int(int32_t, int32_t)> materials, size_t const index, mission_flight_settings const &settings)
  : mission{definition},
    actors{units},
    spatial{locations},
    models{objects},
    terrain{land},
    trig{angles},
    surface{std::move(materials)},
    player{index},
    arcade_mode{settings.arcade} {
  auto &unit{actors.units.at(player)};
  spatial.player = player;
  auto const status{unit.object.status};
  unit.object = render::construct_model(models, 103, unit.object.world, settings.pods);
  unit.object.status = status & ~0x10u;
  if(on_carrier()) prepare_carrier(settings.game_mode);
  unit.network = 0;
  auto const height{[this](int32_t x, int32_t y){return on_carrier() ? math::add(deck(), 2304) : terrain.elevation(x, y);}};
  realistic_world.height = arcade_world.height = height;
  realistic_world.surface = arcade_world.surface = surface;
  realistic_world.place = arcade_world.place = [this](auto const &position){return place(position);};
  realistic_world.leave_carrier = [this](realistic_flight &flight){
    auto const &ship{actors.units[actors.carriers.at(*actors.units[player].carrier).actor]};
    flight.transfer_from_carrier(ship.speed, ship.angles[0], trig);
    leave_carrier();
  };
  arcade_world.leave_carrier = [this](arcade_flight &flight){
    auto const &ship{actors.units[actors.carriers.at(*actors.units[player].carrier).actor]};
    flight.transfer_from_carrier(ship.speed, ship.angles[0], trig);
    leave_carrier();
  };
  flight_start start;
  start.position = unit.object.world.position;
  start.on_carrier = on_carrier();
  start.object_heading = unit.angles[0];
  start.dry_weight = model_weight;
  start.fuel = settings.fuel;
  start.faults = settings.faults;
  start.quick_flight = settings.flow == 1;
  start.wind_strength = mission.wind_strength;
  start.wind_direction = mission.wind_direction;
  start.ground_height = height(start.position[0], start.position[1]);
  auto position{start.position};
  if(on_carrier()) {
    position = place(unit.object.world).position;
  } else if(!arcade_mode) {
    for(size_t i{0}; i != 2; ++i) position[i] = (position[i] / 256) * 256;
  }
  if(unit.waypoint) {
    auto const &route{mission.tasks.at(unit.location.task).actors.at(unit.location.group).waypoints};
    auto const &target{route.at(*unit.waypoint).position};
    start.heading = trig.arctangent(math::subtract(target[0], position[0]), math::subtract(target[1], position[1]));
  }
  realistic_world.landing_award = arcade_world.landing_award = [this]{
    if(landing_award) landing_award();
  };
  realistic_world.carrier_deck = deck();
  if(arcade_mode) {
    arcade.initialise(start, trig, arcade_world);
    unit.angles[1] = unit.angles[2] = 0;
  } else {
    realistic.initialise(start, trig, realistic_world);
  }
  realistic.died = arcade.died = [this]{
    if(died) died();
  };
  synchronize();
}
int32_t mission_flight::deck() const {
  auto const &unit{actors.units[player]};
  if(!unit.carrier) return 0;
  auto const &ship{actors.units[actors.carriers.at(*unit.carrier).actor]};
  return math::add(ship.object.world.position[2], models.at(ship.object.id)->bounds[5]);
}
void mission_flight::swap_slots(size_t const other) {
  auto &a{actors.units[player]};
  auto &b{actors.units[other]};
  if(!a.carrier || !a.carrier_slot || !b.carrier || !b.carrier_slot) return;
  for(auto index : {player, other}) {
    auto const &unit{actors.units[index]};
    auto &ship{actors.carriers[*unit.carrier]};
    auto &slot{ship.slots[*unit.carrier_slot]};
    slot.occupant.reset();
    actors.units[ship.actor].object.children[slot.attachment].reset();
  }
  std::swap(a.carrier, b.carrier);
  std::swap(a.carrier_slot, b.carrier_slot);
  for(auto index : {player, other}) {
    auto &unit{actors.units[index]};
    auto &slot{actors.carriers[*unit.carrier].slots[*unit.carrier_slot]};
    unit.object.relative = slot.relative;
    slot.reserved = index;
    if(unit.conditions & 0x400) slot.occupant = index;
  }
  actors.synchronize_models();
}
void mission_flight::prepare_carrier(unsigned int const game_mode) {
  auto &unit{actors.units[player]};
  if(!unit.carrier || !unit.carrier_slot) throw std::runtime_error{"Player carrier placement has no assigned slot"};
  auto &carrier{actors.carriers.at(*unit.carrier)};
  auto &ship{actors.units[carrier.actor]};
  if(game_mode != 2 && ship.object.id == 130 && *unit.carrier_slot != 7 && carrier.slots.size() > 7) {
    auto const other{carrier.slots[7].reserved};
    if(other) {
      swap_slots(*other);
    } else {
      auto &old{carrier.slots[*unit.carrier_slot]};
      old.occupant.reset();
      ship.object.children[old.attachment].reset();
      unit.carrier_slot = 7;
    }
  }
  auto &slot{carrier.slots[*unit.carrier_slot]};
  carrier_relative = slot.relative;
  unit.speed = ship.speed;
  unit.angles = ship.angles;
  unit.object.world.rotation = trig.transformation(unit.angles[0], unit.angles[1], unit.angles[2]);
  if(unit.object.world.position[0] || unit.object.world.position[1]) spatial.place(player);
  slot.occupant.reset();
  ship.object.children[slot.attachment].reset();
  unit.object.world = place(unit.object.world);
}
math::pose mission_flight::place(math::pose const &position) {
  auto result{position};
  if(on_carrier()) {
    auto const &unit{actors.units[player]};
    auto const &ship{actors.units[actors.carriers.at(*unit.carrier).actor]};
    result = math::attach(ship.object.world, carrier_relative);
    result.position[2] = math::add(result.position[2], 2304);
  }
  auto &unit{actors.units[player]};
  unit.object.world.rotation = result.rotation;
  spatial.move(unit, result.position);
  return result;
}
void mission_flight::leave_carrier() {
  actors.units[player].conditions &= ~0x400u;
}
void mission_flight::synchronize() {
  auto &object{actors.units[player].object};
  object.relative = object.world = readings().position;
  // Moving the root does not update attachment transforms. The original gun
  // updater consumes the preceding barrel position before maintaining the tree.
}
flight_step_result mission_flight::advance(double const dt, int32_t const delta, flight_input const &input, unsigned int const random_sample) {
  realistic_world.carrier_deck = deck();
  auto result{arcade_mode ? arcade.advance(dt, input, arcade_world, trig) : realistic.advance(dt, delta, input, realistic_world, trig, random_sample)};
  synchronize();
  return result;
}
void mission_flight::receive_shared(flight_readings const &state, bool const landed, double const endurance, int32_t const weight) {
  auto &current{arcade_mode ? arcade.readings : realistic.readings};
  auto const previous{current.position};
  current = state;
  if(!death() && !actors.units[player].destroyed) {
    auto &object{actors.units[player].object};
    object.world.rotation = state.position.rotation;
    spatial.move(actors.units[player], state.position.position);
    object.relative = object.world;
  } else {
    current.position = previous;
  }
  auto &engine{arcade_mode ? arcade.power : realistic.dynamics.power};
  engine.endurance = endurance;
  model_weight = weight;
  if(arcade_mode) {
    arcade.landed = landed;
  } else {
    realistic.landed = landed;
  }
}
void mission_flight::reposition(math::vector3 const &position) {
  if(arcade_mode) {
    arcade.x = position[0];
    arcade.y = position[1];
    arcade.z = position[2];
  } else {
    realistic.dynamics.x = position[0] / 256;
    realistic.dynamics.y = position[1] / 256;
    realistic.dynamics.z = position[2] / 256;
  }
  auto &display{arcade_mode ? arcade.readings : realistic.readings};
  display.position.position = position;
  display.position = place(display.position);
  synchronize();
}
void mission_flight::refuel(double const fuel) {
  auto &engine{arcade_mode ? arcade.power : realistic.dynamics.power};
  engine.weight = arcade_mode ? fuel - engine.fuel + engine.weight : fuel + model_weight;
  engine.fuel = fuel;
  if(fuel > 0) {
    engine.faults &= ~0x300000u;
  } else if(fuel == 0) engine.faults |= 0x300000;
}
void mission_flight::restart(int const flow, double const fuel) {
  auto &unit{actors.units[player]};
  flight_start start;
  start.position = unit.object.world.position;
  start.on_carrier = on_carrier();
  start.object_heading = unit.angles[0];
  start.dry_weight = model_weight;
  start.fuel = fuel;
  start.quick_flight = flow == 1;
  start.wind_strength = realistic.dynamics.wind_strength;
  start.wind_direction = realistic.wind_direction;
  start.ground_height = realistic_world.height(start.position[0], start.position[1]);
  auto position{start.position};
  if(!arcade_mode) {
    for(size_t i{0}; i < 2; ++i) position[i] = (position[i] / 256) * 256;
  }
  if(unit.waypoint) {
    auto const &target{mission.tasks.at(unit.location.task).actors.at(unit.location.group).waypoints.at(*unit.waypoint).position};
    start.heading = trig.arctangent(math::subtract(target[0], position[0]), math::subtract(target[1], position[1]));
  }
  if(arcade_mode) {
    auto const old{arcade};
    arcade.initialise(start, trig, arcade_world);
    auto const pose{arcade.readings.position};
    arcade.readings = old.readings;
    arcade.readings.position = pose;
    arcade.power = old.power;
    arcade.power.weight = 14058 + fuel - old.power.fuel + fuel;
    arcade.power.fuel = fuel;
    arcade.power.faults = 0;
    arcade.power.rotor_sound = 1;
    arcade.power.overheat_message = false;
    arcade.power.overtorque_timer = 0;
    arcade.delta_heading = old.delta_heading;
    arcade.delta_throttle = old.delta_throttle;
    arcade.delta_forward_velocity = old.delta_forward_velocity;
    arcade.delta_lateral_velocity = old.delta_lateral_velocity;
    arcade.infinite_fuel = old.infinite_fuel;
    arcade.invulnerable = old.invulnerable;
    arcade.no_crashes = old.no_crashes;
    unit.angles[1] = unit.angles[2] = 0;
  } else {
    auto const old{realistic};
    realistic.initialise(start, trig, realistic_world);
    realistic.readings.airspeed = old.readings.airspeed;
    realistic.readings.sidespeed = old.readings.sidespeed;
    realistic.readings.climb = old.readings.climb;
    realistic.readings.torque = old.readings.torque;
    realistic.readings.fuel = old.readings.fuel;
    realistic.infinite_fuel = old.infinite_fuel;
    realistic.invulnerable = old.invulnerable;
    realistic.no_crashes = old.no_crashes;
    realistic.game_mode = old.game_mode;
    realistic.pull_up_timer = old.pull_up_timer;
    realistic.dynamics.power.warning_timer = old.dynamics.power.warning_timer;
  }
  realistic.died = arcade.died = [this]{
    if(died) died();
  };
  collision_model = 0;
  hit_actor.reset();
  synchronize();
}
void mission_flight::request_death(unsigned int const cause) {
  if(!arcade_mode) {
    realistic.request_death(cause);
  } else {
    arcade.request_death(cause);
  }
}
void mission_flight::land_on_carrier(size_t const owner, render::model_instance const &object) {
  auto const contact_pose{object.world};
  auto const contact_model{object.id};
  auto &unit{actors.units[player]};
  size_t const carrier_index{*actors.units[owner].carrier};
  auto &carrier{actors.carriers.at(carrier_index)};
  math::vector3 offset;
  for(size_t i{0}; i != 3; ++i) offset[i] = math::subtract(unit.object.world.position[i], contact_pose.position[i]);
  auto const local{math::transform(contact_pose.rotation, offset)};
  std::optional<size_t> nearest;
  int32_t distance{INT32_MAX};
  for(size_t i{0}; i != carrier.slots.size(); ++i) {
    auto const &slot{carrier.slots[i]};
    if(actors.units[carrier.actor].object.children[slot.attachment]) continue;
    math::vector3 difference;
    for(size_t j{0}; j != 3; ++j) {
      auto const v{math::subtract(local[j], slot.relative.position[j])};
      difference[j] = v < 0 ? math::negate(v) : v;
    }
    std::ranges::sort(difference);
    int32_t const range{math::add(math::add(difference[2], difference[1] >> 2), difference[0] >> 2)};
    if(range < distance) {
      nearest = i;
      distance = range;
    }
  }
  if(!nearest) {
    unit.carrier = carrier_index;
  } else if(unit.carrier == carrier_index && unit.carrier_slot == nearest) {
    swap_slots(player);
  } else if(!carrier.slots[*nearest].reserved) {
    unit.carrier = carrier_index;
    unit.carrier_slot = nearest;
  } else {
    if(!unit.carrier_slot) {
      for(size_t i{0}; i != actors.carriers.size(); ++i) {
        auto &available{actors.carriers[i]};
        if(!available.available) continue;
        unit.carrier = i;
        unit.carrier_slot = --available.available;
        available.slots[*unit.carrier_slot].reserved = player;
        break;
      }
    }
    if(unit.carrier && unit.carrier_slot) {
      swap_slots(*carrier.slots[*nearest].reserved);
    } else {
      unit.carrier = carrier_index;
      unit.carrier_slot = nearest;
    }
  }
  carrier_relative.position = local;
  carrier_relative.rotation = trig.transformation(trig.angles(unit.object.world.rotation)[0] - trig.angles(contact_pose.rotation)[0], 0, 0);
  carrier_relative.position[2] = math::add(contact_pose.position[2], models.at(contact_model)->bounds[5]);
  unit.conditions |= 0x400;
  realistic.on_carrier = arcade.on_carrier = true;
}
std::optional<landing_result> mission_flight::collide(math::vector3 const &previous, scenery_collisions const &scenery, render::mobile_scene const &mobiles, craters const &marks, std::function<void(target_identity const &)> const &destroy) {
  hit_actor.reset();
  auto const hit{check_flight_collision(previous, readings().position.position, spatial.mobile_id(player), mobiles, scenery, marks, terrain, models, trig)};
  if(hit.kind == flight_collision_kind::none) return {};
  collision_model = hit.model;
  if(hit.kind == flight_collision_kind::scenery) {
    request_death(2);
    if(destroy) destroy({target_kind::scenery, hit.ground->layer, static_cast<uint32_t>(hit.ground->index)});
    return {};
  }
  if(hit.kind == flight_collision_kind::crater) {
    request_death(2);
    return {};
  }
  if(hit.part == &mobiles.at(*hit.mobile)) {
    hit_actor = spatial.actor_index(*hit.mobile);
  } else {
    for(auto const &carrier : actors.carriers) {
      for(auto const &slot : carrier.slots) {
        if(slot.occupant && actors.units[carrier.actor].object.children[slot.attachment].get() == hit.part) hit_actor = slot.occupant;
      }
    }
  }
  if(!landed() && hit_actor && actors.units[*hit_actor].carrier && carrier_landing_contact(true, math::add(hit.part->world.position[2], models.at(hit.model)->bounds[5]), math::subtract(previous[2], 2304), math::subtract(readings().position.position[2], 2304))) {
    auto const speed{actors.units[*hit_actor].speed};
    land_on_carrier(*hit_actor, *hit.part);
    auto &display{arcade_mode ? arcade.readings : realistic.readings};
    display.angles[1] = display.angles[2] = 0;
    auto const matrix{flight_attitude(static_cast<double>(std::bit_cast<int32_t>(display.angles[0])) * 6.28318530718 * 2.328306437080797e-10, 0, 0)};
    for(size_t i{0}; i != 9; ++i) display.position.rotation[i] = static_cast<int32_t>(matrix[i] * 2147483647.0);
    auto const forward{arcade_mode ? arcade.forward_velocity : realistic.dynamics.u};
    auto const vertical{arcade_mode ? arcade.delta_vertical_velocity * 72 : realistic.dynamics.zdot};
    auto const result{evaluate_landing((forward - speed) * 0.00390625, vertical, surface(display.position.position[0], display.position.position[1]), true, death())};
    if(result.landed) {
      if(arcade_mode) {
        arcade.landed = true;
      } else {
        realistic.landed = true;
      }
    }
    if(result.death) request_death(result.death);
    if(result.award && landing_award) landing_award();
    synchronize();
    return result;
  }
  if(!on_carrier()) {
    request_death(2);
    if(mobile_collision) mobile_collision();
    if(hit_actor && destroy) destroy(spatial.targets[*hit_actor]->identity);
  }
  return {};
}
} // namespace apache::simulation
