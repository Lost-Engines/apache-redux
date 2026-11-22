#include "target_search.h"
#include <algorithm>
#include <vector>

namespace apache::simulation {
void contact_bucket::insert(shared_target target, bool const priority) {
  if(count < targets.size()) {
    targets[count] = std::move(target);
  } else if(priority) targets[0] = std::move(target);
  count = static_cast<uint8_t>(count + 1);
}
bool contact_bucket::remove(shared_target const &target) {
  if(!count) return false;
  bool const overflow{count >= targets.size()};
  size_t const last{std::min<size_t>(count, targets.size()) - 1};
  bool found{false};
  for(size_t i{0}; i <= last; ++i) {
    if(targets[i] == target) {
      targets[i] = targets[last];
      targets[last].reset();
      found = true;
      break;
    }
  }
  --count;
  return found && overflow;
}
std::span<shared_target const> contact_bucket::candidates() const {
  return std::span<shared_target const>{targets}.first(std::min<size_t>(count, targets.size()));
}
namespace {
int32_t approximate_distance(math::vector3 const &a, math::vector3 const &b) {
  int32_t x{math::subtract(a[0], b[0])};
  int32_t y{math::subtract(a[1], b[1])};
  if(x < 0) x = math::negate(x);
  if(y < 0) y = math::negate(y);
  return std::bit_cast<int32_t>(static_cast<uint32_t>(std::max(x, y)) * 4u + static_cast<uint32_t>(std::min(x, y))) >> 2;
}
shared_target random_entry(std::span<shared_target const> const targets, std::function<int32_t()> const &random) {
  return targets.empty() ? shared_target{} : targets[static_cast<uint32_t>(random()) % targets.size()];
}
} // anonymous namespace
shared_target choose_sector_target(sector_search_inputs const &sector, math::vector3 const &origin, int32_t const side, search_kind const kind, bool const randomised, target_search_callbacks const &callbacks) {
  if(kind == search_kind::air_defence) {
    auto const first{randomised ? search_kind::mobile_air_defence : search_kind::static_air_defence};
    auto const second{randomised ? search_kind::static_air_defence : search_kind::mobile_air_defence};
    auto target{choose_sector_target(sector, origin, side, first, randomised, callbacks)};
    return target ? target : choose_sector_target(sector, origin, side, second, randomised, callbacks);
  }
  if(randomised && (kind == search_kind::air || kind == search_kind::ground)) {
    auto const target{random_entry(kind == search_kind::air ? sector.air : sector.ground, callbacks.random)};
    return target && kind == search_kind::air && callbacks.position(*target)[2] < 7680 ? shared_target{} : target;
  }
  std::vector<shared_target> statics;
  if(kind == search_kind::static_air_defence) {
    if((side == 1 || side == 2) && sector.scenery_side != side) return {};
    for(auto const &group : {sector.guns, sector.missiles}) {
      for(auto const &target : group) {
        if(!target) break;
        if(callbacks.status(*target) != 1) statics.push_back(target);
      }
    }
  }
  auto const candidates{kind == search_kind::static_air_defence ? std::span<shared_target const>{statics} : kind == search_kind::air ? sector.air : sector.ground};
  std::vector<shared_target> eligible;
  shared_target best;
  int32_t distance{INT32_MAX};
  for(auto const &target : candidates) {
    if(!target) continue;
    if(randomised) {
      if(kind != search_kind::mobile_air_defence || (target->identity.kind == target_kind::mobile && callbacks.mobile_air_defence(*target))) eligible.push_back(target);
    } else {
      auto const position{callbacks.position(*target)};
      if(kind == search_kind::air && position[2] < 7680) continue;
      auto const range{approximate_distance(position, origin)};
      if(range >= distance) continue;
      if(kind == search_kind::mobile_air_defence && (target->identity.kind != target_kind::mobile || !callbacks.mobile_air_defence(*target))) continue;
      distance = range;
      best = target;
    }
  }
  return randomised ? random_entry(eligible, callbacks.random) : best;
}
shared_target search_sector_rings(math::vector3 const &origin, int32_t const side, int32_t const minimum, int32_t const maximum, std::function<shared_target(int32_t, int32_t, int32_t)> const &sector, std::function<int32_t()> const &random) {
  auto const radius{[](int32_t value){return value / 0x400000 + (value % 0x400000 > 0x200000 ? 1 : 0);}};
  int32_t const x{origin[0] >> 22};
  int32_t const y{origin[1] >> 22};
  if(x < 0 || y < 0 || x >= 64 || y >= 64) return {};
  int32_t const first{radius(minimum)};
  int32_t const last{std::min(3, radius(maximum))};
  if(!first) {
    auto target{sector(x, y, side)};
    if(target) return target;
  }
  for(int32_t r{std::max(1, first)}; r <= last; ++r) {
    std::vector<shared_target> candidates;
    for(int32_t i{0}; i <= 2 * r; ++i) {
      std::array<std::array<int32_t, 2>, 4> const cells{{{x - r, y - r + i}, {x - r + i, y + r}, {x + r, y + r - i}, {x + r - i, y - r}}};
      for(auto const &cell : cells) {
        if(cell[0] >= 0 && cell[1] >= 0 && cell[0] < 64 && cell[1] < 64) {
          auto target{sector(cell[0], cell[1], side)};
          if(target) candidates.push_back(std::move(target));
        }
      }
    }
    if(!candidates.empty()) return random_entry(candidates, random);
  }
  return {};
}
} // namespace apache::simulation
