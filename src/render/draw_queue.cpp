#include "draw_queue.h"

namespace apache::render {
draw_queue::draw_queue() {
  items.reserve(7200);
  clear();
}
void draw_queue::clear() {
  bins.fill(-1);
  items.clear();
  flat_head = behind_head = -1;
}
bool draw_queue::consume_unused_slot() {
  if(full()) return false;
  items.emplace_back();
  return true;
}
bool draw_queue::push(queued_draw const &draw) {
  if(items.size() == 7200) return false;
  int const index{static_cast<int>(items.size())};
  items.push_back({draw});
  if(draw.kind == draw_kind::ground_object && draw.flat) {
    items[index].next = flat_head;
    flat_head = index;
    return true;
  }
  if(draw.depth < 0) {
    items[index].next = behind_head;
    behind_head = index;
    return true;
  }
  int const bin{draw.depth > 0x3ffffff ? 767 : draw.depth > 0xffffff ? (draw.depth >> 18) + 512 : draw.depth > 0xfffff ? (draw.depth >> 16) + 256 : draw.depth >> 12};
  int *link{&bins[bin]};
  while(*link != -1) {
    auto const &existing{items[*link].draw};
    if(existing.depth < draw.depth || (existing.depth == draw.depth && existing.kind == draw_kind::terrain && draw.kind == draw_kind::terrain && existing.terrain_depth_sum < draw.terrain_depth_sum)) break;
    link = &items[*link].next;
  }
  items[index].next = *link;
  *link = index;
  return true;
}
std::vector<queued_draw> draw_queue::ordered() const {
  std::vector<queued_draw> result;
  result.reserve(items.size());
  auto const append{[&](int index){
    while(index != -1) {
      result.push_back(items[index].draw);
      index = items[index].next;
    }
  }};
  append(flat_head);
  for(auto i{bins.rbegin()}; i != bins.rend(); ++i) append(*i);
  append(behind_head);
  return result;
}
} // namespace apache::render
