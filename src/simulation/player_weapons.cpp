#include "player_weapons.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace apache::simulation {
player_weapons::player_weapons(resources::executable_data const &executable) {
  resources::reader data{executable.at(0x446cbc, 27 * 4), "Player weapon sequence"};
  for(auto &row : next_list) for(auto &id : row) { id = data.u32(); if(id >= 9) throw std::runtime_error{"Invalid player weapon sequence"}; }
  selected.fill(-1); pylons[7] = {8, 32}; pylons[8] = {7, 32}; rearm();
}
void player_weapons::rearm() {
  static std::array<int32_t, 9> constexpr capacity{0, 1200, 4, 4, 19, 19, 2, 32, 32};
  ammunition.fill(0); selected.fill(-1);
  for(size_t i{0}; i != pylons.size(); ++i) {
    auto &p{pylons[i]}; if(p.weapon >= 9) throw std::runtime_error{"Invalid player pylon weapon"};
    if(p.weapon) p.ammunition = capacity[p.weapon];
    ammunition[p.weapon] = math::add(ammunition[p.weapon], p.ammunition);
    if(selected[p.weapon] == -1) selected[p.weapon] = static_cast<int32_t>(i);
  }
  current = 1; armed = false;
  if(events.clear_impact_times) events.clear_impact_times();
}
bool player_weapons::accepts(uint16_t const key, player_weapon_context const &context) const {
  return context.control_mode != 0 && !(context.control_mode == 1 && events.key_event && events.key_event(key));
}
void player_weapons::announce() const {
  static std::array<unsigned int, 7> constexpr sounds{0, 30, 27, 27, 28, 28, 29};
  if(armed && current > 0 && current < sounds.size() && events.cockpit_sound) events.cockpit_sound(sounds[current]);
}
bool player_weapons::available(unsigned int const id, int32_t const now) const {
  return id < 9 && selected[id] >= 0 && selected[id] < 9 && now >= deadlines[id] && ammunition[id] > 0;
}
bool player_weapons::next(player_weapon_context const &context) {
  unsigned int radar{0};
  if(context.game_mode != 1 && context.game_mode != 4 && context.game_mode != 5) radar = context.ground_radar ? 1 : context.air_radar ? 2 : 0;
  for(unsigned int i{0}; i != 10; ++i) {
    current = next_list.at(current)[radar];
    if(i == 9) { current = 1; armed = false; if(helmet_mode == 1) helmet_mode = 0; return false; }
    if(ammunition[current] > 0 && selected[current] != -1) {
      armed = true; if(!helmet_mode) helmet_mode = 1; announce(); if(events.clear_impact_times) events.clear_impact_times(); return true;
    }
  }
  return false;
}
bool player_weapons::select(uint16_t const key, player_weapon_context const &context) {
  if(!accepts(key, context)) return false;
  if(!(key & 0x2000)) {
    if(armed) return next(context);
    armed = true; if(!helmet_mode) helmet_mode = 1; announce(); return true;
  }
  if(context.active && armed && events.cockpit_sound) events.cockpit_sound(31);
  armed = false; if(helmet_mode == 1) helmet_mode = 0; return true;
}
bool player_weapons::toggle_countermeasures(uint16_t const key, player_weapon_context const &context) {
  if(!accepts(key, context)) return false;
  automatic_chaff = !automatic_chaff; if(events.cockpit_sound) events.cockpit_sound(automatic_chaff ? 77 : 76); return true;
}
bool player_weapons::fire(uint16_t const key, player_weapon_context const &context) {
  if(context.dead) return false;
  if(context.landed) { armed = false; if(helmet_mode == 1) helmet_mode = 0; return false; }
  if(!accepts(key, context)) return false;
  if(!armed) { armed = true; if(!helmet_mode) helmet_mode = 1; return false; }
  if(!available(current, context.now) || current < 1 || current > 6) return false;
  size_t const slot{static_cast<size_t>(selected[current])};
  auto consume{[&]{ if(!infinite) { --pylons[slot].ammunition; --ammunition[current]; } }};
  auto make{[&](math::pose const &pose){ return events.launch ? events.launch({current, pose, current == 2 || current == 3 || current == 6, current == 2, context.target_kind == 0}) : std::optional<size_t>{}; }};
  std::optional<size_t> last;
  if(current == 1) {
    if(context.faults & 0x80000) return false;
    auto pose{events.launch_pose ? events.launch_pose(current, slot) : math::pose{}};
    int32_t count{4};
    if(context.game_mode == 0) {
      double const requested{context.frame_time * 200.0 / definitions[1].fire_interval_ticks};
      if(!std::isfinite(requested) || requested < INT32_MIN || requested > INT32_MAX) throw std::runtime_error{"Invalid player firing interval"};
      count = static_cast<int32_t>(requested); if(!count) count = 1;
    }
    math::vector3 increment;
    for(size_t i{0}; i != 3; ++i) increment[i] = math::multiply_q31(pose.rotation[3 + i], definitions[1].launch_speed / definitions[1].fire_interval_ticks);
    gun_fired = true; count = std::clamp(count, 0, ammunition[current]);
    while(count-- > 0 && ammunition[current] > 0) {
      for(size_t i{0}; i != 3; ++i) pose.position[i] = math::add(pose.position[i], increment[i]);
      last = make(pose); if(last) consume();
    }
  } else {
    if(events.launch_sound) events.launch_sound(current == 2 ? 1 : current == 3 || current == 6 ? 2 : 3);
    last = make(events.launch_pose ? events.launch_pose(current, slot) : math::pose{});
    if(last) {
      if(events.missile_launched) events.missile_launched(*last);
      if(current == 2 || current == 3 || current == 6) { if(events.impact_time) events.impact_time(*last); }
      if(current == 3 || current == 6) { if(events.cycle_target) events.cycle_target(); }
      consume();
    }
  }
  if(last && events.remote_fire) events.remote_fire(*last);
  if(context.target_kind == 2 && events.fired_at) events.fired_at();
  deadlines[current] = math::add(context.now, definitions[current].fire_interval_ticks);
  if(!infinite && (current == 2 || current == 3) && events.remove_missile) events.remove_missile(slot, pylons[slot].ammunition);
  static std::array<int32_t, 9> constexpr alternate{1, 0, 3, 2, 5, 4, 6, 7, 8};
  selected[current] = alternate[slot];
  if(pylons[static_cast<size_t>(selected[current])].ammunition < 1) {
    std::array<bool, 9> seen{};
    for(size_t i{0}; i != 9; ++i) { auto const &p{pylons[i]}; if(!seen.at(p.weapon) && p.ammunition > 0) { seen[p.weapon] = true; selected[p.weapon] = static_cast<int32_t>(i); } }
  }
  if(ammunition[current] < 1) { next(context); deadlines[current] = math::add(context.now, 100); }
  return true;
}
} // namespace apache::simulation
