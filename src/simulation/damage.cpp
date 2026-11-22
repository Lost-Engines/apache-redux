#include "damage.h"
#include <algorithm>
#include <cmath>

namespace apache::simulation {
namespace {
int32_t range(math::vector3 const &a, math::vector3 const &b, bool mobile) {
  math::vector3 d;
  for(size_t i{0}; i != 3; ++i) d[i] = math::subtract(a[i], b[i]);
  if(!mobile) {
    uint64_t const sum{static_cast<uint64_t>(int64_t{d[0]} * d[0]) + static_cast<uint64_t>(int64_t{d[1]} * d[1])};
    return std::bit_cast<int32_t>(math::integer_sqrt(sum));
  }
  double const x{static_cast<double>(d[0])};
  double const y{static_cast<double>(d[1])};
  double const z{static_cast<double>(d[2])};
  return std::bit_cast<int32_t>(static_cast<uint32_t>(static_cast<uint64_t>(std::sqrt(x * x + y * y + z * z))));
}
int32_t next(uint32_t &value, uint32_t &global) {
  combat_random r{value, global};
  auto result{r.next()};
  value = r.value;
  global = r.global_seed;
  return result;
}
} // anonymous namespace
damage_result apply_damage(damage_recipient &target, resources::model const &model, damage_source const &source, unsigned int const weapon_id, weapon_info const &weapon, math::vector3 const &impact, blast_limits &blast, damage_random &random, damage_environment const &environment) {
  damage_result result;
  if((target.flags & 1) || (target.mobile && !target.actor)) return result;
  if(source.side == static_cast<int32_t>((target.flags & 7) >> 1) && !(source.kind == target_kind::mobile && (!source.actor || source.network != -1))) return result;
  damage_context context;
  context.range = range(target.position, impact, target.mobile);
  context.armour = model.metadata[2];
  context.radius_override = blast.radius;
  context.damage_override = blast.damage;
  context.alternate_armour = (model.metadata[7] & 2) != 0;
  context.enemy_source = source.side == 2;
  context.difficulty_percent = environment.difficulty_percent;
  context.mode = environment.mode;
  if(int64_t{std::max(weapon.blast_radius, blast.radius)} * 2 >= context.range) {
    if(weapon.weapon_role == 2) context.role_random = next(random.weapon, random.global);
    if(environment.mode == 1 || environment.mode == 4 || environment.mode == 5) context.round_random = next(random.round, random.global);
  }
  result.amount = projectile_damage(weapon_id, weapon, context);
  if(target.mobile && environment.flow != 7 && target.model == 64 && target.network == -1 && source.side == 2) {
    combat_random sample{random.round, random.global};
    result.amount /= 2 + static_cast<int>(decoy_distracts(weapon, environment.mode, true, environment.decoy_modifier, sample));
    random.round = sample.value;
    random.global = sample.global_seed;
  }
  int32_t const previous{target.mobile ? target.damage : static_cast<int32_t>((target.flags & 0x7ff) >> 3)};
  int32_t const total{math::add(previous, result.amount)};
  if(total >= model.metadata[3] && model.metadata[3] != 255) {
    if(blast.damage < model.metadata[3]) {
      blast.damage = model.metadata[3] / 2;
      blast.radius = model.metadata[4];
    }
    result.actions.push_back(damage_action::kill);
    if(target.player) {
      if(source.kind == target_kind::scenery) {
        result.actions.push_back(damage_action::recorded_ground_killer);
      } else if(source.kind == target_kind::mobile && source.actor) result.actions.push_back(damage_action::recorded_actor_killer);
    }
  } else {
    auto const value{static_cast<uint8_t>(std::clamp(total, 0, 255))};
    if(target.mobile) {
      target.damage = value;
    } else {
      target.flags = (target.flags & 0xf807u) | (static_cast<uint32_t>(value) << 3);
    }
    if(target.player) {
      result.actions.push_back(damage_action::player_systems);
      result.actions.push_back(result.amount < 10 ? damage_action::metal_sound : damage_action::explosion_sound);
    }
  }
  return result;
}
void damage_player_item(player_damage_state &state, unsigned int const item, damage_feedbacks &events) {
  struct item_info {
    uint32_t bit;
    std::string_view message;
    unsigned int sound;
  };
  static std::array<item_info, 20> constexpr items{{
    {}, {0x10, "MFD-1 Damaged", 0}, {0x20, "MFD-2 Damaged", 0}, {0x40, "MFD-3 Damaged", 0}, {0x80, "MFD-4 Damaged", 0},
    {0x100, "FLIR Damaged", 0}, {0x200, "Radar Damaged", 0x53}, {}, {0x800, "DTV Damaged", 0x56}, {},
    {0x2000, "TADS Damaged", 0x58}, {}, {0x8000, "Flir Damaged", 0x5a}, {0x10000, "ECM Damaged", 0x5b}, {}, {},
    {0x400000, "Fire-Left Engine", 0x55}, {0x800000, "Fire-Right Engine", 0x55}, {0x1000, "Ihadss Damaged", 0x57}, {0x80000, "Gun Damaged", 0x5e}
  }};
  if(item >= items.size()) return;
  auto const &info{items[item]};
  if(!info.bit || (state.faults & info.bit)) return;
  if(item >= 1 && item <= 4) {
    if(state.broken_mfds >= 3) return;
    ++state.broken_mfds;
  }
  state.faults |= info.bit;
  events.push_back({damage_feedback_kind::message, 400, info.message});
  if(info.sound) events.push_back({damage_feedback_kind::cockpit_sound, info.sound, {}});
  if(item == 6 && (state.ground_radar || state.air_radar)) events.push_back({damage_feedback_kind::radar_toggle, 0, {}});
}
void damage_player_systems(player_damage_state &state, uint8_t const damage, int32_t const resistance, int32_t const now, damage_feedbacks &events) {
  int32_t const difference{static_cast<int32_t>(damage) - state.processed_damage};
  if(state.invulnerable || difference <= 10) return;
  int32_t const serious{std::bit_cast<int32_t>(static_cast<uint32_t>(resistance) * 3u) / 4};
  for(int32_t i{0}; i <= difference / 10; ++i) {
    state.random = state.random * 0x41c64e6du + 0x3039u;
    unsigned int const item{((state.random >> 16) & 0x7fffu) % 20};
    if(damage < serious) {
      if(item < 16) damage_player_item(state, item, events);
    } else {
      damage_player_item(state, item, events);
      if(state.warning_at < now) {
        events.push_back({damage_feedback_kind::cockpit_sound, 0x6e, {}});
        state.warning_at = math::add(now, 60000);
      }
    }
  }
  state.processed_damage = damage;
}
} // namespace apache::simulation
