#pragma once
#include "resources/missions.h"

namespace apache::simulation {
enum class mission_result : uint32_t {
  untried,
  aborted,
  failure,
  success,
  outstanding
};
struct mission_history {
  uint32_t mission{0};
  mission_result result{mission_result::untried};
};
struct campaign_progress {
  uint32_t warzone{0};
  uint32_t level{0};
  uint32_t lives{0};
  uint32_t seed{0};
  std::vector<mission_history> missions;
};
struct pilot_log {
  std::string name;
  uint32_t rank{0};
  uint32_t status{2};
  uint32_t total_time{0};
  uint32_t combat_time{0};
  uint32_t status_hour{0};
  uint32_t protected_profile{0};
  std::array<uint32_t, 3> medals{};
  std::vector<mission_history> missions;
  campaign_progress campaign;
  std::array<bool, 3> victories{};
  // Recomputed by update(), never stored in the original file.
  uint32_t completed{0};
  uint32_t successes{0};
  uint32_t failures{0};
  uint32_t outstanding{0};
  uint32_t campaigns_won{0};
  uint32_t awards{0};
  static pilot_log decode(std::span<uint8_t const> bytes);
  resources::bytes encode() const;
  void update(std::span<resources::mission_briefing const> definitions);
  void recover(uint32_t now_hours);
  void set_status(uint32_t status, uint32_t now_hours);
  void log_result(uint32_t mission, mission_result result, bool in_campaign);
  // Positive mission number, -1 no campaign, -2 defeat, -3 victory.
  int32_t next_mission(std::span<resources::mission_briefing const> definitions) const;
};
} // namespace apache::simulation
