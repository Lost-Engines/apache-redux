#include "framebuffer.h"
#include <algorithm>
#include <stdexcept>

namespace apache::render {
bool rectangle::contains(int const x, int const y) const {
  return x >= left && y >= top && x <= right && y <= bottom;
}
rectangle intersect(rectangle const a, rectangle const b) {
  return {std::max(a.left, b.left), std::max(a.top, b.top), std::min(a.right, b.right), std::min(a.bottom, b.bottom)};
}
framebuffer::framebuffer(unsigned int const width, unsigned int const height) {
  if(!width || !height || width > 8192 || height > 8192) throw std::runtime_error{"Invalid framebuffer dimensions"};
  image.width = width;
  image.height = height;
  image.pixels.resize(static_cast<size_t>(width) * height);
  reset_clip();
}
void framebuffer::reset_clip() {
  clip = {0, 0, static_cast<int>(image.width) - 1, static_cast<int>(image.height) - 1};
}
void framebuffer::clear(uint8_t const colour) {
  std::ranges::fill(image.pixels, colour);
  std::ranges::fill(coverage, 1);
}
void framebuffer::point(int const x, int const y, uint8_t const colour) {
  if(clip.contains(x, y) && x >= 0 && y >= 0 && x < static_cast<int>(image.width) && y < static_cast<int>(image.height)) { size_t const at{static_cast<size_t>(y) * image.width + static_cast<unsigned int>(x)}; image.pixels[at] = colour; mark(at); }
}
void framebuffer::fill(rectangle area, uint8_t const colour) {
  area = intersect(intersect(area, clip), {0, 0, static_cast<int>(image.width) - 1, static_cast<int>(image.height) - 1});
  if(area.right < area.left) return;
  for(int y{area.top}; y <= area.bottom; ++y) {
    auto const start{image.pixels.begin() + static_cast<ptrdiff_t>(y) * image.width + area.left};
    std::fill_n(start, area.right - area.left + 1, colour);
    if(!coverage.empty()) std::fill_n(coverage.begin() + (start - image.pixels.begin()), area.right - area.left + 1, 1);
  }
}
void framebuffer::surround(rectangle const area, bool const pressed) {
  uint8_t const light{static_cast<uint8_t>(pressed ? 247 : 250)};
  uint8_t const dark{static_cast<uint8_t>(pressed ? 250 : 247)};
  fill({area.left, area.top, area.right, area.top}, light);
  fill({area.left, area.top, area.left, area.bottom}, light);
  fill({area.left, area.bottom, area.right, area.bottom}, dark);
  fill({area.right, area.top, area.right, area.bottom}, dark);
  point(area.left, area.top, static_cast<uint8_t>(pressed ? 246 : 251));
  point(area.left, area.bottom, 248);
  point(area.right, area.top, 248);
  point(area.right, area.bottom, static_cast<uint8_t>(pressed ? 251 : 246));
}
void framebuffer::blit(resources::indexed_image const &source, int const x, int const y, bool const transparent) {
  auto const area{intersect(intersect({x, y, x + static_cast<int>(source.width) - 1, y + static_cast<int>(source.height) - 1}, clip), {0, 0, static_cast<int>(image.width) - 1, static_cast<int>(image.height) - 1})};
  if(area.right < area.left) return;
  for(int yy{area.top}; yy <= area.bottom; ++yy) {
    auto const from{source.pixels.begin() + static_cast<ptrdiff_t>(yy - y) * source.width + area.left - x};
    auto const to{image.pixels.begin() + static_cast<ptrdiff_t>(yy) * image.width + area.left};
    auto const count{area.right - area.left + 1};
    if(transparent) {
      for(int i{0}; i != count; ++i) if(from[i]) { to[i] = from[i]; mark(static_cast<size_t>(to - image.pixels.begin()) + i); }
    } else {
      std::copy_n(from, count, to);
      if(!coverage.empty()) std::fill_n(coverage.begin() + (to - image.pixels.begin()), count, 1);
    }
  }
}
std::vector<rgba> framebuffer::colour_pixels() const {
  std::vector<rgba> pixels;
  pixels.reserve(image.pixels.size());
  for(auto const index : image.pixels) {
    auto const &c{image.colours[index]};
    pixels.push_back({c.r, c.g, c.b, 255});
  }
  return pixels;
}
} // namespace apache::render
