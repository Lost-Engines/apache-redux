#include "vector_hud.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include "stroke.h"

namespace apache::render {
vector_hud::vector_hud(framebuffer &destination, hud_mapping const transform, uint8_t const colour)
  : target{destination},
    mapping{transform} {
  target.hud_coverage.clear();
  target.hud_colour = colour;
}
void vector_hud::line(std::array<double, 2> const &from, std::array<double, 2> const &to, rectangle const logical_clip, uint32_t const pattern) {
  double const dx{to[0] - from[0]};
  double const dy{to[1] - from[1]};
  double first{0};
  double last{1};
  auto const edge{[&](double a, double b){
    if(a < 0 && b < 0) return false;
    if(a < 0) first = std::max(first, a / (a - b));
    if(b < 0) last = std::min(last, a / (a - b));
    return first <= last;
  }};
  if(!edge(from[0] - logical_clip.left, to[0] - logical_clip.left)
      || !edge(logical_clip.right - from[0], logical_clip.right - to[0])
      || !edge(from[1] - logical_clip.top, to[1] - logical_clip.top)
      || !edge(logical_clip.bottom - from[1], logical_clip.bottom - to[1])) return;
  std::array<double, 2> const a{from[0] + dx * first, from[1] + dy * first};
  std::array<double, 2> const b{from[0] + dx * last, from[1] + dy * last};
  auto clip{intersect(mapping.clip, {
    static_cast<int>(std::ceil(mapping.offset_x + logical_clip.left * mapping.scale_x)),
    static_cast<int>(std::ceil(mapping.offset_y + logical_clip.top * mapping.scale_y)),
    static_cast<int>(std::ceil(mapping.offset_x + (logical_clip.right + 1) * mapping.scale_x)) - 1,
    static_cast<int>(std::ceil(mapping.offset_y + (logical_clip.bottom + 1) * mapping.scale_y)) - 1,
  })};
  clip = intersect(clip, {0, 0, static_cast<int>(target.image.width) - 1, static_cast<int>(target.image.height) - 1});
  if(clip.right < clip.left || clip.bottom < clip.top) return;
  if(target.hud_coverage.empty()) target.hud_coverage.assign(target.image.pixels.size(), 0);
  auto const stroke{[&](double begin, double end){
    auto const point{[&](double at){
      return std::array{
        mapping.offset_x + (a[0] + (b[0] - a[0]) * at + 0.5) * mapping.scale_x,
        mapping.offset_y + (a[1] + (b[1] - a[1]) * at + 0.5) * mapping.scale_y,
      };
    }};
    paint_stroke(point(begin), point(end), clip, mapping.stroke_width, true,
      [&](int x, int y, uint8_t coverage){
        auto &ink{target.hud_coverage[size_t(y) * target.image.width + x]};
        ink = std::max(ink, coverage);
      });
  }};
  if(pattern == UINT32_MAX) {
    stroke(0, 1);
    return;
  }
  // Keep the original 32-bit dash masks in logical pixels, independent of resolution.
  double const length{std::max(std::abs(b[0] - a[0]), std::abs(b[1] - a[1]))};
  uint32_t mask{pattern};
  double begin{-1};
  for(int i{0}; i <= static_cast<int>(std::ceil(length)); ++i) {
    mask = std::rotl(mask, 1);
    if(mask & 1) {
      if(begin < 0) begin = std::max(0.0, i - 0.5);
    } else if(begin >= 0) {
      stroke(length ? begin / length : 0, length ? std::min(length, i - 0.5) / length : 1);
      begin = -1;
    }
  }
  if(begin >= 0) stroke(length ? begin / length : 0, 1);
}
} // namespace apache::render
