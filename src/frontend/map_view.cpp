#include "map_view.h"
#include <algorithm>
#include <stdexcept>

namespace apache::frontend {
void map_view::restrict() {
  auto &[x, y, scale, angle]{position}; static_cast<void>(angle);
  int32_t const left{x - scale * 320}, right{x + scale * 320};
  int32_t const bottom{y - scale * 240}, top{y + scale * 240};
  if(scale * 640 > 0x8000000) scale = 0x33333;
  if(left < 0x2000000) x += 0x2000000 - left;
  if(right > 0xe000000) x += 0xe000000 - right;
  if(bottom < 0x2000000) y += 0x2000000 - bottom;
  if(top > 0xe000000) y += 0xe000000 - top;
}
void map_view::set(int32_t const x, int32_t const y, int32_t const scale, uint32_t const angle) {
  position = {std::clamp(x, 0, 0xfffffff), std::clamp(y, 0, 0xfffffff), std::clamp(scale, 256, 0x271000), angle}; restrict();
}
void map_view::centre(int32_t const x, int32_t const y) {
  set(x, y, position.zoom, position.rotation);
}
void map_view::zoom(int32_t const scale) { position.zoom = std::clamp(scale, 256, 0x271000); restrict(); }
void map_view::rotate(uint32_t const angle) { position.rotation = angle; restrict(); }
void map_view::push() { restrict(); if(history.size() < 32) history.push_back(position); }
void map_view::pop() {
  if(history.empty()) set(0x7ffffff, 0x7ffffff, 0x271000, 0);
  else { auto const saved{history.back()}; history.pop_back(); set(saved.x, saved.y, saved.zoom, saved.rotation); }
}
void map_view::fit(std::span<render::vertex2 const> const points) {
  int32_t left{0}, right{0xfffffff}, bottom{0}, top{0xfffffff};
  if(!points.empty()) {
    left = right = points.front().x; bottom = top = points.front().y;
    for(auto const &point : points) {
      left = std::min(left, point.x); right = std::max(right, point.x);
      bottom = std::min(bottom, point.y); top = std::max(top, point.y);
    }
  }
  clear();
  set((right + left) / 2, (top + bottom) / 2, std::max((right - left) / 640 * 150 / 100, (top - bottom) / 480 * 150 / 100), 0);
}
render::window2 map_view::window(int const width, int const height) const {
  if(width < 2 || height < 2 || width > 640 || height > 480) throw std::invalid_argument{"Invalid briefing map dimensions"};
  int32_t const w{position.zoom * (width - 1)}, h{position.zoom * (height - 1)};
  int32_t const left{position.x - w / 2}, bottom{position.y - h / 2};
  return {left, bottom, left + w, bottom + h};
}
render::projection2 map_view::projection(math::trigonometry const &trig, int const width, int const height) const {
  render::projection2 view{{0, 0, width - 1, height - 1}, window(width, height)};
  view.sine = trig.sine(position.rotation); view.cosine = trig.cosine(position.rotation);
  return view;
}
} // namespace apache::frontend
