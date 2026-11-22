#include "projectiles.h"
#include <algorithm>
#include <stdexcept>

namespace apache::simulation {
namespace {
int32_t approximate_range(math::vector3 v) {
  for(auto &x : v) {
    if(x < 0) x = math::negate(x);
  }
  std::ranges::sort(v);
  return math::add(math::add(v[2], v[1] >> 2), v[0] >> 2);
}
bool outside(math::vector3 const &p) {
  return p[0] < 0 || p[0] > 0xfffffff || p[1] < 0 || p[1] > 0xfffffff;
}
}
projectiles::projectiles(resources::model_archive const &objects, math::trigonometry const &angles, combat_tables const &performance, resources::terrain const &ground, scenery_collisions const &structures, render::mobile_scene &scene, effects &puffs, uint32_t &seed, projectile_callbacks events)
  : weapons{weapon_definitions()},
    models{objects},
    trig{angles},
    tables{performance},
    terrain{ground},
    scenery{structures},
    mobiles{scene},
    smoke{puffs},
    random_seed{seed},
    callbacks{std::move(events)} {}
std::optional<size_t> projectiles::construct(projectile_request const &request) {
  if(count == rounds.size()) return {};
  size_t index{cursor % rounds.size()};
  for(size_t i{0}; i != rounds.size(); ++i, index = (index + 1) % rounds.size()) {
    if(!rounds[index]) {
      projectile_record round;
      round.source = request.source;
      round.selected = request.selected;
      round.designator = request.designator;
      auto input{request.launch};
      input.has_target = request.selected.has_value();
      auto const &weapon{weapons.at(request.weapon)};
      construct_projectile(round.state, request.weapon, weapon, input, trig, [&](int32_t x, int32_t y){return terrain.elevation(x, y);}, random_seed, weapon_random);
      // Remote firing explicitly uses the guided constructor for weapon 10,
      // whose table entry otherwise selects unguided movement.
      if(request.guided_constructor) {
        round.state.body.motion.flags |= 4;
        round.state.body.seeker.flags = (round.state.body.seeker.flags & ~1u) | (input.has_target ? 1u : 0u);
        round.state.body.seeker.retry_time = 0;
      }
      if(weapon.movement_class != 3) {
        auto object{projectile_model(round.state, weapon, models)};
        round.mobile = mobiles.add(object.id, object.world);
        mobiles.at(*round.mobile) = std::move(object);
      }
      rounds[index] = std::move(round);
      cursor = index + 1;
      ++count;
      return index;
    }
  }
  return {};
}
void projectiles::launch(size_t const index, int32_t const now) {
  launch_projectile(at(index).state, now);
}
void projectiles::emit(size_t const index, projectile_event const kind, math::vector3 const &position, std::optional<target_identity> const target) {
  if(callbacks.event) callbacks.event(index, at(index), {kind, position, target});
}
void projectiles::remove(size_t const index) {
  auto &round{at(index)};
  emit(index, projectile_event::removed, round.state.body.pose.position);
  if(round.mobile) mobiles.remove(*round.mobile);
  rounds[index].reset();
  --count;
}
void projectiles::clear() {
  for(size_t i{0}; i != rounds.size(); ++i) {
    if(rounds[i]) {
      if(rounds[i]->mobile) mobiles.remove(*rounds[i]->mobile);
      rounds[i].reset();
    }
  }
  count = 0;
}
bool projectiles::collide(size_t const index, math::vector3 const &move) {
  auto &round{at(index)};
  auto const &start{round.state.body.pose.position};
  auto const bound{bound_projectile_move(start, move, [&](int32_t x, int32_t y){return terrain.elevation(x, y);})};
  if(!(round.state.body.motion.flags & 1)) return true;
  if(outside(bound.end)) {
    emit(index, projectile_event::map_edge, start);
    return true;
  }
  auto const &origin{round.source.identity};
  auto const hit{mobile_line_collision(mobiles, models, trig, start, bound.end, origin.kind == target_kind::mobile ? std::optional{origin.index} : std::nullopt, round.mobile)};
  if(hit && !(mobiles.at(*hit).status & 1)) {
    emit(index, projectile_event::mobile, mobiles.at(*hit).world.position, target_identity{target_kind::mobile, 0, *hit});
    return true;
  }
  auto const ground{scenery.line(start, bound.end, origin.kind == target_kind::scenery ? std::optional{std::array{origin.layer, static_cast<size_t>(origin.index)}} : std::nullopt)};
  if(ground && !(scenery.at(ground->layer, ground->index).status & 1)) {
    emit(index, projectile_event::scenery, ground->position, target_identity{target_kind::scenery, ground->layer, static_cast<uint32_t>(ground->index)});
    return true;
  }
  if(projectile_expired(round.state.body.motion, weapons.at(round.state.weapon))) {
    emit(index, projectile_event::expired, start);
    return true;
  }
  if(bound.hit_terrain) {
    emit(index, projectile_event::terrain, bound.impact);
    return true;
  }
  return false;
}
void projectiles::update(projectile_time const &time) {
  if(time.delta < 0 || time.seconds < 0 || time.smoke_density > 10) throw std::runtime_error{"Invalid projectile update time or smoke density"};
  size_t remaining{count};
  for(size_t index{0}; index != rounds.size() && remaining; ++index) {
    if(!rounds[index] || !(rounds[index]->state.body.motion.flags & 1)) continue;
    --remaining;
    auto &round{*rounds[index]};
    auto &state{round.state};
    auto &body{state.body};
    auto const &weapon{weapons.at(state.weapon)};
    if(weapon.movement_class == 3) {
      if(time.now >= state.impact_at) {
        emit(index, projectile_event::ballistic, state.impact);
        remove(index);
      }
      continue;
    }
    body.motion.age = math::subtract(time.now, state.launched_at);
    body.motion.altitude = body.pose.position[2];
    if(update_projectile_speed(body.motion, weapon, time.seconds, tables) && round.mobile) {
      auto &children{mobiles.at(*round.mobile).children};
      if(!children.empty()) children[0].reset();
    }
    auto const aim{weapon.movement_class == 2 && callbacks.guidance ? callbacks.guidance(round, time.now) : projectile_aim{}};
    if(weapon.movement_class == 2 && aim.target && aim.actor && body.motion.distance >= weapon.guidance_start_distance) emit(index, projectile_event::notify, body.pose.position);
    auto const step{calculate_projectile_move(body, weapon, aim.target, aim.actor, trig, time.delta)};
    if(body.seeker.adjusted == seeker_target::none) round.adjusted = {};
    if(round.mobile) mobiles.move(*round.mobile, body.pose);
    if(step.explosion) emit(index, projectile_event::proximity, *step.explosion);
    if(collide(index, step.displacement)) {
      remove(index);
      continue;
    }
    body.motion.distance = math::add(body.motion.distance, approximate_range(step.displacement));
    if((body.motion.flags & 2) && state.weapon != 29) {
      math::vector3 from;
      math::vector3 to;
      for(size_t i{0}; i != 3; ++i) {
        from[i] = math::subtract(body.pose.position[i], math::multiply_q31(body.pose.rotation[3 + i], 512));
        to[i] = math::add(from[i], step.displacement[i]);
      }
      smoke.add_trail(from, to, time.smoke_radius, time.smoke_lifetime, weapon.movement_class == 4 ? time.smoke_density / 2 : time.smoke_density, random_seed);
    }
    body.previous = body.pose.position;
    for(size_t i{0}; i != 3; ++i) body.pose.position[i] = math::add(body.pose.position[i], step.displacement[i]);
    if(round.mobile) {
      mobiles.move(*round.mobile, body.pose);
      render::maintain_model(mobiles.at(*round.mobile));
    }
  }
}
} // namespace apache::simulation
