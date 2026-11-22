#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace apache::render {
enum class draw_kind { terrain, ground_object, mobile, shadow, crater, explosion, smoke, fireball, cloud, fragment };
struct queued_draw {
  draw_kind kind{draw_kind::terrain};
  uint32_t payload{0};
  int32_t depth{0};
  int32_t terrain_depth_sum{0};
  bool flat{false};
};
class draw_queue {
  struct item {
    queued_draw draw;
    int next{-1};
  };
  std::array<int, 768> bins;
  std::vector<item> items;
  int flat_head{-1};
  int behind_head{-1};
public:
  draw_queue();
  void clear();
  bool push(queued_draw const &draw);
  bool consume_unused_slot();
  size_t available() const {
    return 7200 - items.size();
  }
  bool full() const {
    return items.size() == 7200;
  }
  std::vector<queued_draw> ordered() const;
};
} // namespace apache::render
