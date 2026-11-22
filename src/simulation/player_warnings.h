#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace apache::simulation {
struct incoming_warning {
  std::optional<size_t> round;
  unsigned int weapon{0};
  std::string text;
};
struct incoming_warning_events {
  std::function<void()> sound;
  std::function<void(unsigned int)> cockpit_sound;
  std::function<void(size_t, bool, std::string_view)> message;
};
class player_warnings {
public:
  size_t count{0};
  size_t previous_count{0};
  std::array<int32_t, 3> priorities{INT32_MAX, INT32_MAX, INT32_MAX};
  std::array<incoming_warning, 3> entries;
  void begin();
  void incoming(size_t round, unsigned int weapon, int32_t arrival);
  void present(incoming_warning_events const &events) const;
};
} // namespace apache::simulation
