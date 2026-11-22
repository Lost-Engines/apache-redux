#include "mission_targets.h"

namespace apache::simulation {
mission_targets::mission_targets(resources::mission_definition const &definition, mission_scenery const &ground, mission_actors const &units, mission_spatial const &locations, combat_random &rng, resources::terrain const &terrain, scenery_collisions const &structures, render::mobile_scene const &mobiles, resources::model_archive const &models, math::trigonometry const &trig)
  : mission{definition}, scenery{ground}, actors{units}, spatial{locations}, random{rng}, world{terrain, structures, mobiles, models, trig, [this](uint32_t id){ return mobile_state(id); }} {}
mobile_target_state mission_targets::mobile_state(uint32_t const id) const {
  auto const index{spatial.actor_index(id)};
  mobile_target_state state{additional_state ? additional_state(id) : mobile_target_state{}};
  if(!index) { if(!additional_state) state.type = 0; return state; }
  auto const &actor{actors.units[*index]};
  state.type = 1; state.classification = static_cast<int32_t>(actor.classification); state.speed = actor.speed; state.destroyed = actor.destroyed; state.conditions = actor.conditions; state.player = spatial.player == index;
  return state;
}
sector_search_inputs mission_targets::sector(int32_t const x, int32_t const y, int32_t const side) const {
  sector_search_inputs result;
  if(x < 0 || y < 0 || x >= 64 || y >= 64) return result;
  if(auto const *cell{spatial.grid.sector(x, y, side == 1 ? 1u : 2u)}) { result.air = cell->air.candidates(); result.ground = cell->ground.candidates(); }
  auto const &ground{scenery.sectors[static_cast<size_t>(x * 64 + y)]}; result.scenery_side = ground.side;
  for(size_t i{0}; i != 2; ++i) {
    if(auto const object{ground.objects[i]}) result.guns[i] = scenery.objects[*object].combat.self;
    if(auto const object{ground.objects[i + 2]}) result.missiles[i] = scenery.objects[*object].combat.self;
  }
  return result;
}
shared_target mission_targets::in_sector(int32_t const x, int32_t const y, int32_t const side, math::vector3 const &origin, search_kind const kind, bool const randomised) {
  if(x < 0 || y < 0 || x >= 64 || y >= 64) return {};
  target_search_callbacks callbacks;
  callbacks.position = [&](target_reference const &target){ return world.position(target); };
  callbacks.status = [&](target_reference const &target){ return describe(target).status; };
  callbacks.mobile_air_defence = [&](target_reference const &target){ auto const index{spatial.actor_index(target.identity.index)}; return index && mission.tasks[actors.units[*index].location.task].type == 19; };
  callbacks.random = [&]{ return random.next(); };
  return choose_sector_target(sector(x, y, side), origin, side, kind, randomised, callbacks);
}
shared_target mission_targets::search(math::vector3 const &origin, int32_t const side, int32_t const minimum, int32_t const maximum, search_kind const kind, bool const randomised) {
  return search_sector_rings(origin, side, minimum, maximum, [&](int32_t x, int32_t y, int32_t faction){ return in_sector(x, y, faction, origin, kind, randomised); }, [&]{ return random.next(); });
}
} // namespace apache::simulation
