#include "ground_movement.h"
#include <algorithm>

namespace apache::simulation {
namespace {
int32_t ratio(int32_t const a, int32_t const b, int32_t const divisor) {
  if(!divisor) return 0;
  return std::bit_cast<int32_t>(static_cast<uint32_t>(int64_t{a} * b / divisor));
}
int32_t distance(math::vector3 const &a, math::vector3 const &b) {
  uint64_t sum{0};
  for(size_t i{0}; i != 2; ++i) {
    int64_t const d{math::subtract(a[i], b[i])};
    sum += static_cast<uint64_t>(d * d);
  }
  return std::bit_cast<int32_t>(math::integer_sqrt(sum));
}
int32_t approximate_distance(int32_t x, int32_t y) {
  if(x < 0) x = math::negate(x);
  if(y < 0) y = math::negate(y);
  return math::add(std::bit_cast<int32_t>(static_cast<uint32_t>(std::max(x, y)) * 4), std::min(x, y)) >> 2;
}
void translate(actor_state &unit, math::vector3 const &amount, actor_environment const &environment) {
  auto position{unit.object.world.position};
  for(size_t i{0}; i != 3; ++i) position[i] = math::add(position[i], amount[i]);
  environment.set_position(unit, position);
}
void advance(actor_state &unit, int32_t const delta, actor_environment const &environment) {
  math::vector3 amount;
  for(size_t i{0}; i != 3; ++i) amount[i] = math::multiply_q31(unit.velocity[i], delta);
  translate(unit, amount, environment);
}
} // anonymous namespace
ground_movement::ground_movement(std::vector<actor_state> &actors, actor_group const &actor_group, resources::mission_task const &orders, std::span<resources::formation_member const> const members, math::trigonometry const &angles, actor_environment const &world)
  : units{actors},
    group{actor_group},
    task{orders},
    formation{members},
    trig{angles},
    environment{world} {}
std::span<size_t const> ground_movement::members(actor_state const &unit) const {
  return &unit == &units[group.units.front()] ? std::span<size_t const>{group.units}.subspan(1) : std::span<size_t const>{};
}
math::vector3 ground_movement::formation_point(math::vector3 position, size_t const member, uint32_t const heading) const {
  if(formation.empty()) return position;
  auto const &offset{formation[member]};
  auto const s{trig.sine(heading)};
  auto const c{trig.cosine(heading)};
  position[0] = math::add(position[0], math::add(math::multiply_q31(offset.offset[0], c), math::multiply_q31(offset.offset[1], s)));
  position[1] = math::add(position[1], math::subtract(math::multiply_q31(offset.offset[1], c), math::multiply_q31(offset.offset[0], s)));
  position[2] = math::add(position[2], offset.offset[2]);
  return position;
}
math::vector3 ground_movement::drive_to(actor_state const &leader, size_t const member) const {
  auto const position{leader.waypoint ? group.definition.waypoints.at(*leader.waypoint).position : leader.object.world.position};
  return formation_point(position, member, leader.angles[0]);
}
void ground_movement::starting_speed(actor_state &unit) {
  unit.conditions |= 0x40000;
  unit.speed = unit.waypoint ? group.definition.waypoints.at(*unit.waypoint).speed : 0;
  for(auto id : members(unit)) {
    units[id].speed = unit.speed;
    units[id].conditions |= 0x40000;
  }
}
void ground_movement::zero_speed(actor_state &unit, bool const stopped) {
  unit.conditions = (unit.conditions & ~0x40000u) | (stopped ? 0x40000u : 0u);
  unit.speed = 0;
  for(auto id : members(unit)) {
    units[id].conditions = (units[id].conditions & ~0x40000u) | (stopped ? 0x40000u : 0u);
    units[id].speed = 0;
  }
}
void ground_movement::check_type(actor_state &unit) {
  auto const following{members(unit)};
  auto const &route{group.definition.waypoints};
  if(following.empty() || !unit.waypoint) {
    unit.conditions &= ~0x200u;
    return;
  }
  size_t const current{*unit.waypoint};
  auto const surface{environment.surface(route[current].position[0], route[current].position[1])};
  bool const independent{surface == 4 || surface == 5 || surface == 1 || route[current].position[2] || (current + 1 < route.size() && route[current + 1].position[2]) || (current && route[current - 1].position[2]) || (current > 1 && route[current - 2].position[2])};
  if(independent) {
    if(!(unit.conditions & 0x200)) {
      for(auto id : following) {
        auto &member{units[id]};
        if(member.destroyed) continue;
        member.waypoint = current;
        member.route_distance = distance(route[current].position, member.object.world.position);
        member.speed = unit.speed;
        member.angles[0] = trig.arctangent(math::subtract(route[current].position[0], member.object.world.position[0]), math::subtract(route[current].position[1], member.object.world.position[1]));
      }
    }
    unit.conditions |= 0x200;
    return;
  }
  if(unit.conditions & 0x200) {
    size_t member_number{1};
    // This transition pairs ascending offsets with the original reverse list.
    for(size_t i{following.size()}; i-- > 0; ++member_number) {
      auto &member{units[following[i]]};
      if(member.destroyed) continue;
      environment.set_position(member, formation_point(unit.object.world.position, member_number, unit.angles[0]));
      auto const target{drive_to(unit, member_number)};
      auto const range{distance(target, member.object.world.position)};
      member.angles[0] = trig.arctangent(math::subtract(target[0], member.object.world.position[0]), math::subtract(target[1], member.object.world.position[1]));
      auto const speed{ratio(unit.speed, range, unit.route_distance)};
      member.velocity = {math::multiply_q31(trig.sine(member.angles[0]), speed), math::multiply_q31(trig.cosine(member.angles[0]), speed), 0};
      member.speed = unit.speed;
      member.object.world.rotation = trig.transformation(member.angles[0], member.angles[1], member.angles[2]);
    }
  }
  unit.conditions &= ~0x200u;
}
void ground_movement::past_waypoint(actor_state &unit, int32_t const movement) {
  if(unit.destroyed || !unit.waypoint) return;
  auto const &route{group.definition.waypoints};
  size_t const previous{*unit.waypoint};
  int32_t const old_speed{unit.speed};
  environment.set_position(unit, route[previous].position);
  if(task.battle_cell && route[previous].type == 4) {
    unit.conditions &= ~6u;
    zero_speed(unit, true);
    return;
  }
  if(unit.conditions & 2) {
    unit.waypoint = previous ? std::optional{previous - 1} : std::nullopt;
  } else {
    unit.waypoint = previous + 1 < route.size() ? std::optional{previous + 1} : task.type == 21 ? std::optional<size_t>{0} : std::nullopt;
  }
  if(!unit.waypoint) {
    unit.route_distance = 0;
    unit.waypoint = previous;
    unit.speed = 0;
  } else {
    auto const &next{route[*unit.waypoint]};
    auto const &position{unit.object.world.position};
    unit.speed = next.speed;
    auto const dx{math::subtract(next.position[0], position[0])};
    auto const dy{math::subtract(next.position[1], position[1])};
    auto const dz{math::subtract(next.position[2], position[2])};
    unit.angles[0] = trig.arctangent(dx, dy);
    unit.angles[1] = dz ? trig.arctangent(dz, approximate_distance(dx, dy)) : 0;
    int32_t const remainder{math::subtract(movement, unit.route_distance)};
    translate(unit, {math::multiply_q31(trig.sine(unit.angles[0]), remainder), math::multiply_q31(trig.cosine(unit.angles[0]), remainder), math::multiply_q31(trig.sine(unit.angles[1]), remainder)}, environment);
    unit.route_distance = distance(next.position, unit.object.world.position);
  }
  unit.velocity = {math::multiply_q31(trig.sine(unit.angles[0]), unit.speed), math::multiply_q31(trig.cosine(unit.angles[0]), unit.speed), math::multiply_q31(trig.sine(unit.angles[1]), unit.speed)};
  unit.object.world.rotation = trig.transformation(unit.angles[0], unit.angles[1], unit.angles[2]);
  if(old_speed != unit.speed) {
    for(auto id : members(unit)) {
      auto &member{units[id]};
      if(member.destroyed) continue;
      for(auto &component : member.velocity) component = ratio(component, unit.speed, old_speed);
      member.speed = unit.speed;
    }
  }
  check_type(unit);
}
void ground_movement::members_past_waypoint(actor_state &leader, int32_t const movement, int32_t const previous_distance) {
  auto const following{members(leader)};
  if(formation.empty()) return;
  for(size_t i{following.size()}; i-- > 0;) {
    auto &member{units[following[i]]};
    if(member.destroyed) continue;
    environment.set_position(member, formation_point(leader.object.world.position, i + 1, leader.angles[0]));
    int32_t range{0};
    if(leader.speed) {
      auto const target{drive_to(leader, i + 1)};
      member.angles[0] = trig.arctangent(math::subtract(target[0], member.object.world.position[0]), math::subtract(target[1], member.object.world.position[1]));
      range = distance(target, member.object.world.position);
      auto const speed{ratio(leader.speed, range, leader.route_distance)};
      member.velocity[0] = math::multiply_q31(trig.sine(member.angles[0]), speed);
      member.velocity[1] = math::multiply_q31(trig.cosine(member.angles[0]), speed);
      auto const remaining{math::subtract(movement, previous_distance)};
      translate(member, {ratio(math::multiply_q31(member.velocity[0], delta), remaining, movement), ratio(math::multiply_q31(member.velocity[1], delta), remaining, movement), 0}, environment);
      member.object.world.rotation = trig.transformation(member.angles[0], member.angles[1], member.angles[2]);
    }
    // The original leaves this distance uninitialized when its leader stops.
    member.route_distance = range;
    member.speed = leader.speed;
  }
}
void ground_movement::move(actor_state &unit, actor_state const &leader, bool const with_members) {
  if(unit.destroyed) return;
  if(!leader.speed) {
    unit.speed = 0;
    return;
  }
  int32_t const movement{math::multiply_q31(leader.speed, delta)};
  int32_t const previous_distance{unit.route_distance};
  if(movement < previous_distance) {
    unit.route_distance = math::subtract(unit.route_distance, math::multiply_q31(unit.speed, delta));
    advance(unit, delta, environment);
    if(with_members) {
      auto const following{members(unit)};
      for(size_t i{following.size()}; i-- > 0;) {
        if(!units[following[i]].destroyed) advance(units[following[i]], delta, environment);
      }
    }
  } else {
    past_waypoint(unit, movement);
    if(with_members) members_past_waypoint(unit, movement, previous_distance);
  }
}
void ground_movement::finish_battle(bool const advance, int32_t const delta_time) {
  if(group.units.empty()) return;
  auto &leader{units[group.units.front()]};
  auto const &route{group.definition.waypoints};
  leader.conditions = (leader.conditions & ~6u) | (advance ? 4u : 2u);
  if(leader.waypoint && *leader.waypoint < route.size()) {
    auto const current{*leader.waypoint};
    if(advance && current + 1 < route.size()) {
      leader.waypoint = current + 1;
    } else if(!advance && current) leader.waypoint = current - 1;
    auto const &point{route[*leader.waypoint]};
    leader.speed = leader.destroyed ? 0 : point.speed;
    leader.route_distance = std::max(1, distance(point.position, leader.object.world.position));
    leader.angles[0] = trig.arctangent(math::subtract(point.position[0], leader.object.world.position[0]), math::subtract(point.position[1], leader.object.world.position[1]));
  } else {
    leader.speed = 0;
    leader.route_distance = 1;
  }
  leader.velocity = {math::multiply_q31(trig.sine(leader.angles[0]), leader.speed), math::multiply_q31(trig.cosine(leader.angles[0]), leader.speed), math::multiply_q31(trig.sine(leader.angles[1]), leader.speed)};
  delta = delta_time;
  members_past_waypoint(leader, 1, 0);
}
void ground_movement::update(int32_t const now, int32_t const delta_time) {
  if(group.units.empty() || task.type == 10 || task.type == 11 || task.type == 16 || task.type == 20) return;
  auto &leader{units[group.units.front()]};
  if(leader.classification != 3 && leader.classification != 4 && leader.classification != 8) return;
  if(leader.network != -1 || group.definition.start_time > now) return;
  delta = delta_time;
  if(!(leader.conditions & 0x40000)) {
    starting_speed(leader);
    if(task.type == 8) {
      leader.velocity = {math::multiply_q31(trig.sine(leader.angles[0]), leader.speed), math::multiply_q31(trig.cosine(leader.angles[0]), leader.speed), math::multiply_q31(trig.sine(leader.angles[1]), leader.speed)};
      for(auto id : members(leader)) units[id].velocity = leader.velocity;
    }
  }
  if(leader.classification == 8) {
    if(leader.destroyed) return;
    move(leader, leader, false);
    for(auto id : members(leader)) {
      if(units[id].destroyed) break;
      move(units[id], leader, false);
    }
  } else if(!task.battle_cell || (leader.conditions & 6)) {
    bool const independent{(leader.conditions & 0x200) != 0};
    move(leader, leader, !independent);
    if(independent) {
      auto const following{members(leader)};
      for(size_t i{following.size()}; i-- > 0;) move(units[following[i]], leader, false);
    }
  }
  for(auto id : group.units) {
    if(!units[id].carrier_slot) units[id].object.relative = units[id].object.world;
  }
}
} // namespace apache::simulation
