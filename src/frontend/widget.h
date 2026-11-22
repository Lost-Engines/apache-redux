#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include "render/framebuffer.h"

namespace apache::frontend {
struct widget {
  int parent{-1};
  std::string name;
  std::vector<std::string> globals;
  unsigned int type{0};
  render::rectangle area;
  uint32_t flags{0};
  std::string action{};
  std::string picture{};
  std::string text{};
  unsigned int font_height{15};
  bool on{false};
  uint8_t colour{249};
  uint8_t text_colour{251};
  bool shadow{false};
  std::array<int, 4> range{0, 0, 0, 0};
  std::string sprite{};
  std::array<int, 6> sprite_geometry{0, 0, 0, 0, 0, 0};
  unsigned int palette_part{0};
  int selected_line{0};
  unsigned int start_line{0};
  int item{0};
  int number_divisor{1};
  int number_min{0};
  int number_max{0};
  int *number{nullptr};
};
std::vector<widget> initial_widgets();
} // namespace apache::frontend
