#include "player_targeting.h"
#include <algorithm>
#include <cmath>

namespace apache::simulation {
namespace {
uint32_t magnitude(int32_t value) {
  auto const bits{static_cast<uint32_t>(value)};
  return value < 0 ? 0u - bits : bits;
}
}
int32_t radar_target_priority(target_priority_inputs const &f, math::trigonometry const &trig) {
  if((f.kind != target_kind::scenery && f.kind != target_kind::mobile) || !f.enabled) return -1;
  auto const &d{f.offset};
  int32_t distance{static_cast<int32_t>(std::min<double>(INT32_MAX, std::sqrt(static_cast<double>(d[0]) * d[0] + static_cast<double>(d[1]) * d[1] + static_cast<double>(d[2]) * d[2])))};
  int32_t priority{f.marked ? 65535 : f.model_priority};
  if(f.source == 1) {
    auto const sine{trig.sine(f.heading)};
    auto const cosine{trig.cosine(f.heading)};
    auto const cross{magnitude(math::rounded_q31(math::product_bits(d[0], cosine) - math::product_bits(d[1], sine)))};
    auto const along{magnitude(math::rounded_q31(math::product_bits(d[1], cosine) + math::product_bits(d[0], sine)))};
    int32_t factor{0};
    if(along > cross) {
      if(distance >= 0x4074000) distance = 0x3f2a000;
      auto const alignment{static_cast<int32_t>(((uint64_t{cross} << 31) / along) >> 4)};
      auto const range{static_cast<int32_t>((int64_t{distance} << 31) / 0x4074000)};
      factor = math::subtract(INT32_MAX, math::multiply_q31(alignment, range));
    }
    priority = math::multiply_q31(priority, factor);
  }
  if(distance >= 0x19c8000) distance = 0x187e000;
  return math::subtract(priority, math::multiply_q31(static_cast<int32_t>((int64_t{distance} << 31) / 0x19c8000), 10000));
}
unsigned int target_filter_level(int32_t const priority) {
  switch(priority) {
  case 10000:
    return 1;
  case 30000:
    return 2;
  case 50000:
    return 3;
  default:
    return 0;
  }
}
player_targeting::player_targeting(unsigned int const mode) {
  if(mode == 1 || mode == 4 || mode == 5) {
    ground = false;
    air = true;
    toggle_tactical_range();
  }
}
bool player_targeting::accept(uint16_t const key, unsigned int const mode, targeting_events const &events) const {
  return mode && !(mode == 1 && events.key_event && events.key_event(key));
}
void player_targeting::update(std::span<target_reference const> const ranked, target_reference const &optical, std::function<target_description(target_reference const &)> const &describe) {
  if(ranked.empty() && !tads) {
    current.identity.kind = target_kind::none;
    locked = false;
    name = "NO TARGET";
    return;
  }
  if(automatic) {
    if(!ranked.empty()) {
      current = ranked.front();
    } else {
      current.identity.kind = target_kind::none;
    }
  } else if(!tads) {
    auto const it{std::find_if(ranked.begin(), ranked.end(), [&](auto const &entry){return equivalent_targets(current, entry);})};
    if(it == ranked.end()) {
      current = ranked.front();
      automatic = true;
    } else if(cycle_direction) {
      auto const index{static_cast<size_t>(it - ranked.begin())};
      current = ranked[(index + (cycle_direction == 1 ? 1 : ranked.size() - 1)) % ranked.size()];
      cycle_direction = 0;
    }
  } else if(!locked) {
    if((optical.identity.kind == target_kind::scenery || optical.identity.kind == target_kind::mobile) && (describe(optical).status & 1)) {
      current.identity.kind = target_kind::none;
      laser = false;
      return;
    }
    current = optical;
  }
  if(current.identity.kind == target_kind::scenery || current.identity.kind == target_kind::mobile) {
    auto const description{describe(current)};
    name = "TGT: " + std::string{description.name};
    laser = current.identity.kind == target_kind::scenery && description.designated;
  } else {
    laser = false;
  }
}
void player_targeting::cycle_target(bool const forward, unsigned int const mode) {
  if(mode && !tads) {
    automatic = false;
    cycle_direction = forward ? 1 : 2;
  }
}
void player_targeting::toggle_automatic(unsigned int const mode, uint32_t const faults) {
  if(!mode) return;
  if(!automatic) {
    tads = locked = false;
    cycle_direction = 0;
    automatic = true;
  } else if(!(faults & 0x2000)) {
    tads = true;
    locked = false;
    cycle_direction = 0;
    automatic = false;
  }
}
void player_targeting::toggle_lock(uint16_t const key, unsigned int const mode, uint32_t const faults, targeting_events const &events) {
  if((faults & 0x2000) || !accept(key, mode, events) || current.identity.kind == target_kind::none) return;
  if(!locked) {
    previous_tads = tads;
    tads = true;
    automatic = false;
    locked = true;
  } else {
    locked = false;
    if(!previous_tads) {
      tads = false;
      automatic = true;
    }
  }
}
void player_targeting::cycle_radar(uint16_t const key, unsigned int const mode, targeting_events const &events) {
  if(!accept(key, mode, events)) return;
  if((key & 0x2000) && (ground || air)) {
    ground = air = frozen = false;
    if(events.cockpit_sound) events.cockpit_sound(80);
    return;
  }
  air = ground;
  ground = !ground;
  frozen = false;
  if(events.cockpit_sound) events.cockpit_sound(ground ? 79 : 78);
  if(events.show_radar) events.show_radar();
  if(!locked) {
    automatic = true;
    cycle_direction = 0;
    tads = false;
  }
}
void player_targeting::set_radar(unsigned int const radar, uint32_t const faults, targeting_events const &events) {
  if((faults & 0x200) || radar > 1 || (radar == 0 ? ground : air)) return;
  ground = radar == 0;
  air = !ground;
  frozen = false;
  if(events.cockpit_sound) events.cockpit_sound(ground ? 79 : 78);
}
void player_targeting::toggle_freeze(uint16_t const key, unsigned int const mode, targeting_events const &events) {
  if(!accept(key, mode, events)) return;
  frozen = !frozen;
  if(events.cockpit_sound) events.cockpit_sound(frozen ? 81 : 82);
}
void player_targeting::toggle_ecm(uint16_t const key, unsigned int const mode, targeting_events const &events) {
  if(!accept(key, mode, events)) return;
  automatic_ecm = !automatic_ecm;
  if(events.cockpit_sound) events.cockpit_sound(automatic_ecm ? 108 : 109);
}
void player_targeting::cycle_ground_range(uint16_t const key, unsigned int const mode, targeting_events const &events) {
  if(!accept(key, mode, events)) return;
  ground_range_index = ground_range_index == 0 ? 1 : ground_range_index == 1 ? 2 : 0;
  static std::array<int32_t, 3> constexpr ranges{0x1dab00, 0x3b5600, 0x76ac00};
  static std::array<int32_t, 3> constexpr corners{0x29f4f3, 0x53e9e7, 0xa7d3ce};
  static std::array<int32_t, 3> constexpr marks{0xfd2a, 0x1fa55, 0x3f4aa};
  ground_range = ranges[ground_range_index];
  ground_corner = corners[ground_range_index];
  ground_mark = marks[ground_range_index];
}
void player_targeting::toggle_tactical_range() {
  bool const large{tactical_range == 0x3b5600};
  tactical_range = large ? 0xbde000 : 0x3b5600;
  tactical_corner = large ? 0x10c8617 : 0x53e9e7;
  tactical_mark = large ? 0x65444 : 0x1fa55;
}
} // namespace apache::simulation
