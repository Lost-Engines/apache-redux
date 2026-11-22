#pragma once
#include <array>
#include <functional>
#include <memory>
#include <optional>
#include "resources/reader.h"

namespace apache::audio {
struct stream_source {
  std::shared_ptr<resources::bytes const> pcm;
  std::string name;
  unsigned int channels{2};
  bool repeat{false};
  size_t loop{0};
};
// Original 22000 Hz stream timing and block-reader policy, without a platform thread.
class stream {
  static size_t constexpr capacity{1 << 20};
  static size_t constexpr block{2048};
  resources::bytes ring = resources::bytes(capacity, 128);
  struct saved_stream {
    stream_source source;
    size_t position{0};
  };
  std::optional<saved_stream> saved;
  void preread();
  void read_block();
  void update_space();
  void reader();
  void begin(stream_source source, size_t offset);
public:
  stream_source source;
  uint64_t clock{0};
  uint64_t due{0};
  size_t head{0};
  size_t tail{0};
  size_t position{0};
  size_t space{capacity};
  size_t file_position{0};
  bool open{false};
  bool waiting{false};
  bool playing{false};
  unsigned int volume{63};
  std::function<void(stream_source const &, uint64_t)> started;
  using pcm_sink = std::function<void(std::span<uint8_t const>, unsigned int)>;
  void start(stream_source source);
  void store();
  void stop();
  void restore();
  void forget() {
    saved.reset();
  }
  bool stored() const {
    return saved.has_value();
  }
  size_t stored_position() const {
    return saved ? saved->position : 0;
  }
  void advance(size_t frames, pcm_sink const &sink = {});
  std::array<float, 2> sample(size_t offset = 0) const;
};
} // namespace apache::audio
