#include "combat_air_patrol.h"
#include <algorithm>
#include <bit>

namespace apache::simulation {
namespace {
int32_t range3(math::vector3 v) {
  for(auto &x : v) if(x < 0) x = math::negate(x);
  std::ranges::sort(v); return math::add(math::add(v[2], v[1] >> 2), v[0] >> 2);
}
int32_t patrol_range(math::vector3 const &from, math::vector3 const &to) {
  int32_t const y{math::subtract(to[1], from[1])};
  // Acquisition supplies the horizontal Y difference for both Y and Z.
  return range3({math::subtract(to[0], from[0]), y, y});
}
int32_t expanded_range(int32_t const value) {
  int64_t const scaled{int64_t{value} * 3 / 2};
  return scaled >= INT32_MIN && scaled <= INT32_MAX ? static_cast<int32_t>(scaled) : INT32_MIN;
}
} // anonymous namespace
void patrol_contacts::insert(unsigned int const side, shared_target target) {
  if(side < 1 || side > 2 || !target) return;
  auto &list{visible[side - 1]}; if(list.size() < 40) list.push_back(std::move(target));
}
void scan_patrol_contacts(patrol_contacts &contacts, int32_t const now, std::function<patrol_sector(int32_t, int32_t)> const &sector, std::function<int32_t(int32_t, int32_t, unsigned int)> const &radar_status, patrol_radar_callbacks const &radar) {
  for(auto &list : contacts.visible) list.clear();
  for(int32_t x{0}; x != 64; ++x) for(int32_t y{0}; y != 64; ++y) {
    auto const cell{sector(x, y)}; if(!cell.present[0] || !cell.present[1]) continue;
    for(unsigned int side{1}; side <= 2; ++side) {
      if(radar_status(x, y, side) != 0) continue;
      auto const &list{cell.air[2 - side]};
      for(size_t i{0}; i != std::min<size_t>(8, list.size()); ++i) if(list[i]) {
        auto const p{radar.position(*list[i])};
        if(radar.floor(p, side) < p[2] && cell.owner == static_cast<int32_t>(side)) contacts.insert(side, list[i]);
      }
    }
  }
  contacts.last_scan = now;
}
shared_target select_patrol_target(patrol_contacts &contacts, actor_state const &actor, int32_t const attention, flight_path const &path, unsigned int const slot, int32_t const now, math::trigonometry const &trig, std::function<void()> const &scan, std::function<math::vector3(target_reference const &)> const &position) {
  if(math::add(contacts.last_scan, 2000) < now && scan) scan();
  auto const side{(actor.object.status & 6) == 2 ? 0u : 1u};
  auto const &list{contacts.visible[side]}; if(list.empty()) return nullptr;
  int32_t closest{INT32_MAX}; shared_target result;
  for(auto const &candidate : list) if(candidate && actor.speed) {
    auto const p{position(*candidate)};
    if(distance_from_flight_path(path, p, trig) > math::add(attention, attention)) continue;
    math::vector3 delta; for(size_t i{0}; i != 3; ++i) delta[i] = math::subtract(p[i], actor.object.world.position[i]);
    int32_t const distance{range3(delta)};
    if(distance < closest) { closest = distance; result = candidate; }
  }
  if(slot < 20) contacts.assigned[side][slot] = result;
  return result;
}
void acquire_patrol_target(actor_state &actor, actor_armament &armament, shared_target const &assigned, int32_t const now, patrol_acquisition_callbacks const &callbacks) {
  unsigned int const side{(actor.object.status & 6) == 2 ? 1u : 2u};
  auto const release{[&]{ release_actor_target(actor, armament); if(callbacks.released) callbacks.released(); }};
  if(!armament.target) {
    if(callbacks.basic) callbacks.basic();
    if(armament.target) { actor.conditions &= ~0x100u; return; }
    armament.target = callbacks.select ? callbacks.select() : nullptr;
    if(!armament.target) return;
    if(actor.classification == 1 && callbacks.actor_class && callbacks.actor_class(*armament.target) != 1) { armament.target.reset(); actor.conditions &= ~0x100u; return; }
    actor.conditions = (actor.conditions | 0x108) & ~2u;
    int32_t const distance{patrol_range(actor.object.world.position, callbacks.radar.position(*armament.target))};
    // Selection excludes stopped aircraft; keep direct callers bounded as well.
    int64_t const travel{actor.speed ? int64_t{distance} / actor.speed : 0};
    actor.retreat_until = math::add(now, std::bit_cast<int32_t>(static_cast<uint32_t>(travel) * 1000u)); return;
  }
  if(!(actor.conditions & 0x100)) return;
  if(armament.target->identity.kind == target_kind::fixed) {
    if(!assigned) { release(); return; }
    auto const p{callbacks.radar.position(*assigned)}; int32_t const distance{patrol_range(actor.object.world.position, p)};
    int32_t const floor{callbacks.radar.floor(p, side)}; auto const radar{callbacks.radar.radar(p, side)};
    bool recover{floor < p[2]};
    if(!recover) {
      auto const last{callbacks.radar.position(*armament.target)}; int32_t const last_distance{patrol_range(actor.object.world.position, last)};
      int32_t const range{expanded_range(callbacks.weapon_range())};
      if(last_distance >= range) return;
      recover = distance < range && radar && callbacks.radar.status(*radar) == 0;
    }
    if(recover) armament.target = assigned;
    else release();
  } else {
    auto const p{callbacks.radar.position(*armament.target)}; int32_t const distance{patrol_range(actor.object.world.position, p)};
    int32_t const range{expanded_range(callbacks.weapon_range())};
    int32_t const floor{callbacks.radar.floor(p, side)}; auto const radar{callbacks.radar.radar(p, side)};
    if((p[2] < floor && distance > range) || (radar && callbacks.radar.status(*radar) == 1)) {
      armament.target = callbacks.fixed ? callbacks.fixed(p) : nullptr;
    }
  }
}
} // namespace apache::simulation
