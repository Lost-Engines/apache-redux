#include "instrument_font.h"
#include <limits>
#include <stdexcept>

namespace apache::render {
instrument_font::instrument_font(resources::executable_data const &executable, bool const large)
  : glyph_width{large ? 5u : 3u},
    glyph_height{large ? 9u : 5u} {
  auto const source{executable.at(large ? 0x4f0ddc : 0x4f0b50, 130 * glyph_height)};
  rows.assign(source.begin(), source.end());
}
int instrument_font::width(std::string_view const text) const {
  size_t const count{text.substr(0, text.find('\0')).size()};
  if(count > static_cast<size_t>(std::numeric_limits<int>::max()) / (glyph_width + 1)) throw std::runtime_error{"Instrument text is too long"};
  return static_cast<int>(count * (glyph_width + 1));
}
void instrument_font::character(framebuffer &target, uint8_t const code, int const x, int const y, uint8_t const colour) const {
  if(code >= 130) return;
  for(unsigned int row{0}; row != glyph_height; ++row) {
    for(unsigned int column{0}; column != glyph_width; ++column) {
      if(rows[code * glyph_height + row] & (128u >> column)) target.point(x + static_cast<int>(column), y + static_cast<int>(row), colour);
    }
  }
}
void instrument_font::draw(framebuffer &target, std::string_view const text, int x, int const y, uint8_t const colour) const {
  for(uint8_t const code : text) {
    if(!code) break;
    character(target, code, x, y, colour);
    x += static_cast<int>(glyph_width + 1);
  }
}
} // namespace apache::render
