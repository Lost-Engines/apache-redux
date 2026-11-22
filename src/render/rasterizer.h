#pragma once
#include <array>
#include <span>
#include <vector>
#include "framebuffer.h"
#include "resources/texture.h"

namespace apache::render {
struct screen_vertex {
  int x{0};
  int y{0};
  int32_t u{0};
  int32_t v{0};
  int32_t intensity{0};
};
struct perspective_vertex {
  screen_vertex screen;
  int32_t depth{1};
};
struct perspective_edge {
  int x{0};
  double reciprocal{0};
  double u{0};
  double v{0};
  double intensity{0};
};
enum class fill_pattern {
  solid,
  fine,
  medium,
  coarse,
  remap,
  heat_haze
};
enum class shading {
  none,
  constant,
  gouraud
};
struct material {
  uint32_t colours{0};
  fill_pattern pattern{fill_pattern::solid};
  std::span<uint8_t const> remapping;
  resources::texture const *image{nullptr};
  bool masked{false};
  uint8_t heat_phase{0};
  shading light{shading::none};
  int32_t intensity{0};
  std::span<uint8_t const> intensity_map;
};
struct scan_edge {
  int x{0};
  int32_t u{0};
  int32_t v{0};
  int32_t intensity{0};
};
class rasterizer {
  framebuffer &target;
  std::vector<scan_edge> left;
  std::vector<scan_edge> right;
  std::vector<perspective_edge> perspective_left;
  std::vector<perspective_edge> perspective_right;
  void scan_perspective(std::span<perspective_vertex const> vertices);
  int first_row{0};
  int last_row{-1};
  void scan(std::span<screen_vertex const> polygon);
  void paint_scans(material const &paint);
public:
  size_t invalid_texture_samples{0};
  explicit rasterizer(framebuffer &destination);
  void line(screen_vertex a, screen_vertex b, uint8_t colour);
  void circle_outline(int x, int y, int radius, uint8_t colour);
  void circle(int x, int y, int radius, material const &paint);
  void fill(std::span<screen_vertex const> vertices, material const &paint);
  void perspective(std::span<perspective_vertex const> vertices, resources::texture const &image, std::span<uint8_t const> intensity_map);
  void polygon(std::span<screen_vertex const> vertices, uint32_t colours, fill_pattern pattern = fill_pattern::solid, std::span<uint8_t const> remapping = {});
};
} // namespace apache::render
