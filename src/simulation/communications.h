#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace apache::simulation {
struct communication_message {
  std::string text;
  int32_t lifetime{0};
  int32_t priority{0};
  bool displayed{false};
  bool changing{false};
  uint32_t identity{0};
};
struct communication_status {
  int32_t fuel{0};
  int32_t endurance{0};
  int32_t flow{0};
  int32_t score{0};
  bool infinite_fuel{false};
  uint32_t faults{0};
  std::string_view weapon;
};
// Only messages which have appeared on the panel consume their display time.
class communications {
public:
  std::array<communication_message, 16> messages;
  std::array<std::string, 3> lines;
  uint32_t damaged_seed{0};
  bool add(std::string_view text, int32_t priority, int32_t lifetime);
  bool add_changing(uint32_t identity, std::string_view text, int32_t priority);
  void update_changing(uint32_t identity, std::string_view text);
  void remove(uint32_t identity);
  void advance(double seconds);
  std::array<std::string, 3> const &compose(communication_status const &status);
};
} // namespace apache::simulation
