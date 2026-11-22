#include "targets.h"
#include <algorithm>
#include <cstdlib>

namespace apache::simulation {
bool equivalent_targets(target_reference const &first, target_reference const &second) {
  auto const &a{first.identity};
  auto const &b{second.identity};
  if(a.kind != b.kind) return false;
  if(a.kind == target_kind::fixed) return first.position == second.position;
  return a.index == b.index && (a.kind != target_kind::scenery || a.layer == b.layer);
}
bool within_target_sectors(math::vector3 const &first, math::vector3 const &second, int32_t const range) {
  int32_t sectors{range / 0x400000};
  if(range % 0x400000 > 0x200000) ++sectors;
  return std::abs((first[0] >> 22) - (second[0] >> 22)) <= sectors && std::abs((first[1] >> 22) - (second[1] >> 22)) <= sectors;
}
target_world::target_world(resources::terrain const &ground, scenery_collisions const &structures, render::mobile_scene const &objects, resources::model_archive const &definitions, math::trigonometry const &angles, std::function<mobile_target_state(uint32_t)> state)
  : terrain{ground},
    scenery{structures},
    mobiles{objects},
    models{definitions},
    trig{angles},
    mobile_state{std::move(state)} {}
bool target_world::valid(target_reference const &target) const {
  auto const &id{target.identity};
  switch(id.kind) {
  case target_kind::scenery:
    return scenery.contains(id.layer, id.index);
  case target_kind::mobile:
    return mobiles.contains(id.index);
  case target_kind::fixed:
    return true;
  default:
    return false;
  }
}
math::vector3 target_world::position(target_reference const &target) const {
  if(!valid(target)) return {};
  if(target.identity.kind == target_kind::mobile) return mobiles.at(target.identity.index).world.position;
  auto p{target.position};
  if(target.identity.kind == target_kind::scenery) p[2] = 0;
  return p;
}
target_description target_world::describe(target_reference const &target, int32_t const now) const {
  target_description result;
  result.valid = valid(target);
  if(!result.valid) return result;
  bool user_named{false};
  result.status = 0;
  result.movement.position = position(target);
  auto const &id{target.identity};
  if(id.kind == target_kind::fixed) {
    result.movement.direction = {0, 1, 0};
    if(!target.classification) target.classification = target.position[2] == terrain.elevation(target.position[0], target.position[1]) ? 1 : 2;
    result.name = result.description = "Fixed point target";
  } else if(id.kind == target_kind::scenery) {
    auto const &object{scenery.at(id.layer, id.index)};
    result.model = object.model;
    result.status = object.status & 1;
    result.side = (object.status & 7) >> 1;
    result.damage = (object.status & 0x7ff) >> 3;
    result.designated = (object.status & 0x800) != 0;
    result.movement.direction = {trig.sine(0), trig.cosine(0), 0};
    if(!target.classification) target.classification = 1;
  } else {
    auto const &object{mobiles.at(id.index)};
    auto const state{mobile_state(id.index)};
    result.model = object.id;
    result.side = (object.status & 7) >> 1;
    std::copy_n(object.world.rotation.begin() + 3, 3, result.movement.direction.begin());
    if(state.type == 1) {
      result.actor = true;
      result.destroyed = state.destroyed != 0;
      result.status = object.status & 1;
      result.damage = state.damage;
      result.movement.speed = state.player ? std::bit_cast<int32_t>(static_cast<uint32_t>(state.player_airspeed) * 432u) : state.speed;
      if(!target.classification) target.classification = state.classification == 1 || state.classification == 2 ? 2 : 1;
      bool const radar_model{object.id == 46 || object.id == 77 || object.id == 111 || object.id == 328 || object.id == 526};
      result.radar = (state.player && (state.ground_radar || state.air_radar)) || state.classification == 1 || state.classification == 2 || (radar_model && !state.destroyed);
      result.designated = (state.conditions & 1) != 0;
      static auto const weapons{weapon_definitions()};
      result.chaff = state.chaff_at && state.chaff_valid && math::subtract(math::subtract(now, state.chaff_at), weapons[7].fire_interval_ticks) <= 1000;
      result.flare = state.flare_at && state.flare_valid && math::subtract(math::subtract(now, state.flare_at), weapons[8].fire_interval_ticks) <= 1000;
      if(!state.user_name.empty()) {
        user_named = true;
        result.name = result.description = state.user_name;
      }
    } else if(state.type == 2) {
      result.movement.speed = state.speed;
      if(!target.classification) target.classification = 2;
    } else {
      result.valid = false;
      result.status = 1;
    }
  }
  result.movement.classification = target.classification;
  if(id.kind != target_kind::fixed && !user_named && result.model < models.size() && models[result.model]) {
    result.name = models[result.model]->name;
    result.description = models[result.model]->description;
  }
  return result;
}
} // namespace apache::simulation
