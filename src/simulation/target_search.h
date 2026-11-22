#pragma once
#include <functional>
#include <memory>
#include <span>
#include "target_visibility.h"

namespace apache::simulation {
using shared_target = std::shared_ptr<target_reference>;
struct contact_bucket {
  uint8_t count{0};
  std::array<shared_target, 8> targets{};
  void insert(shared_target target, bool priority = false);
  bool remove(shared_target const &target);
  std::span<shared_target const> candidates() const;
};
enum class search_kind {
  air,
  ground,
  mobile_air_defence,
  static_air_defence,
  air_defence
};
struct sector_search_inputs {
  std::span<shared_target const> air;
  std::span<shared_target const> ground;
  int32_t scenery_side{0};
  std::array<shared_target, 2> guns{};
  std::array<shared_target, 2> missiles{};
};
struct target_search_callbacks {
  std::function<math::vector3(target_reference const &)> position;
  std::function<int32_t(target_reference const &)> status;
  std::function<bool(target_reference const &)> mobile_air_defence;
  std::function<int32_t()> random;
};
shared_target choose_sector_target(sector_search_inputs const &sector, math::vector3 const &origin, int32_t side, search_kind kind, bool randomised, target_search_callbacks const &callbacks);
shared_target search_sector_rings(math::vector3 const &origin, int32_t side, int32_t minimum, int32_t maximum, std::function<shared_target(int32_t, int32_t, int32_t)> const &sector, std::function<int32_t()> const &random);
} // namespace apache::simulation
