#include "casualties.h"

namespace apache::multiplayer {
bool casualties::mobile(int32_t const task, int32_t const serial, uint8_t const source) {
  if(mobile_count == 63) return false;
  size_t const offset{static_cast<size_t>(mobile_count++) * 4};
  mobile_records[offset] = static_cast<uint8_t>(serial);
  mobile_records[offset + 1] = static_cast<uint8_t>(static_cast<uint32_t>(serial) >> 8);
  mobile_records[offset + 2] = static_cast<uint8_t>(task);
  mobile_records[offset + 3] = source;
  return true;
}
bool casualties::scenery(int32_t const x, int32_t const y, uint8_t const source) {
  if(scenery_count == 28) return false;
  size_t const offset{static_cast<size_t>(scenery_count++) * 12};
  for(size_t i{0}; i < 4; ++i) {
    scenery_records[offset + i] = static_cast<uint8_t>(static_cast<uint32_t>(x) >> (i * 8));
    scenery_records[offset + 4 + i] = static_cast<uint8_t>(static_cast<uint32_t>(y) >> (i * 8));
  }
  scenery_records[offset + 8] = source;
  return true;
}
std::optional<packet> casualties::take_mobile(uint32_t const sender, std::string_view const name) {
  if(!mobile_count) return {};
  auto result{kills(sender, name, false, mobile_count, mobile_records)};
  mobile_count = 0;
  mobile_records.fill(0);
  return result;
}
std::optional<packet> casualties::take_scenery(uint32_t const sender, std::string_view const name) {
  if(!scenery_count) return {};
  auto result{kills(sender, name, true, scenery_count, scenery_records)};
  scenery_count = 0;
  scenery_records.fill(0);
  return result;
}
} // namespace apache::multiplayer
