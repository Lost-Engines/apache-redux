#pragma once
#include "image.h"

namespace apache::resources {
// Digital Integration ANIM packets, shared by the .ANM intro and .Q3 campaigns.
class animation {
  struct packet { uint8_t flags; size_t offset, palette_size, size; };
  bytes source;
  std::vector<packet> packets;
  size_t next_frame{0};
  void palette_frame(std::span<uint8_t const> data, bool runs);
  void blocks(std::span<uint8_t const> data);
  void delta(std::span<uint8_t const> data);
public:
  indexed_image image;
  explicit animation(bytes data);
  size_t count() const {
    return packets.size();
  }
  size_t position() const {
    return next_frame;
  }
  bool advance();
};
} // namespace apache::resources
