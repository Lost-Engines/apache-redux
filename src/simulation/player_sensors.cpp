#include "player_sensors.h"
#include <algorithm>

namespace apache::simulation {
namespace {
math::vector3 difference(math::vector3 const &a, math::vector3 const &b) {
  math::vector3 v;
  for(size_t i{0}; i != 3; ++i) v[i] = math::subtract(a[i], b[i]);
  return v;
}
int64_t magnitude(int32_t value) {
  return value < 0 ? -int64_t{value} : value;
}
math::vector3 horizontal(math::vector3 const &v, int32_t sine, int32_t cosine) {
  return {math::rounded_q31(math::product_bits(v[0], cosine) - math::product_bits(v[1], sine)), math::rounded_q31(math::product_bits(v[1], cosine) + math::product_bits(v[0], sine)), v[2]};
}
}
player_sensors::player_sensors(resources::terrain const &land, resources::model_archive const &definitions, mission_scenery const &scene, render::mobile_scene const &objects, mission_targets const &world, math::trigonometry const &angles, unsigned int const game_mode)
  : terrain{land}, models{definitions}, scenery{scene}, mobiles{objects}, targets{world}, trig{angles}, state{game_mode} {
  for(size_t layer{0}; layer != 2; ++layer) {
    auto const &placements{layer ? scenery.extra : scenery.base};
    for(size_t i{0}; i != placements.size(); ++i) sectors[layer][placements[i].sector].push_back(i);
  }
}
uint32_t player_sensors::model(target_reference const &target) const {
  if(target.identity.kind == target_kind::scenery) return (target.identity.layer ? scenery.extra : scenery.base).at(target.identity.index).model;
  return mobiles.at(target.identity.index).id;
}
void player_sensors::scan_contacts(math::pose const &body, std::optional<uint32_t> const player) {
  ground.clear(); mobile.clear(); missiles.clear();
  auto const visible{[&](math::vector3 const &position){ return terrain_visibility(body.position, position, 3, true, terrain.maximum_height(), [&](int32_t x, int32_t y){ return terrain.elevation(x, y); }).clear; }};
  int32_t const cx{body.position[0] >> 22}, cy{body.position[1] >> 22};
  for(int32_t x{std::max(0, cx - 3)}; x <= std::min(63, cx + 3); ++x) for(int32_t y{std::max(0, cy - 3)}; y <= std::min(63, cy + 3); ++y) {
    auto const sector{static_cast<size_t>(x * 64 + y)};
    for(size_t layer{0}; layer != 2; ++layer) for(size_t const index : sectors[layer][sector]) {
      if(ground.size() == 512) break;
      auto const &object{(layer ? scenery.extra : scenery.base)[index]};
      if((object.status & 1) || !(object.status & 6) || !models.at(object.model) || !models[object.model]->metadata[6]) continue;
      math::vector3 const position{object.x, object.y, terrain.elevation(object.x, object.y)};
      if(visible(position)) ground.push_back({{{target_kind::scenery, layer, static_cast<uint32_t>(index)}, {object.x, object.y, 0}, 1}, {x, y}, position});
    }
    for(uint32_t const id : mobiles.sector(sector)) {
      if(mobile.size() == 512) break;
      auto const &object{mobiles.at(id)};
      if(!models.at(object.id) || !models[object.id]->metadata[6]) continue;
      auto const info{targets.mobile_state(id)};
      if(!(object.status & 1) && (object.status & 6) && player != id) {
        if(!visible(object.world.position)) continue;
        int32_t const classification{(info.classification == 1 || info.classification == 2) && object.world.position[2] >= 0x1e00 ? 2 : 1};
        mobile.push_back({{{target_kind::mobile, 0, id}, {}, classification}, {x, y}, object.world.position});
      } else if((object.status & 1) && info.type == 2 && missiles.size() < 256 && visible(object.world.position)) missiles.push_back({{{target_kind::mobile, 0, id}, {}, 0}, {x, y}, object.world.position});
    }
  }
}
void player_sensors::insert(radar_return const &entry, math::pose const &body, uint32_t const heading) {
  if(!targets.world.valid(entry.target)) return;
  auto const id{model(entry.target)}; if(id >= models.size() || !models[id]) return;
  bool const is_ground{entry.target.identity.kind == target_kind::scenery};
  bool const marked{is_ground ? ((entry.target.identity.layer ? scenery.extra : scenery.base)[entry.target.identity.index].status & 0x800) != 0 : (targets.mobile_state(entry.target.identity.index).conditions & 1) != 0};
  auto item{entry}; item.priority = radar_target_priority({entry.target.identity.kind, static_cast<unsigned int>(entry.target.classification), models[id]->metadata[0], models[id]->metadata[6] != 0, marked, difference(is_ground ? entry.target.position : targets.world.position(entry.target), body.position), heading}, trig);
  if(item.priority == -1) return;
  auto const place{std::find_if(ranked.begin(), ranked.end(), [&](auto const &other){ return other.priority <= item.priority; })}; ranked.insert(place, item);
}
void player_sensors::scan_radars(math::pose const &body, uint32_t const heading, uint32_t const faults) {
  ranked.clear();
  bool const horizontal_direction{body.rotation[3] != 0 || body.rotation[4] != 0};
  if(!(faults & 0x200)) {
    if(state.ground) {
      if(!state.frozen) {
        ground_radar.clear();
        if(horizontal_direction) {
          auto const sine{trig.sine(heading)}, cosine{trig.cosine(heading)};
          math::vector3 const centre{math::add(body.position[0], math::multiply_q31(sine, state.ground_range)), math::add(body.position[1], math::multiply_q31(cosine, state.ground_range)), 0};
          auto const collect{[&](sensor_contact const &contact){
            if(ground_radar.size() >= 511 || magnitude(contact.sector[0] - (centre[0] >> 22)) >= 5 || magnitude(contact.sector[1] - (centre[1] >> 22)) >= 5) return;
            if(state.threat != 4 && target_filter_level(models.at(model(contact.target))->metadata[0]) != state.threat) return;
            auto const local{horizontal(difference(contact.position, centre), sine, cosine)};
            if(local[1] <= -state.ground_range || magnitude(local[0]) >= magnitude(math::add(local[1], state.ground_range)) || magnitude(local[0]) > state.ground_range || magnitude(local[1]) > state.ground_range) return;
            auto target{contact.target}; target.classification = 1;
            if(target.identity.kind == target_kind::scenery) target.position = contact.position;
            ground_radar.push_back({target, {local[0], local[1], target.identity.kind == target_kind::scenery ? 0 : contact.position[2]}});
          }};
          for(auto const &contact : ground) collect(contact);
          for(auto const &contact : mobile) if(contact.target.classification == 1) collect(contact);
        }
      }
      for(auto const &entry : ground_radar) insert(entry, body, heading);
    } else if(state.air) {
      if(!state.frozen) {
        air_radar.clear();
        if(horizontal_direction) for(auto const &contact : mobile) {
          if(air_radar.size() >= 256) break;
          if(contact.target.classification != 2) continue;
          auto const relative{difference(contact.position, body.position)};
          if(magnitude(relative[0]) >= state.air_range || magnitude(relative[1]) >= state.air_range || (!state.scan_all && (mobiles.at(contact.target.identity.index).status & 6) != 4)) continue;
          auto target{contact.target}; target.classification = 2; air_radar.push_back({target, relative});
        }
      }
      for(auto const &entry : air_radar) insert(entry, body, heading);
    }
  }
  if(state.tactical && horizontal_direction) {
    tactical.clear();
    auto const collect{[&](sensor_contact const &contact){
      if(tactical.size() >= 512) return;
      auto relative{difference(contact.position, body.position)};
      if(magnitude(relative[0]) >= state.tactical_range || magnitude(relative[1]) >= state.tactical_range) return;
      auto target{contact.target}; target.classification = 0;
      if(target.identity.kind == target_kind::scenery) relative[2] = 0;
      tactical.push_back({target, relative});
    }};
    for(auto const &contact : missiles) collect(contact);
    for(auto const &contact : mobile) collect(contact);
    for(auto const &contact : ground) collect(contact);
  }
}
void player_sensors::update(math::pose const &body, uint32_t const heading, std::optional<uint32_t> const player, unsigned int const control_mode, uint32_t const faults) {
  scan_contacts(body, player); scan_radars(body, heading, faults);
  if(control_mode) {
    std::vector<target_reference> ordered; ordered.reserve(ranked.size()); for(auto const &entry : ranked) ordered.push_back(entry.target);
    bool const radar_copy{state.automatic || (!state.tads && (state.cycle_direction || std::ranges::none_of(ranked, [&](auto const &entry){ return equivalent_targets(state.current, entry.target); })))};
    bool optical_copy{!state.automatic && state.tads && !state.locked};
    if(optical_copy && (optical.identity.kind == target_kind::scenery || optical.identity.kind == target_kind::mobile)) {
      auto copy{optical}; optical_copy = !(targets.describe(copy).status & 1);
    }
    // Name/status queries do not populate the original target classification cache.
    state.update(ordered, optical, [&](auto const &target){ auto copy{target}; return targets.describe(copy); });
    if(radar_copy && !ranked.empty()) {
      if(auto found{std::ranges::find_if(ranked, [&](auto const &entry){ return equivalent_targets(state.current, entry.target); })}; found != ranked.end()) current_display = found->display;
    } else if(optical_copy) current_display = {};
  }
}
void player_sensors::pick_optical(math::pose const &view, uint32_t const heading, render::projection const &projection, unsigned int const control_mode, uint32_t const faults) {
  optical_symbol.reset(); if(!control_mode) return;
  auto const lose_lock{[&]{ state.current.identity.kind = target_kind::none; state.locked = false; optical.identity.kind = target_kind::none; if(message) message("TARGET LOST"); }};
  if((faults & 0x2000) && state.tads) {
    if(state.locked) lose_lock();
    state.locked = state.tads = false; state.current.identity.kind = optical.identity.kind = target_kind::none; return;
  }
  auto const project{[&](math::vector3 const &position){
    auto const point{projection.project_unclipped(math::transform(view.rotation, difference(position, view.position)))};
    if(point && point->x > 0 && point->x < 800 && point->y > 0 && point->y < 600) optical_symbol = std::array{point->x, point->y};
  }};
  if(state.locked) {
    auto const &list{state.current.identity.kind == target_kind::mobile ? mobile : ground};
    auto const found{std::find_if(list.begin(), list.end(), [&](auto const &contact){ return equivalent_targets(contact.target, state.current); })};
    if(found == list.end()) { lose_lock(); return; }
    if(state.current.identity.kind == target_kind::mobile) state.current.position = targets.world.position(state.current);
    auto const local{horizontal(difference(state.current.position, view.position), trig.sine(heading), trig.cosine(heading))};
    if(local[1] <= 0 && magnitude(local[1]) >= magnitude(local[0])) { lose_lock(); return; }
    project(state.current.position); return;
  }
  sensor_contact const *chosen{nullptr}; int64_t best{0x100000};
  auto const consider{[&](sensor_contact const &contact){
    auto const local{math::transform(view.rotation, difference(contact.position, view.position))}; auto const score{magnitude(local[0]) + magnitude(local[2])};
    if(local[1] > 0 && score < best) { best = score; chosen = &contact; }
  }};
  for(auto const &contact : ground) consider(contact);
  for(auto const &contact : mobile) consider(contact);
  if(!chosen) { optical.identity.kind = target_kind::none; return; }
  auto const classification{optical.classification}; optical = chosen->target; optical.classification = classification; optical.position = chosen->position;
  if(optical.identity.kind == target_kind::scenery) optical.position[2] = 0;
  project(chosen->position); if(!optical_symbol) optical.identity.kind = target_kind::none;
}
} // namespace apache::simulation
