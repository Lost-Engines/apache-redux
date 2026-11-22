#pragma once
#include <functional>
#include <span>
#include <string>
#include "targets.h"

namespace apache::simulation {
struct target_priority_inputs {
  target_kind kind{target_kind::none};
  unsigned int source{0};
  int32_t model_priority{0};
  bool enabled{false};
  bool marked{false};
  math::vector3 offset{};
  uint32_t heading{0};
};
int32_t radar_target_priority(target_priority_inputs const &input, math::trigonometry const &trig);
unsigned int target_filter_level(int32_t priority);
struct targeting_events {
  std::function<bool(uint16_t)> key_event;
  std::function<void(unsigned int)> cockpit_sound;
  std::function<void()> show_radar;
};
class player_targeting {
  bool accept(uint16_t key, unsigned int mode, targeting_events const &events) const;
public:
  target_reference current;
  bool tads{true};
  bool automatic{false};
  bool locked{false};
  bool previous_tads{false};
  bool laser{false};
  unsigned int cycle_direction{0};
  bool ground{true};
  bool air{false};
  bool tactical{true};
  bool frozen{false};
  bool automatic_ecm{true};
  bool scan_all{false};
  unsigned int threat{4};
  unsigned int ground_range_index{2};
  int32_t ground_range{0x76ac00};
  int32_t ground_corner{0xa7d3ce};
  int32_t ground_mark{0x3f4aa};
  int32_t air_range{0x76ac00};
  int32_t tactical_range{0x3b5600};
  int32_t tactical_corner{0x53e9e7};
  int32_t tactical_mark{0x1fa55};
  std::string name{"NO TARGET"};
  explicit player_targeting(unsigned int mode = 0);
  void update(std::span<target_reference const> ranked, target_reference const &optical, std::function<target_description(target_reference const &)> const &describe);
  void cycle_target(bool forward, unsigned int mode);
  void toggle_automatic(unsigned int mode, uint32_t faults);
  void toggle_lock(uint16_t key, unsigned int mode, uint32_t faults, targeting_events const &events = {});
  void cycle_radar(uint16_t key, unsigned int mode, targeting_events const &events = {});
  void set_radar(unsigned int radar, uint32_t faults, targeting_events const &events = {});
  void toggle_freeze(uint16_t key, unsigned int mode, targeting_events const &events = {});
  void toggle_ecm(uint16_t key, unsigned int mode, targeting_events const &events = {});
  void cycle_ground_range(uint16_t key, unsigned int mode, targeting_events const &events = {});
  void toggle_tactical_range();
};
} // namespace apache::simulation
