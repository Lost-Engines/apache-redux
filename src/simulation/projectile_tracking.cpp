#include "projectile_tracking.h"

namespace apache::simulation {
namespace {
target_description describe_copy(target_world const &world, target_reference const &target, int32_t now) {
  auto copy{target}; return world.describe(copy, now);
}
std::shared_ptr<target_reference> linked(target_slot_lookup const &slots, target_reference const &owner, target_slot slot) {
  return slots && owner.identity.kind == target_kind::mobile ? slots(owner.identity.index, slot) : nullptr;
}
target_reference *resolve(projectile_record &round, projectile_target_binding const &binding, target_world const &world) {
  auto *target{binding.selected ? (round.selected ? &*round.selected : nullptr) : binding.target.get()};
  return target && world.valid(*target) ? target : nullptr;
}
} // anonymous namespace
projectile_target_binding selected_projectile_target(projectile_record &round, weapon_info const &weapon, target_world const &world, target_slot_lookup const &slots) {
  if(weapon.movement_class != 2) return {};
  auto *target{round.selected ? &*round.selected : nullptr};
  if(weapon.guidance == 1 && round.designator && round.designator->identity.kind == target_kind::mobile && (!target || !describe_copy(world, *target, 0).designated)) return {false, linked(slots, *round.designator, target_slot::current)};
  return {true, {}};
}
target_reference *adjusted_projectile_target(projectile_record &round, target_world const &world, target_slot_lookup const &) {
  return resolve(round, round.adjusted, world);
}
bool projectile_has_target(projectile_record &round, weapon_info const &weapon, target_world const &world, target_slot_lookup const &slots, int32_t const now) {
  auto &state{round.state.body.seeker};
  if(weapon.movement_class != 2 || !(state.flags & 1)) return false;
  if(!round.designator) {
    bool const valid{round.selected && describe_copy(world, *round.selected, now).status == 0};
    state.flags = (state.flags & ~1u) | static_cast<uint32_t>(valid); return valid;
  }
  if(describe_copy(world, *round.designator, now).status == 1) { state.flags &= ~1u; return false; }
  return linked(slots, *round.designator, target_slot::current) != nullptr;
}
projectile_aim track_projectile(projectile_record &round, weapon_info const &weapon, target_world const &world, target_slot_lookup const &slots, resources::terrain const &terrain, tracking_environment const &environment, uint32_t &random_seed) {
  auto const selected{selected_projectile_target(round, weapon, world, slots)};
  auto *target{resolve(round, selected, world)};
  if(target) {
    auto const description{describe_copy(world, *target, environment.now)};
    auto const source{describe_copy(world, round.source, environment.now)};
    auto const designator{round.designator ? describe_copy(world, *round.designator, environment.now) : target_description{}};
    auto &state{round.state.body.seeker};
    seeker_inputs input;
    input.now = environment.now; input.weather = environment.weather; input.cloud_bottom = environment.cloud_bottom; input.cloud_top = environment.cloud_top;
    input.target_status = description.status; input.source_side = source.side;
    input.adjusted_valid = adjusted_projectile_target(round, world, slots) != nullptr;
    input.has_designator = designator.valid; input.ground_designated = description.designated;
    input.designator_active = round.designator && linked(slots, *round.designator, target_slot::current);
    input.designator_destroyed = designator.destroyed;
    input.chaff = description.chaff; input.flare = description.flare; input.source_radar = source.radar;
    input.missile_position = round.state.body.pose.position; input.target_position = description.movement.position;
    input.source_position = source.movement.position; input.designator_position = designator.movement.position;
    combat_random random{round.state.random, random_seed};
    seeker_queries queries;
    queries.visible = [&](auto const &from, auto const &to){ return terrain_visibility(from, to, 3, true, terrain.maximum_height(), [&](int32_t x, int32_t y){ return terrain.elevation(x, y); }).clear; };
    queries.distracted = [&]{ return decoy_distracts(weapon, environment.mode, source.side == 2, environment.decoy_modifier, random); };
    queries.random = [&]{ return random.next(); };
    queries.classification = [&]{ return world.describe(*target, environment.now).movement.classification; };
    if(update_seeker(state, weapon, input, queries)) {
      switch(state.adjusted) {
        case seeker_target::none: round.adjusted = {}; break;
        case seeker_target::selected: round.adjusted = selected; break;
        case seeker_target::chaff: round.adjusted = {false, linked(slots, *target, target_slot::chaff)}; break;
        case seeker_target::flare: round.adjusted = {false, linked(slots, *target, target_slot::flare)}; break;
      }
    }
    round.state.random = random.value; random_seed = random.global_seed;
  }
  auto *adjusted{adjusted_projectile_target(round, world, slots)};
  if(!adjusted) return {};
  auto const description{world.describe(*adjusted, environment.now)};
  return {description.movement, description.actor};
}
} // namespace apache::simulation
