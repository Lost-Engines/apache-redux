#include "player_warnings.h"
#include <algorithm>
#include "weapon_info.h"

namespace apache::simulation {
void player_warnings::begin() {
  previous_count = count;
  count = 0;
  priorities.fill(INT32_MAX);
  for(auto &entry : entries) entry.round.reset();
}
void player_warnings::incoming(size_t const round, unsigned int const weapon, int32_t arrival) {
  ++count;
  arrival = std::max(0, arrival);
  for(size_t i{0}; i != entries.size(); ++i) {
    if(arrival < priorities[i]) {
      // The original moves messages and identities but leaves the other priorities in place.
      for(size_t j{entries.size() - 1}; j > i; --j) entries[j] = entries[j - 1];
      priorities[i] = arrival;
      entries[i] = {round, weapon, "Incoming " + std::to_string(i + 1) + '-' + std::to_string(arrival / 200)};
      break;
    }
  }
}
void player_warnings::present(incoming_warning_events const &events) const {
  size_t const visible{std::min<size_t>(2, count)};
  size_t const previous{std::min<size_t>(2, previous_count)};
  if(count < previous_count) {
    if(events.message) {
      for(size_t i{visible}; i < previous; ++i) events.message(i, false, entries[i].text);
    }
  } else if(count > previous_count) {
    if(events.sound) events.sound();
    static auto const weapons{weapon_definitions()};
    for(size_t i{previous}; i < visible; ++i) {
      if(events.message) events.message(i, true, entries[i].text);
      if(entries[i].round && entries[i].weapon < weapons.size() && events.cockpit_sound) {
        auto const guidance{weapons[entries[i].weapon].guidance};
        if(guidance == 2) {
          events.cockpit_sound(36);
        } else if(guidance == 3) events.cockpit_sound(35);
      }
    }
  }
}
} // namespace apache::simulation
