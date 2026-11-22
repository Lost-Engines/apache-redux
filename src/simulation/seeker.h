#pragma once
#include <functional>
#include "math/geometry.h"
#include "weapon_rules.h"

namespace apache::simulation {
enum class seeker_target : uint8_t {
  none,
  selected,
  chaff,
  flare
};
struct seeker_state {
  uint32_t flags{1};
  int32_t retry_time{0};
  seeker_target adjusted{seeker_target::none};
};
struct seeker_inputs {
  int32_t now{0};
  int32_t weather{0};
  int32_t cloud_bottom{0};
  int32_t cloud_top{0};
  int32_t target_status{0};
  int32_t target_classification{0};
  int32_t source_side{0};
  bool adjusted_valid{true};
  bool has_designator{true};
  bool ground_designated{false};
  bool designator_active{false};
  bool designator_destroyed{false};
  bool chaff{false};
  bool flare{false};
  bool source_radar{false};
  math::vector3 missile_position{};
  math::vector3 target_position{};
  math::vector3 source_position{};
  math::vector3 designator_position{};
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
