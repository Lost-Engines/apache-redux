#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include "framebuffer.h"

namespace apache::render {
// Scan only the strip covered by a round-ended stroke, including at steep banks.
// Coordinates describe pixel centres; the callback receives linear edge coverage.
template<class Paint>
void paint_stroke(std::array<double, 2> const &a, std::array<double, 2> const &b, rectangle const clip,
                  double const width, bool const antialias, Paint const &paint) {
  if(clip.right < clip.left || clip.bottom < clip.top) return;
  double const dx{b[0] - a[0]};
  double const dy{b[1] - a[1]};
  double const length2{dx * dx + dy * dy};
  double const radius{std::max(1.0, width) / 2};
  double const reach{radius + (antialias ? 0.5 : 0.0)};
  double const inner{std::max(0.0, radius - 0.5)};
  int const top{static_cast<int>(std::clamp(std::floor(std::min(a[1], b[1]) - reach), double(clip.top), double(clip.bottom + 1)))};
  int const bottom{static_cast<int>(std::clamp(std::ceil(std::max(a[1], b[1]) + reach), double(clip.top - 1), double(clip.bottom)))};
  for(int y{top}; y <= bottom; ++y) {
    double first{0};
    double last{1};
    if(dy != 0) {
      double const t0{std::clamp((y + 0.5 - reach - a[1]) / dy, 0.0, 1.0)};
      double const t1{std::clamp((y + 0.5 + reach - a[1]) / dy, 0.0, 1.0)};
      first = std::min(t0, t1);
      last = std::max(t0, t1);
    }
    double const x0{a[0] + dx * first};
    double const x1{a[0] + dx * last};
    int const left{static_cast<int>(std::clamp(std::floor(std::min(x0, x1) - reach), double(clip.left), double(clip.right + 1)))};
    int const right{static_cast<int>(std::clamp(std::ceil(std::max(x0, x1) + reach), double(clip.left - 1), double(clip.right)))};
    for(int x{left}; x <= right; ++x) {
      double const px{x + 0.5 - a[0]};
      double const py{y + 0.5 - a[1]};
      double const t{length2 ? std::clamp((px * dx + py * dy) / length2, 0.0, 1.0) : 0.0};
      double const ex{px - dx * t};
      double const ey{py - dy * t};
      double const distance2{ex * ex + ey * ey};
      if(distance2 >= reach * reach) continue;
      auto const coverage{antialias && distance2 > inner * inner ? static_cast<uint8_t>(std::lround(std::clamp(radius + 0.5 - std::sqrt(distance2), 0.0, 1.0) * 255)) : uint8_t{255}};
      if(coverage) paint(x, y, coverage);
    }
  }
}
} // namespace apache::render
