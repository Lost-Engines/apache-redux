#include "scene_palette.h"
#include <algorithm>
#include <stdexcept>
#include <string>
#include "resources/executable.h"

namespace apache::render {
void scene_palette::spread(unsigned int const colour, unsigned int const first, unsigned int const count) {
  for(unsigned int level{0}; level != 256; ++level) intensity[colour * 256 + level] = static_cast<uint8_t>(first + level * count / 256);
}
scene_palette::scene_palette(resources::data_store const &data, std::string_view const zone, bool const overcast) {
  auto const load{[&](std::string const &name, size_t const first, size_t const count){
    auto const file{data.read(name)};
    if(file.size() < count * 3) throw std::runtime_error{"Truncated palette: " + name};
    std::copy_n(file.begin(), count * 3, colours.begin() + static_cast<ptrdiff_t>(first * 3));
  }};
  for(unsigned int colour{0}; colour != 256; ++colour) {
    spread(colour, colour, 0);
    visual_remap[colour] = static_cast<uint8_t>(colour);
  }
  load("3DDATA/3DPAL.RGB", 0, 224);
  colours[203 * 3] = 64; colours[203 * 3 + 1] = 64; colours[203 * 3 + 2] = 128;
  std::array<std::array<unsigned int, 2>, 14> constexpr ramps{{{2, 3}, {5, 12}, {17, 12}, {29, 12}, {41, 8}, {49, 8}, {57, 7}, {64, 7}, {156, 12}, {71, 12}, {83, 16}, {99, 16}, {115, 12}, {127, 8}}};
  for(auto const &[first, count] : ramps) for(unsigned int i{0}; i != count; ++i) spread(first + i, first, i + 1);
  spread(155, 155, 1);
  spread(0xaa, 0x57, 0);
  spread(0xab, 0x56, 1);
  spread(0xac, 0x56, 2);
  spread(0xad, 0x55, 3);
  spread(0xae, 0x55, 4);
  spread(0xaf, 0x54, 5);
  spread(0xb0, 0x54, 6);
  spread(0xb1, 0x53, 7);
  spread(0xb4, 0x57, 0);
  spread(0xb5, 0x56, 1);
  spread(0xb6, 0x56, 1);
  spread(0xb7, 0x55, 2);
  spread(0xb8, 0x55, 2);
  spread(0xb9, 0x54, 2);
  spread(0xba, 0x54, 2);
  spread(0xbb, 0x53, 3);
  spread(0xbe, 0x67, 0);
  spread(0xbf, 0x66, 1);
  spread(0xc0, 0x66, 2);
  spread(0xc1, 0x65, 3);
  spread(0xc2, 0x65, 4);
  spread(0xc3, 100, 5);
  spread(0xc4, 100, 6);
  spread(0xc5, 99, 7);
  spread(200, 0x67, 0);
  spread(0xc9, 0x66, 2);
  spread(0xca, 0x66, 4);
  spread(0xcb, 0x65, 6);
  spread(0xcc, 0x65, 8);
  spread(0xcd, 100, 10);
  spread(0xce, 100, 0xc);
  spread(0xcf, 99, 0xe);
  spread(0xd2, 0x7a, 0);
  spread(0xd3, 0x79, 1);
  spread(0xd4, 0x79, 1);
  spread(0xd5, 0x78, 3);
  spread(0xd6, 0x78, 3);
  spread(0xd7, 0x77, 4);
  spread(0xd8, 0x76, 6);
  spread(0xd9, 0x76, 6);
  spread(0xdc, 0x78, 0);
  spread(0xdd, 0x78, 1);
  spread(0xde, 0x77, 2);
  spread(0xdf, 0x77, 3);
  spread(0xe0, 0x77, 3);
  spread(0xe1, 0x76, 4);
  spread(0xe2, 0x76, 4);
  spread(0xe3, 0x76, 5);
  spread(0xe6, 0x81, 0);
  spread(0xe7, 0x81, 1);
  spread(0xe8, 0x81, 2);
  spread(0xe9, 0x80, 3);
  spread(0xea, 0x80, 4);
  spread(0xeb, 0x80, 5);
  spread(0xec, 0x7f, 6);
  spread(0xed, 0x7f, 7);

  std::array<float, 4> constexpr fog_strength{0.04f, 0.26f, 0.47f, 0.74f};
  // The original stores each slope as a 32-bit float before using x87
  // multiplication to truncate the number of colours in an intensity ramp.
  for(size_t bank{0}; bank != fog_intensity.size(); ++bank) {
    double const strength{fog_strength[bank]};
    for(auto const &[first, count, offset, extent] : std::array<std::array<unsigned int, 4>, 5>{{{71, 12, 3, 9}, {83, 16, 4, 12}, {99, 16, 4, 12}, {115, 12, 7, 5}, {127, 8, 2, 6}}}) {
      auto const low{static_cast<unsigned int>(strength * offset + 0.5)};
      auto const high{static_cast<unsigned int>((1.0 - strength) * extent + offset + 0.5)};
      float const slope{static_cast<float>((high - low) * static_cast<double>(1.0f / static_cast<float>(count)))};
      for(unsigned int i{0}; i != count; ++i) {
        auto const length{static_cast<unsigned int>(i * static_cast<double>(slope))};
        for(unsigned int level{0}; level != 256; ++level) fog_intensity[bank][(first + i) * 256 + level] = static_cast<uint8_t>(first + low + level * length / 256);
      }
    }
  }

  std::string const prefix{std::string{zone} + ".WZ/"};
  load(prefix + "TERRAIN.RGB", 71, 64);
  // Infrared mappings are built before fog and sky colours are installed.
  mfd_infrared.fill(208);
  auto const ramp{[](uint8_t const value, int const low = 32){ return static_cast<uint8_t>((std::clamp(static_cast<int>(value), low, 223) - 32) / 12 + 208); }};
  for(size_t i{2}; i != 192; ++i) {
    auto const light{luminance(colours[i * 3], colours[i * 3 + 1], colours[i * 3 + 2])};
    infrared[i] = ramp(light);
    mfd_infrared[i] = i <= 71 ? ramp(static_cast<uint8_t>(light + 90), 170) : i >= 155 && i <= 168 ? ramp(static_cast<uint8_t>(light + 126)) : i > 168 ? ramp(static_cast<uint8_t>(light - 24)) : ramp(light);
    hud_nvs[i] = i <= 71 ? ramp(static_cast<uint8_t>(light - 60), 150) : i >= 155 && i <= 168 ? static_cast<uint8_t>(208 + (i - 155) / 2) : i > 168 ? ramp(static_cast<uint8_t>(light - 24)) : i >= 135 && i <= 138 ? static_cast<uint8_t>(i + 332) : ramp(static_cast<uint8_t>(light - 60));
  }
  grassy = zone == "FORT" || zone == "KOREA";
  resources::executable_data const embedded{data};
  std::ranges::copy(embedded.at(0x4ef37c, grassy_colours.size()), grassy_colours.begin());
  std::array<std::string_view, 3> constexpr names{"CLEAR.RGB", "OVERCAST.RGB", "ABOVE.RGB"};
  for(size_t i{0}; i != names.size(); ++i) {
    auto const file{data.read("3DDATA/" + std::string{names[i]})};
    if(file.size() != sky_colours[i].size()) throw std::runtime_error{"Invalid sky palette"};
    std::ranges::copy(file, sky_colours[i].begin());
  }
  load(prefix + (overcast ? "FOGDULL.RGB" : "FOG.RGB"), 135, 20);
  auto const mapping{data.read(prefix + "GLASS.CMP")};
  if(mapping.size() != glass.size()) throw std::runtime_error{"Invalid glass colour map"};
  std::ranges::copy(mapping, glass.begin());
  modified = colours;
  apply_terrain_palette(modified);
  glass[155] = 155;
  for(size_t i{156}; i != 168; ++i) glass[i] = static_cast<uint8_t>(i - 1);
  for(size_t i{224}; i != 256; ++i) glass[i] = static_cast<uint8_t>(i);
}
void scene_palette::apply_terrain_palette(palette_rgb &target) const {
  if(grassy) std::ranges::copy(grassy_colours, target.begin() + 83 * 3);
}
void scene_palette::prepare(unsigned int const infrared_mode) {
  if(infrared_mode > 2) throw std::runtime_error{"Invalid infrared mode"};
  for(size_t i{0}; i != visual_remap.size(); ++i) visual_remap[i] = infrared_mode == 2 ? mfd_infrared[i] : static_cast<uint8_t>(i);
}
void scene_palette::update(palette_conditions const &conditions) {
  if(conditions.cloud_mode > 2 || conditions.weather > 3 || conditions.infrared > 2) throw std::runtime_error{"Invalid palette conditions"};
  size_t sky{conditions.weather && conditions.cloud_mode != 2 ? 1u : 0u};
  if(conditions.cloud_mode == 2 && ((conditions.cloud_flags & 2) || conditions.weather == 1)) sky = 2;
  std::ranges::copy(sky_colours[sky], colours.begin() + 155 * 3);
  int32_t weather_level{0}; uint8_t weather_colour{187};
  if((conditions.cloud_flags & 2) || conditions.weather) {
    if(conditions.cloud_mode == 0) {
      int64_t const distance{int64_t{conditions.cloud_bottom} - conditions.altitude};
      if(distance < 0x20000) weather_level = static_cast<int32_t>(std::clamp((0x1ffff - distance) * 0x4000, int64_t{0}, int64_t{INT32_MAX}));
      if(conditions.weather == 2) weather_level = (weather_level >> 1) + 0x20000000;
      else if(conditions.weather == 3) weather_level = (weather_level >> 3) + 0x30000000;
    } else if(conditions.cloud_mode == 1) {
      int64_t const distance{int64_t{conditions.cloud_top} - conditions.altitude};
      if(distance < 0x10000) { weather_colour = 255; weather_level = static_cast<int32_t>(std::clamp((0xffff - distance) * 0x8000, int64_t{0}, int64_t{INT32_MAX})); } else weather_level = INT32_MAX;
    } else {
      weather_colour = 255;
      int64_t const distance{int64_t{conditions.altitude} - conditions.cloud_top};
      if(distance < 0x20000) weather_level = static_cast<int32_t>(std::clamp((0x1ffff - distance) * 0x4000, int64_t{0}, int64_t{INT32_MAX}));
    }
  }
  std::vector<palette_fader> faders{
    {false, 1000, 2, 190, weather_level, {weather_colour, weather_colour, weather_colour}},
    {false, 2000, 2, 190, conditions.sun_glare, {255, 255, 215}},
    {false, 3000, 2, 166, conditions.infrared == 1 ? 0 : std::clamp(conditions.night, 0, 1964947537), {0, 0, 0}},
    {false, 3000, 255, 1, conditions.cockpit_glass, {0, 0, 0}},
    {true, 4000, 2, 190, conditions.infrared == 1 ? 1 : 0, {0, 1, 1}}
  };
  faders.insert(faders.end(), additional_faders.begin(), additional_faders.end());
  auto base{colours};
  apply_terrain_palette(base);
  modified = apply_faders(base, faders);
}
} // namespace apache::render
