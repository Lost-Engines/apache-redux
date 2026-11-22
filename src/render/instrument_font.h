#pragma once
#include <string_view>
#include "framebuffer.h"
#include "resources/executable.h"

namespace apache::render {
class instrument_font {
  unsigned int glyph_width;
  unsigned int glyph_height;
  resources::bytes rows;
public:
  instrument_font(resources::executable_data const &executable, bool large);
  int width(std::string_view text) const;
  unsigned int height() const {
    return glyph_height;
  }
  void character(framebuffer &target, uint8_t code, int x, int y, uint8_t colour) const;
  void draw(framebuffer &target, std::string_view text, int x, int y, uint8_t colour) const;
};
} // namespace apache::render
