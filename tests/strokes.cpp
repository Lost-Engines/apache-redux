#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include "render/projection2.h"
#include "render/vector_hud.h"

namespace {
namespace render = apache::render;
void require(bool const value, char const *const message) {
  if(!value) throw std::runtime_error{message};
}
void wide_lines() {
  render::framebuffer frame{64, 48};
  render::rasterizer raster{frame};
  frame.clip = {4, 4, 59, 43};
  for(double width : {1.1, 2.0, 3.0, 5.0}) {
    frame.clear();
    raster.wide_line({20, 8}, {20, 40}, 17, width);
    int count{0};
    for(int x{0}; x != 64; ++x) count += frame.image.pixels[20 * 64 + x] == 17;
    require(count == static_cast<int>(std::ceil(width)), "model stroke does not retain its minimum width");
  }
  for(auto const endpoints : {std::array<render::screen_vertex, 2>{{{-1000, 10}, {1000, 35}}}, {{{0, 0}, {63, 47}}}, {{{32, 24}, {32, 24}}}}) {
    frame.clear();
    raster.wide_line(endpoints[0], endpoints[1], 17, 5);
    size_t ink{0};
    for(int y{0}; y != 48; ++y) {
      for(int x{0}; x != 64; ++x) {
        if(frame.image.pixels[y * 64 + x] != 17) continue;
        require(frame.clip.contains(x, y), "wide model stroke escaped clipping");
        ++ink;
      }
    }
    require(ink > 0, "wide model stroke vanished");
  }
}
void optical_lines() {
  render::framebuffer frame{640, 480};
  frame.image.colours[17] = {40, 60, 80};
  frame.image.colours[223] = {0, 255, 32};
  frame.image.colours[208] = {0, 0, 0};
  render::display_layout const layout{640, 480, 640, 480, {0, 0, 639, 479}, 90};
  frame.clear(17);
  render::vector_hud vectors{frame, layout.hud_transform({0, 0, 639, 479}, 90), 223};
  vectors.line({20.125, 30.25}, {310.125, 217.25}, frame.clip);
  auto const ink{frame.hud_coverage};
  require(std::ranges::count_if(ink, [](auto value){return value > 0 && value < 255;}) > 200, "HUD has no antialiased edge coverage");
  vectors.line({20.125, 30.25}, {310.125, 217.25}, frame.clip);
  require(frame.hud_coverage == ink, "overlapping HUD strokes change brightness");
  auto const pixels{frame.colour_pixels()};
  auto const partial{std::ranges::find_if(ink, [](auto value){return value > 20 && value < 200;})};
  require(partial != ink.end(), "HUD edge lacks fractional coverage");
  size_t const at{static_cast<size_t>(partial - ink.begin())};
  unsigned int const alpha{*partial};
  require(pixels[at].g == (60 * (255 - alpha) + 255 * alpha + 127) / 255, "green HUD edge does not blend against the scene");
  frame.hud_colour = 208;
  require(frame.colour_pixels()[at].g == (60 * (255 - alpha) + 127) / 255, "black HUD edge does not blend against the scene");
  render::framebuffer tail{9, 1};
  tail.clear(17);
  tail.image.colours = frame.image.colours;
  tail.hud_colour = 223;
  tail.hud_coverage.assign(9, 0);
  tail.hud_coverage[7] = 128;
  tail.hud_coverage[8] = 255;
  auto const remainder{tail.colour_pixels()};
  require(remainder[0].g == 60 && remainder[7].g == (60 * 127 + 255 * 128 + 127) / 255 && remainder[8].g == 255, "sparse HUD conversion loses a block edge or final pixel");
  vectors.line({20.25, 30.25}, {310.25, 217.25}, frame.clip);
  require(frame.hud_coverage != ink, "subpixel HUD movement is rounded to logical pixels");

  render::projection2 const plane{{0, 0, 639, 479}, {-0x100000, -0xc0000, 0x100000, 0xc0000}};
  auto const a{plane.project_precise({0, 0})};
  auto const b{plane.project_precise({1, 0})};
  require(b[0] > a[0] && b[0] - a[0] < 0.001, "HUD projection discards subpixel positions");
  for(uint32_t pattern : {0u, 0xaaaaaaaau, UINT32_MAX}) {
    render::vector_hud dashed{frame, layout.hud_transform({0, 0, 639, 479}, 90), 223};
    dashed.line({100, 100}, {200, 100}, frame.clip, pattern);
    size_t const painted{static_cast<size_t>(std::ranges::count_if(frame.hud_coverage, [](auto value){return value > 200;}))};
    require(pattern == 0 ? painted == 0 : pattern == UINT32_MAX ? painted > 95 : painted > 30 && painted < 70, "HUD dash pattern does not preserve its on/off segments");
  }
}
void clipping_and_occlusion() {
  for(auto const size : {std::array{640u, 480u}, {1437u, 911u}, {3840u, 2160u}, {800u, 1000u}}) {
    for(double fov : {30.0, 90.0, 150.0}) {
      render::framebuffer frame{size[0], size[1]};
      render::display_layout const layout{size[0], size[1], 640, 480, {0, 0, 639, 247}, fov};
      render::vector_hud vectors{frame, layout.hud_transform({0, 0, 639, 247}, fov), 223};
      vectors.line({-10000, -10196}, {10000, 9804}, {0, 0, 639, 247});
      size_t ink{0};
      for(unsigned int y{0}; y != size[1]; ++y) {
        for(unsigned int x{0}; x != size[0]; ++x) {
          if(!frame.hud_coverage[size_t(y) * size[0] + x]) continue;
          require(layout.scene.contains(static_cast<int>(x), static_cast<int>(y)), "vector HUD escaped the windscreen at a custom FOV");
          ++ink;
        }
      }
      require(ink > 0, "vector HUD disappeared after clipping");
    }
  }
  render::framebuffer frame{64, 48};
  frame.hud_coverage.assign(frame.image.pixels.size(), 255);
  frame.fill({4, 5, 10, 12}, 17);
  require(frame.hud_coverage[5 * 64 + 4] == 0 && frame.hud_coverage[5 * 64 + 3] == 255, "native menu fill does not cover the HUD");
  frame.point(20, 20, 17);
  require(frame.hud_coverage[20 * 64 + 20] == 0, "native lettering does not cover the HUD");
  apache::resources::indexed_image sprite;
  sprite.width = 2;
  sprite.height = 1;
  sprite.pixels = {17, 0};
  frame.blit(sprite, 30, 20, true);
  require(frame.hud_coverage[20 * 64 + 30] == 0 && frame.hud_coverage[20 * 64 + 31] == 255, "transparent menu sprite hides the wrong HUD pixels");
  render::framebuffer layer{32, 24};
  layer.coverage.assign(layer.image.pixels.size(), 0);
  layer.point(20, 15, 17);
  render::display_layout const layout{64, 48, 32, 24, {0, 0, 31, 23}, 90};
  layout.composite(layer, frame);
  require(frame.hud_coverage[30 * 64 + 40] == 0 && frame.hud_coverage[31 * 64 + 41] == 0 && frame.hud_coverage[30 * 64 + 39] == 255, "scaled menu does not cover the HUD at exact sampling boundaries");
  frame.clear();
  require(frame.hud_coverage.empty(), "old HUD strokes survive a cleared frame");
}
} // anonymous namespace
int main() try {
  wide_lines();
  optical_lines();
  clipping_and_occlusion();
  std::cout << "Model stroke widths, native HUD edges, subpixel projection, dashes, clipping and occlusion passed\n";
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << error.what() << '\n';
  return EXIT_FAILURE;
}
