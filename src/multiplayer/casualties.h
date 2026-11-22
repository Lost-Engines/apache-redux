#pragma once
#include "packet.h"

namespace apache::multiplayer {
// Per-frame lists used by cooperative and pilot/gunner sessions.
class casualties {
  std::array<uint8_t, 252> mobile_records{};
  std::array<uint8_t, 336> scenery_records{};
  uint8_t mobile_count{0};
  uint8_t scenery_count{0};
public:
  bool mobile(int32_t task, int32_t serial, uint8_t source);
  bool scenery(int32_t x, int32_t y, uint8_t source);
  std::optional<packet> take_mobile(uint32_t sender, std::string_view name);
  std::optional<packet> take_scenery(uint32_t sender, std::string_view name);
};
} // namespace apache::multiplayer
