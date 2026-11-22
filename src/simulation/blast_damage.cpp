#include "blast_damage.h"
#include <algorithm>
#include <stdexcept>

namespace apache::simulation {
namespace {
int32_t multiply(int32_t value, unsigned int factor) {
  return std::bit_cast<int32_t>(static_cast<uint32_t>(value) * factor);
}
bool same(target_identity const &a, target_identity const &b) {
  return a.kind == b.kind && a.index == b.index && (a.kind != target_kind::scenery || a.layer == b.layer);
}
}
blast_damage::blast_damage(unsigned int const id, damage_source const &origin, damage_environment const &settings, damage_random &rng, resources::model_archive const &definitions, scenery_collisions const &structures, render::mobile_scene const &objects, blast_callbacks handlers)
  : weapon_id{id},
    weapon{weapon_definitions().at(id)},
    source{origin},
    environment{settings},
    random{rng},
    models{definitions},
    scenery{structures},
    mobiles{objects},
    callbacks{std::move(handlers)} {
  if(!callbacks.read || !callbacks.write || !callbacks.action) throw std::runtime_error{"Blast damage requires live object handlers"};
}
int32_t blast_damage::damage_one(target_identity const &id, math::vector3 const &impact) {
  auto target{callbacks.read(id)};
  auto const result{apply_damage(target, models.at(target.model).value(), source, weapon_id, weapon, impact, limits, random, environment)};
  callbacks.write(id, target);
  for(auto action : result.actions) callbacks.action(id, action);
  return result.amount;
}
void blast_damage::damage_ground(math::vector3 const &impact, int32_t const radius, std::optional<target_identity> const ignored) {
  for(auto const &hit : scenery.in_range(impact, radius)) {
    target_identity const id{target_kind::scenery, hit.layer, static_cast<uint32_t>(hit.index)};
    if(ignored && same(id, *ignored)) continue;
    auto const target{callbacks.read(id)};
    if(!(target.flags & 1) && models.at(target.model)->metadata[3] != 255) damage_one(id, impact);
  }
}
void blast_damage::damage_mobiles(math::vector3 const &impact, int32_t const radius, std::optional<target_identity> const ignored) {
  for(auto const index : mobiles_in_range(mobiles, impact, radius)) {
    target_identity const id{target_kind::mobile, 0, index};
    if(ignored && same(id, *ignored)) continue;
    if(!(callbacks.read(id).flags & 1)) damage_one(id, impact);
  }
}
void blast_damage::detonate(math::vector3 const &impact) {
  limits = {};
  if(callbacks.weapon_explosion) callbacks.weapon_explosion(impact);
  damage_mobiles(impact, multiply(weapon.blast_radius, 5));
  damage_ground(impact, multiply(weapon.blast_radius, 2));
}
void blast_damage::hit(target_identity const &id, math::vector3 const &impact) {
  if(id.kind != target_kind::scenery && id.kind != target_kind::mobile) throw std::runtime_error{"Invalid direct-hit target"};
  limits = {};
  int32_t radius{multiply(weapon.blast_radius, 2)};
  auto const amount{damage_one(id, impact)};
  auto target{callbacks.read(id)};
  if(target.flags & 1) {
    radius = std::max(radius, multiply(models.at(target.model)->metadata[4], 2));
  } else if(amount > 10 && callbacks.partial_explosion) callbacks.partial_explosion(id);
  damage_ground(impact, radius, id.kind == target_kind::scenery ? std::optional{id} : std::nullopt);
  damage_mobiles(impact, radius, id.kind == target_kind::mobile ? std::optional{id} : std::nullopt);
  if((callbacks.read(id).flags & 1) && callbacks.auxiliary) callbacks.auxiliary(id);
}
} // namespace apache::simulation
