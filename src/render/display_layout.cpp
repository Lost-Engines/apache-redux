#include "display_layout.h"
#include <algorithm>
#include <cmath>
#include <numbers>
namespace apache::render {
display_layout::display_layout(unsigned int width, unsigned int height, unsigned int logical_width,
                               unsigned int logical_height, rectangle viewport, double fov, horizontal_anchor anchor)
  : source_width{logical_width}, source_height{logical_height} {
  double const scale{std::min(double(width) / logical_width, double(height) / logical_height)};
  int const w{static_cast<int>(std::lround(logical_width * scale))}, h{static_cast<int>(std::lround(logical_height * scale))};
  int const spare{static_cast<int>(width) - w};
  int const x{anchor == horizontal_anchor::left ? 0 : anchor == horizontal_anchor::right ? spare : spare / 2};
  int const y{(static_cast<int>(height) - h) / 2};
  artwork = {x, y, x + w - 1, y + h - 1};
  scene = {0, y + static_cast<int>(std::lround(viewport.top * scale)), static_cast<int>(width) - 1,
           y + static_cast<int>(std::lround((viewport.bottom + 1) * scale)) - 1};
  if(viewport.top == 0 && viewport.bottom + 1 == static_cast<int>(logical_height)) { scene.top = 0; scene.bottom = static_cast<int>(height) - 1; }
  instrument_fov = static_cast<unsigned int>(w) == width ? fov : 360 / std::numbers::pi * std::atan(double(w) / width * std::tan(fov * std::numbers::pi / 360));
}
rectangle display_layout::map(rectangle const logical) const {
  int const w{artwork.right - artwork.left + 1}, h{artwork.bottom - artwork.top + 1};
  // Composite samples with floor(destination * source / size). Ceil at both
  // boundaries therefore covers exactly the pixels belonging to this rectangle.
  auto const edge{[](int value, int size, unsigned int source){ return static_cast<int>((int64_t{value} * size + source - 1) / source); }};
  return {artwork.left + edge(logical.left, w, source_width), artwork.top + edge(logical.top, h, source_height),
    artwork.left + edge(logical.right + 1, w, source_width) - 1, artwork.top + edge(logical.bottom + 1, h, source_height) - 1};
}
void display_layout::background(framebuffer const &world, framebuffer &overlay) const {
  auto const w{artwork.right - artwork.left + 1}, h{artwork.bottom - artwork.top + 1};
  std::vector<unsigned int> columns(source_width);
  for(unsigned int x{0}; x != source_width; ++x)
    columns[x] = static_cast<unsigned int>(artwork.left) + static_cast<unsigned int>(uint64_t{x} * w / source_width);
  for(unsigned int y{0}; y != source_height; ++y) {
    auto const sy{artwork.top + static_cast<int>(uint64_t{y} * h / source_height)};
    auto const *row{world.image.pixels.data() + size_t(sy) * world.image.width};
    auto *out{overlay.image.pixels.data() + size_t{y} * source_width};
    for(unsigned int x{0}; x != source_width; ++x) out[x] = row[columns[x]];
  }
  overlay.coverage.assign(overlay.image.pixels.size(), 0);
  overlay.reset_clip();
}
void display_layout::composite(framebuffer const &overlay, framebuffer &world) const {
  int const w{artwork.right - artwork.left + 1}, h{artwork.bottom - artwork.top + 1};
  // visit each logical pixel once, expanding covered runs into the native image
  // ceil boundaries preserve floor(destination * source / size) sampling exactly
  auto const edge{[](unsigned int value, int size, unsigned int source){
    return static_cast<int>((uint64_t{value} * size + source - 1) / source);
  }};
  std::vector<int> columns(source_width + 1);
  for(unsigned int x{0}; x <= source_width; ++x) columns[x] = edge(x, w, source_width);
  for(unsigned int y{0}; y != source_height; ++y) {
    int const top{edge(y, h, source_height)}, bottom{edge(y + 1, h, source_height)};
    if(top == bottom) continue;
    auto const *mask{overlay.coverage.data() + size_t{y} * source_width};
    auto const *source{overlay.image.pixels.data() + size_t{y} * source_width};
    auto *out{world.image.pixels.data() + size_t(top + artwork.top) * world.image.width + artwork.left};
    unsigned int x{0};
    while(x != source_width) {
      while(x != source_width && !mask[x]) ++x;
      int const start{columns[x]};
      while(x != source_width && mask[x]) {
        std::fill(out + columns[x], out + columns[x + 1], source[x]);
        ++x;
      }
      int const count{columns[x] - start};
      if(count) for(int yy{top + 1}; yy != bottom; ++yy)
        std::copy_n(out + start, count, out + size_t(yy - top) * world.image.width + start);
    }
  }
}
void display_layout::composite_hud(framebuffer const &overlay, framebuffer &world, rectangle const reference_view, double const fov) const {
  double const zoom{fov == 90 ? 1 : 1 / std::tan(fov * std::numbers::pi / 360)};
  double const scale_x{double(artwork.right - artwork.left + 1) / source_width * zoom};
  double const scale_y{double(artwork.bottom - artwork.top + 1) / source_height * zoom};
  // the 3D projection's origin lies at left + width/2, bottom - height/2
  double const centre_x{scene.left + (scene.right - scene.left + 1) / 2.0};
  double const centre_y{scene.bottom - (scene.bottom - scene.top + 1) / 2.0};
  double const source_x{reference_view.left + (reference_view.right - reference_view.left + 1) / 2.0};
  double const source_y{reference_view.bottom - (reference_view.bottom - reference_view.top + 1) / 2.0};
  auto const clip{intersect(scene, {0, 0, static_cast<int>(world.image.width) - 1, static_cast<int>(world.image.height) - 1})};
  std::vector<int> columns(source_width + 1);
  for(unsigned int x{0}; x <= source_width; ++x)
    columns[x] = static_cast<int>(std::ceil(centre_x + (x - source_x) * scale_x));
  for(unsigned int y{0}; y != source_height; ++y) {
    int const first{static_cast<int>(std::ceil(centre_y + (y - source_y) * scale_y))};
    int const next{static_cast<int>(std::ceil(centre_y + (y + 1 - source_y) * scale_y))};
    int const top{std::max(clip.top, first)}, bottom{std::min(clip.bottom + 1, std::max(first + 1, next))};
    if(top >= bottom) continue;
    auto const *mask{overlay.coverage.data() + size_t{y} * source_width};
    auto const *source{overlay.image.pixels.data() + size_t{y} * source_width};
    auto *out{world.image.pixels.data() + size_t(top) * world.image.width};
    unsigned int x{0};
    while(x != source_width) {
      while(x != source_width && !mask[x]) ++x;
      int const start{std::clamp(columns[x], clip.left, clip.right + 1)};
      int end{start};
      while(x != source_width && mask[x]) {
        // keep subpixel strokes visible instead of dropping entire box edges
        int const left{std::clamp(columns[x], clip.left, clip.right + 1)};
        end = std::clamp(std::max(columns[x] + 1, columns[x + 1]), clip.left, clip.right + 1);
        std::fill(out + left, out + end, source[x]);
        ++x;
      }
      int const count{end - start};
      if(count) for(int yy{top + 1}; yy != bottom; ++yy)
        std::copy_n(out + start, count, out + size_t(yy - top) * world.image.width + start);
    }
  }
}
} // namespace apache::render
