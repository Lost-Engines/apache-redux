#include "display_layout.h"
#include <algorithm>
#include <cmath>
#include <numbers>
namespace apache::render {
display_layout::display_layout(unsigned int width, unsigned int height, unsigned int logical_width,
                               unsigned int logical_height, rectangle viewport, double fov)
  : source_width{logical_width}, source_height{logical_height} {
  double const scale{std::min(double(width) / logical_width, double(height) / logical_height)};
  int const w{static_cast<int>(std::lround(logical_width * scale))}, h{static_cast<int>(std::lround(logical_height * scale))};
  int const x{(static_cast<int>(width) - w) / 2}, y{(static_cast<int>(height) - h) / 2};
  artwork = {x, y, x + w - 1, y + h - 1};
  scene = {0, y + static_cast<int>(std::lround(viewport.top * scale)), static_cast<int>(width) - 1,
           y + static_cast<int>(std::lround((viewport.bottom + 1) * scale)) - 1};
  if(viewport.top == 0 && viewport.bottom + 1 == static_cast<int>(logical_height)) { scene.top = 0; scene.bottom = static_cast<int>(height) - 1; }
  instrument_fov = static_cast<unsigned int>(w) == width ? fov : 360 / std::numbers::pi * std::atan(double(w) / width * std::tan(fov * std::numbers::pi / 360));
}
void display_layout::background(framebuffer const &world, framebuffer &overlay) const {
  auto const w{artwork.right - artwork.left + 1}, h{artwork.bottom - artwork.top + 1};
  for(unsigned int y{0}; y != source_height; ++y) for(unsigned int x{0}; x != source_width; ++x) {
    auto const sx{artwork.left + static_cast<int>(uint64_t{x} * w / source_width)};
    auto const sy{artwork.top + static_cast<int>(uint64_t{y} * h / source_height)};
    overlay.image.pixels[size_t{y} * source_width + x] = world.image.pixels[size_t(sy) * world.image.width + sx];
  }
  overlay.coverage.assign(overlay.image.pixels.size(), 0);
  overlay.reset_clip();
}
void display_layout::composite(framebuffer const &overlay, framebuffer &world) const {
  int const w{artwork.right - artwork.left + 1}, h{artwork.bottom - artwork.top + 1};
  for(int y{0}; y != h; ++y) {
    size_t const row{size_t(uint64_t(y) * source_height / h) * source_width};
    auto *out{world.image.pixels.data() + size_t(y + artwork.top) * world.image.width + artwork.left};
    for(int x{0}; x != w; ++x) {
      size_t const at{row + size_t(uint64_t(x) * source_width / w)};
      if(overlay.coverage[at]) out[x] = overlay.image.pixels[at];
    }
  }
}
} // namespace apache::render
