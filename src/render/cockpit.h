#pragma once
#include <array>
#include <map>
#include <string>
#include <string_view>
#include "framebuffer.h"
#include "palette_effects.h"
#include "resources/data_store.h"

namespace apache::render {
enum class cockpit_section { complete, left, right };
class cockpit {
  unsigned int mode;
  std::map<std::string, resources::indexed_image, std::less<>> sprites;
  std::array<uint8_t, 96> colours{};
public:
  cockpit(resources::data_store const &data, unsigned int video_mode);
  unsigned int video_mode() const { return mode; }
  resources::indexed_image const &sprite(std::string_view name) const;
  void copy(framebuffer &target, std::string_view name, size_t source_offset, rectangle destination, bool masked = false) const;
  void install_palette(palette_rgb &palette) const;
  std::array<palette_fader, 3> faders(int32_t night, bool night_vision) const;
  void draw(framebuffer &target, unsigned int view, bool new_view, cockpit_section section = cockpit_section::complete) const;
};
} // namespace apache::render
