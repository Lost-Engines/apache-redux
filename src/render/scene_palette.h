#pragma once
#include <array>
#include <string_view>
#include <vector>
#include "palette_effects.h"
#include "resources/data_store.h"

namespace apache::render {
class scene_palette {
  std::array<std::array<uint8_t, 39>, 3> sky_colours{};
  std::array<uint8_t, 96> grassy_colours{};
  bool grassy{false};
  void spread(unsigned int colour, unsigned int first, unsigned int count);
public:
  std::vector<palette_fader> additional_faders;
  palette_rgb colours{};
  palette_rgb modified{};
  std::array<uint8_t, 256> infrared{};
  std::array<uint8_t, 256> mfd_infrared{};
  std::array<uint8_t, 256> hud_nvs{};
  std::array<uint8_t, 65536> intensity{};
  std::array<std::array<uint8_t, 65536>, 4> fog_intensity{};
  std::array<uint8_t, 256> glass{};
  std::array<uint8_t, 256> visual_remap{};
  void grass_colours(std::array<uint8_t, 96> const &values) {
    grassy_colours = values;
  }
  void apply_terrain_palette(palette_rgb &target) const;
  void prepare(unsigned int infrared_mode);
  void update(palette_conditions const &conditions);
  scene_palette(resources::data_store const &data, std::string_view zone, bool overcast = false);
};
} // namespace apache::render
