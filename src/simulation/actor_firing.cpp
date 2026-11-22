#include "actor_firing.h"
#include <algorithm>

namespace apache::simulation {
void refresh_weapon_ranges(actor_state &actor, actor_armament &armament, actor_type const &type, std::array<weapon_info, 55> const &weapons) {
  auto const weapon{[&](size_t slot)->weapon_info const & {return weapons.at(static_cast<size_t>(type.weapons[slot]));}};
  armament.air_maximum = 0;
  if(actor.conditions & 0x10000) {
    actor.conditions &= ~0x10000u;
    for(size_t slot{5}; slot-- > 2;) {
      if(actor.ammunition[slot] > 0) {
        armament.air_maximum = weapon(slot).maximum_range;
        actor.conditions |= 0x10000;
        break;
      }
    }
    if(actor.ammunition[0] > 0) {
      armament.air_maximum = std::max(armament.air_maximum, weapon(0).maximum_range);
      actor.conditions |= 0x10000;
    }
  }
  armament.air_minimum = 0;
  if(actor.conditions & 0x10000) {
    actor.conditions &= ~0x10000u;
    // The original minimum is uninitialized when only a gun remains. Use its actual envelope.
    int32_t minimum{INT32_MAX};
    for(size_t slot{2}; slot != 5 && type.weapons[slot]; ++slot) {
      if(actor.ammunition[slot] > 0) {
        minimum = weapon(slot).minimum_range;
        actor.conditions |= 0x10000;
        break;
      }
    }
    if(actor.ammunition[0] > 0) {
      minimum = std::min(minimum, weapon(0).minimum_range);
      actor.conditions |= 0x10000;
    }
    armament.air_minimum = minimum == INT32_MAX ? 0 : minimum;
  }
  armament.ground_maximum = 0;
  if(actor.conditions & 0x20000) {
    actor.conditions &= ~0x20000u;
    for(size_t slot{0}; slot != 2; ++slot) {
      if(actor.ammunition[slot] > 0) {
        armament.ground_maximum = std::max(armament.ground_maximum, weapon(slot).maximum_range);
        actor.conditions |= 0x20000;
      }
    }
  }
  armament.ground_minimum = 0;
  if(actor.conditions & 0x20000) {
    actor.conditions &= ~0x20000u;
    int32_t minimum{INT32_MAX};
    for(size_t slot{0}; slot != 2; ++slot) {
      if(actor.ammunition[slot] > 0) {
        minimum = std::min(minimum, weapon(slot).minimum_range);
        actor.conditions |= 0x20000;
      }
    }
    armament.ground_minimum = minimum;
  }
}
void refresh_weapon_supply(actor_state &actor, actor_armament &armament, actor_type const &type, std::array<weapon_info, 55> const &weapons) {
  size_t count{0};
  size_t last{0};
  for(size_t slot{0}; slot != 5; ++slot) {
    if(actor.ammunition[slot] > 0) {
      ++count;
      last = slot;
    }
  }
  if(!count) {
    actor.weapon = 0;
    actor.weapon_slot = -1;
    actor.conditions &= ~0xc0u;
  } else if(count == 1) {
    actor.conditions |= 0xc0;
    actor.weapon = type.weapons[last];
    actor.weapon_slot = static_cast<int32_t>(last);
  } else {
    actor.conditions |= 0x40;
  }
  if((type.weapons[1] == 28 || type.weapons[1] == 29) && actor.ammunition[1] <= 0) actor.conditions |= 0x20;
  refresh_weapon_ranges(actor, armament, type, weapons);
}
int32_t choose_actor_weapon(actor_state &actor, actor_armament const &armament, actor_type const &type, std::array<weapon_info, 55> const &weapons, movement_target const &target, int32_t const task) {
  if(!(actor.conditions & 0x40) || !armament.target) return 0;
  if(actor.conditions & 0x80) return actor.weapon;
  math::vector3 difference;
  for(size_t i{0}; i != 3; ++i) {
    difference[i] = math::subtract(target.position[i], actor.object.world.position[i]);
    if(difference[i] < 0) difference[i] = math::negate(difference[i]);
  }
  std::ranges::sort(difference);
  int32_t const distance{math::add(math::add(difference[2], difference[1] >> 2), difference[0] >> 2)};
  weapon_selection selection{actor.conditions, task, distance, armament.air_minimum, armament.air_maximum, actor.ammunition, actor.weapon, actor.weapon_slot};
  if(target.classification == 1) {
    select_ground_weapon(selection, type, weapons);
  } else if(target.classification == 2) {
    select_air_weapon(selection, type, weapons);
  } else {
    return 0;
  }
  actor.weapon = selection.weapon;
  actor.weapon_slot = selection.slot;
  return actor.weapon;
}
void release_actor_target(actor_state &actor, actor_armament &armament) {
  armament.target.reset();
  actor.retreat_until = 0;
  actor.conditions &= ~0x11au;
}
bool fire_actor(actor_state &actor, actor_armament &armament, actor_type const &type, std::array<weapon_info, 55> const &weapons, math::trigonometry const &trig, forces_info const &skill, combat_random &random, actor_firing_context const &context, actor_firing_callbacks const &callbacks) {
  auto const choose{[&]{return choose_actor_weapon(actor, armament, type, weapons, context.target, context.task);}};
  int32_t const id{choose()};
  if(!id || !armament.target || actor.weapon_slot < 0 || actor.weapon_slot >= 5) return false;
  auto const &weapon{weapons.at(static_cast<size_t>(id))};
  size_t const slot{static_cast<size_t>(actor.weapon_slot)};
  if(!can_fire({true, actor.conditions, id, static_cast<int32_t>(armament.target->identity.kind), context.task, actor.ammunition[slot], armament.deadlines[slot], context.now, context.weather}, weapon)) return false;
  projectile_request request;
  request.weapon = static_cast<unsigned int>(id);
  request.source = context.source;
  request.launch.source.position = actor.object.world.position;
  request.launch.source.speed = actor.speed;
  std::copy_n(actor.object.world.rotation.begin() + 3, 3, request.launch.source.direction.begin());
  if(weapon.movement_class == 3) {
    request.launch.pose.position = ballistic_aim(weapon, actor.object.world.position, context.target, armament.target->identity.kind == target_kind::mobile);
  } else {
    request.selected = *armament.target;
    if(id == 29) {
      auto const pose{laser_bomb_aim(actor.object.world, context.target.position, weapon, trig)};
      if(!pose) return false;
      request.launch.pose = *pose;
      request.designator = context.player;
    } else {
      if(!(weapon.flags & 2) && armament.round) return false;
      auto const aim{aim_weapon(weapon, {request.launch.source, context.target, context.source_height, context.night}, trig, callbacks.visible)};
      armament.round.reset();
      if(aim.status == firing_status::lost_target) {
        release_actor_target(actor, armament);
        if(callbacks.target_released) callbacks.target_released();
        return false;
      }
      if(aim.status != firing_status::ready) return false;
      request.launch.pose = aim.pose;
      if(weapon.movement_class != 2) request.selected.reset();
    }
  }
  auto const launched{callbacks.construct ? callbacks.construct(request) : std::nullopt};
  if(launched && callbacks.launch) callbacks.launch(*launched);
  if(weapon.movement_class != 3) {
    armament.round = launched;
    if(!launched) return false;
    if(id == 29) {
      if(callbacks.bomb_released) callbacks.bomb_released();
      if(callbacks.remove_flame) callbacks.remove_flame(*launched);
    }
  }
  if(choose() && actor.weapon_slot >= 0 && actor.weapon_slot < 5) --actor.ammunition[static_cast<size_t>(actor.weapon_slot)];
  int32_t const remaining{choose() && actor.weapon_slot >= 0 && actor.weapon_slot < 5 ? actor.ammunition[static_cast<size_t>(actor.weapon_slot)] : 0};
  if(remaining) {
    armament.deadlines[static_cast<size_t>(actor.weapon_slot)] = math::add(context.now, firing_interval(weapon, remaining, skill, random));
  } else {
    refresh_weapon_supply(actor, armament, type, weapons);
  }
  if(armament.target->identity.kind == target_kind::mobile && callbacks.fired_at) callbacks.fired_at(*armament.target, context.source);
  return true;
}
} // namespace apache::simulation
