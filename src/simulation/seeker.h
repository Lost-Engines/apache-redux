#pragma once
#include <functional>
#include "math/geometry.h"
#include "weapon_rules.h"

namespace apache::simulation {
enum class seeker_target : uint8_t { none, selected, chaff, flare };
struct seeker_state {
  uint32_t flags{1};
  int32_t retry_time{0};
  seeker_target adjusted{seeker_target::none};
};
struct seeker_inputs {
  int32_t now{0}, weather{0}, cloud_bottom{0}, cloud_top{0};
  int32_t target_status{0}, target_classification{0}, source_side{0};
  bool adjusted_valid{true}, has_designator{true}, ground_designated{false}, designator_active{false}, designator_destroyed{false};
  bool chaff{false}, flare{false}, source_radar{false};
  math::vector3 missile_position{}, target_position{}, source_position{}, designator_position{};
};
struct seeker_queries {
  std::function<bool(math::vector3 const &, math::vector3 const &)> visible;
  std::function<bool()> distracted;
  std::function<int32_t()> random;
  std::function<int32_t()> classification;
};
bool decoy_distracts(weapon_info const &weapon, int32_t mode, bool enemy, int32_t modifier, combat_random &random);
// Returns whether the adjusted target binding was replaced.
bool update_seeker(seeker_state &state, weapon_info const &weapon, seeker_inputs const &input, seeker_queries const &queries);
} // namespace apache::simulation
