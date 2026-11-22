#include "actor_movement.h"
#include <algorithm>
#include <cmath>
#include "ground_movement.h"

namespace apache::simulation {
namespace {
uint32_t magnitude(int32_t const value) {
  return value < 0 ? 0u - static_cast<uint32_t>(value) : static_cast<uint32_t>(value);
}
uint32_t range3(math::vector3 const &difference) {
  double const x{static_cast<double>(difference[0])};
  double const y{static_cast<double>(difference[1])};
  double const z{static_cast<double>(difference[2])};
  return static_cast<uint32_t>(static_cast<uint64_t>(std::sqrt(x * x + y * y + z * z)));
}
int32_t distance2(math::vector3 const &a, math::vector3 const &b) {
  uint64_t sum{0};
  for(size_t i{0}; i != 2; ++i) {
    int64_t const v{math::subtract(a[i], b[i])};
    sum += static_cast<uint64_t>(v * v);
  }
  return std::bit_cast<int32_t>(math::integer_sqrt(sum));
}
} // anonymous namespace
math::vector3 intercept_point(math::vector3 const &origin, int32_t const speed, movement_target const &target, unsigned int const iterations) {
  auto result{target.position};
  uint32_t const launch_speed{magnitude(speed)};
  for(unsigned int iteration{0}; iteration != iterations; ++iteration) {
    math::vector3 difference;
    for(size_t i{0}; i != 3; ++i) difference[i] = math::subtract(result[i], origin[i]);
    uint64_t const product{uint64_t{range3(difference)} * magnitude(target.speed)};
    if(!launch_speed || (product >> 32) >= launch_speed) return target.position;
    auto const travel{static_cast<uint32_t>(product / launch_speed)};
    if(travel == UINT32_MAX) return target.position;
    for(size_t i{0}; i != 3; ++i) result[i] = math::add(target.position[i], math::multiply_q31(target.direction[i], std::bit_cast<int32_t>(travel)));
  }
  return result;
}
actor_movement::actor_movement(mission_actors &units, resources::mission_definition const &definition, std::span<std::vector<resources::formation_member> const> const shapes, resources::model_archive const &objects, math::trigonometry const &angles, actor_environment const &terrain, navigation_environment const &scenery, navigation_tables const &performance, combat_random &random_state, movement_callbacks events)
  : actors{units},
    mission{definition},
    formations{shapes},
    models{objects},
    trig{angles},
    ground{terrain},
    world{scenery},
    tables{performance},
    types{actor_types()},
    weapons{weapon_definitions()},
    random{random_state},
    callbacks{std::move(events)} {}
void actor_movement::take_off(size_t const index, actor_group const &group) {
  auto &unit{actors.units[index]};
  if(!unit.carrier || !unit.carrier_slot) return;
  auto &carrier{actors.carriers[*unit.carrier]};
  auto &ship{actors.units[carrier.actor]};
  auto &slot{carrier.slots[*unit.carrier_slot]};
  unit.speed = ship.speed;
  unit.conditions &= ~0x400u;
  unit.angles = ship.angles;
  if(unit.waypoint) {
    auto target{group.definition.waypoints[*unit.waypoint].position};
    // ship.c uses the planner selection's Y with this aircraft's waypoint X.
    if(actors.selected_waypoint) target[1] = actors.groups.at((*actors.selected_waypoint)[0]).definition.waypoints.at((*actors.selected_waypoint)[1]).position[1];
    unit.route_distance = distance2(unit.object.world.position, target);
  }
  unit.object.world.rotation = trig.transformation(unit.angles[0], unit.angles[1], unit.angles[2]);
  if((unit.object.world.position[0] || unit.object.world.position[1]) && callbacks.place) callbacks.place(index);
  slot.occupant.reset();
  ship.object.children[slot.attachment].reset();
}
void actor_movement::release_carrier(size_t const index, bool const place_at_origin) {
  auto const &unit{actors.units.at(index)};
  if(!unit.carrier || !unit.carrier_slot || !(unit.conditions & 0x400)) return;
  for(auto const &group : actors.groups) {
    if(group.task == unit.location.task && group.index == unit.location.group) {
      take_off(index, group);
      if(place_at_origin && !unit.object.world.position[0] && !unit.object.world.position[1] && callbacks.place) callbacks.place(index);
      return;
    }
  }
}
void actor_movement::land(size_t const index) {
  auto &unit{actors.units[index]};
  if(!unit.carrier || !unit.carrier_slot) return;
  auto &carrier{actors.carriers[*unit.carrier]};
  auto &ship{actors.units[carrier.actor]};
  auto &slot{carrier.slots[*unit.carrier_slot]};
  unit.conditions |= 0x400;
  if(ship.destroyed) return;
  if(callbacks.unplace) callbacks.unplace(index);
  math::vector3 delta;
  for(size_t i{0}; i != 3; ++i) delta[i] = math::subtract(unit.object.world.position[i], ship.object.world.position[i]);
  slot.relative.position = math::transform(ship.object.world.rotation, delta);
  slot.relative.rotation = trig.transformation(trig.angles(unit.object.world.rotation)[0] - trig.angles(ship.object.world.rotation)[0], 0, 0);
  unit.object.relative = slot.relative;
  slot.occupant = index;
}
math::vector3 actor_movement::landing_point(actor_state const &unit) const {
  auto const &carrier{actors.carriers.at(*unit.carrier)};
  return math::attach(actors.units[carrier.actor].object.world, carrier.slots.at(*unit.carrier_slot).relative).position;
}
math::vector3 actor_movement::fly_to(size_t const index, actor_group const &group, std::optional<movement_target> const &target) {
  auto &unit{actors.units[index]};
  auto const &route{group.definition.waypoints};
  auto const &waypoint{route.at(*unit.waypoint)};
  if(!target) {
    if(*unit.waypoint + 1 == route.size() && !waypoint.position[2] && ground.surface(waypoint.position[0], waypoint.position[1]) == 3 && unit.carrier && unit.carrier_slot) return landing_point(unit);
    return waypoint.position;
  }
  auto result{target->position};
  if(!(unit.conditions & 8)) {
    result[2] = waypoint.position[2];
    return result;
  }
  auto const &weapon{weapons.at(static_cast<size_t>(callbacks.weapon ? callbacks.weapon(index) : unit.weapon))};
  if(weapon.movement_class != 2) {
    int32_t speed{unit.speed};
    double const threshold{static_cast<double>(magnitude(target->speed)) * 1.2};
    if(magnitude(speed) <= threshold) speed = weapon.maximum_speed;
    if(magnitude(speed) > threshold) result = intercept_point(unit.object.world.position, speed, *target, 3);
  }
  result[2] = std::max(result[2], math::add(world.height(result[0], result[1]), std::max(unit.speed / 2, 0x2800)));
  return result;
}
void actor_movement::retreat(actor_state const &unit, math::vector3 &target) {
  auto const &position{unit.object.world.position};
  for(size_t i{0}; i != 2; ++i) {
    int32_t const half{math::subtract(target[i], position[i]) / 2};
    if(half) target[i] = math::subtract(math::subtract(position[i], half), random.next() % half);
  }
  if(math::add(world.height(position[0], position[1]), 0x1f400) < position[2]) {
    int32_t const half{math::subtract(target[2], position[2]) / 2};
    target[2] = half ? math::subtract(math::subtract(position[2], half), random.next() % half) : math::add(position[2], random.next() % 64000);
  } else {
    target[2] = math::add(position[2], random.next() % 0x1f400);
  }
}
void actor_movement::move_air_or_ship(size_t const index, actor_group const &group, movement_time const &time) {
  auto &unit{actors.units[index]};
  auto const &route{group.definition.waypoints};
  auto const &task{mission.tasks[group.task]};
  if(unit.destroyed || !unit.waypoint) return;
  bool const ship{unit.classification == 7};
  if(!unit.speed && *unit.waypoint + 1 == route.size() && (ship || route[*unit.waypoint].position[2] == unit.object.world.position[2])) return;
  if(!ship && (unit.conditions & 0x400)) take_off(index, group);
  if(!ship && callbacks.check_air_target) callbacks.check_air_target(index);
  int32_t const weapon_id{!ship && (unit.conditions & 8) && callbacks.weapon ? callbacks.weapon(index) : unit.weapon};
  auto const target{callbacks.target ? callbacks.target(index) : std::nullopt};
  if(!ship && !target && unit.retreat_until < time.now) unit.conditions &= ~2u;
  auto performance{ship ? tables.ships.at(types.at(unit.object.id).aircraft) : tables.aircraft.at(types.at(unit.object.id).aircraft)};
  navigation_order order;
  order.target = ship || (time.game_flow == 7 && (task.type == 13 || task.type == 14) && !(unit.conditions & 0x20)) ? route[*unit.waypoint].position : fly_to(index, group, target);
  if(!ship && (order.target[0] < 0 || order.target[1] < 0 || order.target[0] > 0xfffffff || order.target[1] > 0xfffffff)) {
    if(callbacks.kill) {
      callbacks.kill(index);
    } else {
      unit.destroyed = 1;
    }
    return;
  }
  if(unit.conditions & 2) retreat(unit, order.target);
  order.waypoint_height = route[*unit.waypoint].position[2];
  order.waypoint_speed = route[*unit.waypoint].speed;
  order.arrival_radius = unit.conditions & 8 ? weapons.at(static_cast<size_t>(weapon_id)).maximum_range / 3 : unit.speed;
  if(*unit.waypoint) order.previous_height = route[*unit.waypoint - 1].position[2];
  if(unit.carrier && !ship) order.carrier_speed = actors.units[actors.carriers[*unit.carrier].actor].speed;
  order.has_target = target.has_value();
  order.target_classification = target ? target->classification : 0;
  order.player_task = task.user;
  order.arcade = time.arcade;
  if(unit.classification == 1 && time.arcade) performance.acceleration *= 4;
  auto const result{navigate_actor(unit, order, performance, trig, world, time.delta)};
  if(result.outside_world) {
    if(callbacks.kill) {
      callbacks.kill(index);
    } else {
      unit.destroyed = 1;
    }
    return;
  }
  if(!result.arrived) return;
  if(ship) {
    if(unit.conditions & 2) return;
    if(*unit.waypoint + 1 < route.size() && task.type == 12 && route[*unit.waypoint + 1].type == 4) {
      ++*unit.waypoint;
      if(callbacks.fire_at_waypoint) callbacks.fire_at_waypoint(index);
    }
    if(*unit.waypoint + 1 < route.size()) {
      ++*unit.waypoint;
    } else if(route.front().speed) unit.waypoint = 0;
  } else if((!(unit.conditions & 8) && !target) || task.type == 13) {
    if(*unit.waypoint + 1 < route.size()) {
      ++*unit.waypoint;
      return;
    }
    int32_t const height{world.height(unit.object.world.position[0], unit.object.world.position[1])};
    if(math::add(height, 0x300) < route[*unit.waypoint].position[2]) {
      unit.waypoint = 0;
      for(size_t i{0}; i != route.size(); ++i) {
        if(route[i].type == 3) {
          unit.waypoint = i;
          break;
        }
      }
    } else {
      unit.object.world.position[2] = unit.carrier ? math::add(actors.units[actors.carriers[*unit.carrier].actor].object.world.position[2], models.at(actors.units[actors.carriers[*unit.carrier].actor].object.id)->bounds[5]) : height;
      unit.speed = 0;
      unit.angles[1] = unit.angles[2] = 0;
      unit.object.world.rotation = trig.transformation(unit.angles[0], 0, 0);
      land(index);
    }
  }
}
void actor_movement::update(movement_time const &time) {
  for(auto const &group : actors.groups) {
    auto const &task{mission.tasks[group.task]};
    auto const index{group.units.front()};
    auto &leader{actors.units[index]};
    if(task.type == 10 || task.type == 11 || task.type == 16 || task.type == 20 || time.player == index || leader.network != -1 || group.definition.start_time > time.now) continue;
    if(leader.classification == 3 || leader.classification == 4 || leader.classification == 8) {
      ground_movement movement{actors.units, group, task, formations[group.definition.formation], trig, ground};
      movement.update(time.now, time.delta);
      continue;
    }
    if(!(leader.conditions & 0x40000) && leader.waypoint) {
      leader.conditions |= 0x40000;
      if(leader.classification == 1) {
        leader.speed = leader.object.world.position[2] && *leader.waypoint ? group.definition.waypoints[*leader.waypoint - 1].speed : 0;
      } else {
        leader.speed = group.definition.waypoints[*leader.waypoint].speed;
        for(size_t i{1}; i != group.units.size(); ++i) {
          auto &member{actors.units[group.units[i]]};
          member.speed = leader.speed;
          member.conditions |= 0x40000;
        }
      }
    }
    if(leader.classification == 1 || leader.classification == 2 || leader.classification == 7) move_air_or_ship(index, group, time);
  }
}
} // namespace apache::simulation
