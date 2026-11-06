#include "cockpit.h"
#include <algorithm>
#include <stdexcept>
#include "math/fixed.h"

namespace apache::render {
namespace {
struct sprite_definition { std::string_view name; unsigned int width; unsigned int height; unsigned int mode; };
constexpr std::array sprite_definitions{
  sprite_definition{"PLT_1.320", 320, 116, 0},
  sprite_definition{"PLT_2.320", 320, 4, 0},
  sprite_definition{"PLT_3.320", 85, 120, 0},
  sprite_definition{"PLT_4.320", 85, 120, 0},
  sprite_definition{"PLTL45_1.320", 58, 240, 0},
  sprite_definition{"PLTL90_1.320", 17, 191, 0},
  sprite_definition{"PLTL90_2.320", 39, 38, 0},
  sprite_definition{"PLTL90_3.320", 199, 16, 0},
  sprite_definition{"PLTL90_4.320", 87, 15, 0},
  sprite_definition{"PLTR45_1.320", 58, 240, 0},
  sprite_definition{"PLTR90_1.320", 17, 191, 0},
  sprite_definition{"PLTR90_2.320", 39, 38, 0},
  sprite_definition{"PLTR90_3.320", 199, 16, 0},
  sprite_definition{"PLTR90_4.320", 87, 15, 0},
  sprite_definition{"CPG_1.320", 320, 120, 0},
  sprite_definition{"CPG_2.320", 26, 26, 0},
  sprite_definition{"CPG_3.320", 26, 26, 0},
  sprite_definition{"CPGL45_1.320", 96, 120, 0},
  sprite_definition{"CPGL45_2.320", 53, 98, 0},
  sprite_definition{"CPGL45_3.320", 198, 22, 0},
  sprite_definition{"CPGL90_1.320", 145, 48, 0},
  sprite_definition{"CPGR45_1.320", 96, 120, 0},
  sprite_definition{"CPGR45_2.320", 53, 98, 0},
  sprite_definition{"CPGR45_3.320", 198, 22, 0},
  sprite_definition{"CPGR90_1.320", 145, 48, 0},
  sprite_definition{"HOZBEZEL.320", 22, 22, 0},
  sprite_definition{"HOZSHADE.320", 22, 22, 0},
  sprite_definition{"COMBEZEL.320", 17, 9, 0},
  sprite_definition{"COMSHADE.320", 17, 9, 0},
  sprite_definition{"END_YN.320", 149, 15, 0},
  sprite_definition{"PLT_1.640", 640, 232, 1},
  sprite_definition{"PLT_2.640", 640, 8, 1},
  sprite_definition{"PLT_3.640", 171, 240, 1},
  sprite_definition{"PLT_4.640", 171, 240, 1},
  sprite_definition{"PLTL45_1.640", 116, 480, 1},
  sprite_definition{"PLTL90_1.640", 35, 391, 1},
  sprite_definition{"PLTL90_2.640", 78, 75, 1},
  sprite_definition{"PLTL90_3.640", 404, 33, 1},
  sprite_definition{"PLTL90_4.640", 185, 31, 1},
  sprite_definition{"PLTR45_1.640", 116, 480, 1},
  sprite_definition{"PLTR90_1.640", 35, 391, 1},
  sprite_definition{"PLTR90_2.640", 78, 75, 1},
  sprite_definition{"PLTR90_3.640", 404, 33, 1},
  sprite_definition{"PLTR90_4.640", 185, 31, 1},
  sprite_definition{"CPG_1.640", 640, 240, 1},
  sprite_definition{"CPG_2.640", 52, 52, 1},
  sprite_definition{"CPG_3.640", 52, 52, 1},
  sprite_definition{"CPGL45_1.640", 192, 240, 1},
  sprite_definition{"CPGL45_2.640", 106, 195, 1},
  sprite_definition{"CPGL45_3.640", 398, 45, 1},
  sprite_definition{"CPGL90_1.640", 291, 96, 1},
  sprite_definition{"CPGR45_1.640", 192, 240, 1},
  sprite_definition{"CPGR45_2.640", 106, 195, 1},
  sprite_definition{"CPGR45_3.640", 398, 45, 1},
  sprite_definition{"CPGR90_1.640", 291, 96, 1},
  sprite_definition{"HOZBEZEL.640", 43, 43, 1},
  sprite_definition{"HOZSHADE.640", 43, 43, 1},
  sprite_definition{"COMBEZEL.640", 37, 18, 1},
  sprite_definition{"COMSHADE.640", 37, 18, 1},
  sprite_definition{"PLTMFDL1.640", 5, 5, 1},
  sprite_definition{"PLTMFDL2.640", 5, 5, 1},
  sprite_definition{"PLTMFDL3.640", 5, 5, 1},
  sprite_definition{"PLTMFDL4.640", 5, 5, 1},
  sprite_definition{"PLTMFDR1.640", 5, 5, 1},
  sprite_definition{"PLTMFDR2.640", 5, 5, 1},
  sprite_definition{"PLTMFDR3.640", 5, 5, 1},
  sprite_definition{"PLTMFDR4.640", 5, 5, 1},
  sprite_definition{"CPGMFDL1.640", 5, 5, 1},
  sprite_definition{"CPGMFDL2.640", 5, 5, 1},
  sprite_definition{"CPGMFDL3.640", 5, 5, 1},
  sprite_definition{"CPGMFDL4.640", 5, 5, 1},
  sprite_definition{"CPGMFDR1.640", 5, 5, 1},
  sprite_definition{"CPGMFDR2.640", 5, 5, 1},
  sprite_definition{"CPGMFDR3.640", 5, 5, 1},
  sprite_definition{"CPGMFDR4.640", 5, 5, 1},
  sprite_definition{"END_YN.640", 222, 22, 1},
};
struct panel_part { unsigned int mode; unsigned int view; std::string_view name; int x; int y; bool opaque; cockpit_section section{cockpit_section::complete}; };
constexpr std::array panel_parts{
  panel_part{0, 0, "PLT_1.320", 0, 124, true},
  panel_part{0, 0, "PLT_2.320", 0, 120, false},
  panel_part{0, 0, "PLT_3.320", 0, 0, false},
  panel_part{0, 0, "PLT_4.320", 235, 0, false},
  panel_part{0, 1, "PLTL45_1.320", 262, 0, false},
  panel_part{0, 2, "PLTL90_1.320", 0, 11, false, cockpit_section::left},
  panel_part{0, 2, "PLTL90_2.320", 0, 202, false, cockpit_section::left},
  panel_part{0, 2, "PLTL90_3.320", 39, 224, false, cockpit_section::left},
  panel_part{0, 2, "PLTL90_4.320", 233, 0, false, cockpit_section::right},
  panel_part{0, 3, "PLTR45_1.320", 0, 0, false},
  panel_part{0, 4, "PLTR90_1.320", 303, 11, false, cockpit_section::right},
  panel_part{0, 4, "PLTR90_2.320", 281, 202, false, cockpit_section::right},
  panel_part{0, 4, "PLTR90_3.320", 82, 224, false, cockpit_section::right},
  panel_part{0, 4, "PLTR90_4.320", 0, 0, false, cockpit_section::left},
  panel_part{0, 5, "CPG_1.320", 0, 120, true},
  panel_part{0, 5, "CPG_2.320", 0, 94, false},
  panel_part{0, 5, "CPG_3.320", 294, 94, false},
  panel_part{0, 6, "CPGL45_1.320", 224, 0, false},
  panel_part{0, 6, "CPGL45_2.320", 267, 120, false},
  panel_part{0, 6, "CPGL45_3.320", 122, 218, false},
  panel_part{0, 7, "CPGL90_1.320", 175, 0, false},
  panel_part{0, 8, "CPGR45_1.320", 0, 0, false},
  panel_part{0, 8, "CPGR45_2.320", 0, 120, false},
  panel_part{0, 8, "CPGR45_3.320", 0, 218, false},
  panel_part{0, 9, "CPGR90_1.320", 0, 0, false},
  panel_part{1, 0, "PLT_1.640", 0, 248, true},
  panel_part{1, 0, "PLT_2.640", 0, 240, false},
  panel_part{1, 0, "PLT_3.640", 0, 0, false},
  panel_part{1, 0, "PLT_4.640", 469, 0, false},
  panel_part{1, 1, "PLTL45_1.640", 524, 0, false},
  panel_part{1, 2, "PLTL90_1.640", 0, 14, false, cockpit_section::left},
  panel_part{1, 2, "PLTL90_2.640", 0, 405, false, cockpit_section::left},
  panel_part{1, 2, "PLTL90_3.640", 78, 447, false, cockpit_section::left},
  panel_part{1, 2, "PLTL90_4.640", 455, 0, false, cockpit_section::right},
  panel_part{1, 3, "PLTR45_1.640", 0, 0, false},
  panel_part{1, 4, "PLTR90_1.640", 605, 14, false, cockpit_section::right},
  panel_part{1, 4, "PLTR90_2.640", 562, 405, false, cockpit_section::right},
  panel_part{1, 4, "PLTR90_3.640", 158, 447, false, cockpit_section::right},
  panel_part{1, 4, "PLTR90_4.640", 0, 0, false, cockpit_section::left},
  panel_part{1, 5, "CPG_1.640", 0, 240, true},
  panel_part{1, 5, "CPG_2.640", 0, 188, false},
  panel_part{1, 5, "CPG_3.640", 588, 188, false},
  panel_part{1, 6, "CPGL45_1.640", 448, 0, false},
  panel_part{1, 6, "CPGL45_2.640", 534, 240, false},
  panel_part{1, 6, "CPGL45_3.640", 242, 435, false},
  panel_part{1, 7, "CPGL90_1.640", 349, 0, false},
  panel_part{1, 8, "CPGR45_1.640", 0, 0, false},
  panel_part{1, 8, "CPGR45_2.640", 0, 240, false},
  panel_part{1, 8, "CPGR45_3.640", 0, 435, false},
  panel_part{1, 9, "CPGR90_1.640", 0, 0, false},
};
} // namespace
cockpit::cockpit(resources::data_store const &data, unsigned int const video_mode) : mode{video_mode} {
  if(mode > 1) throw std::runtime_error{"Unsupported cockpit video mode"};
  auto const rgb{data.read("COCKPITS/COCKPITS.RGB")};
  if(rgb.size() != colours.size()) throw std::runtime_error{"Invalid cockpit palette size"};
  std::ranges::copy(rgb, colours.begin());
  for(auto const &definition : sprite_definitions) if(definition.mode == mode) {
    resources::indexed_image image;
    image.width = definition.width; image.height = definition.height;
    image.pixels = data.read("COCKPITS/" + std::string{definition.name});
    if(image.pixels.size() != size_t{image.width} * image.height) throw std::runtime_error{"Invalid cockpit sprite size: " + std::string{definition.name}};
    sprites.emplace(definition.name, std::move(image));
  }
}
resources::indexed_image const &cockpit::sprite(std::string_view const name) const {
  auto const found{sprites.find(name)};
  if(found == sprites.end()) throw std::runtime_error{"Unknown cockpit sprite"};
  return found->second;
}
void cockpit::copy(framebuffer &target, std::string_view const name, size_t const source_offset, rectangle const destination, bool const masked) const {
  auto const &image{sprite(name)};
  for(int y{destination.top}; y <= destination.bottom; ++y) for(int x{destination.left}; x <= destination.right; ++x) {
    auto const colour{image.pixels.at(source_offset + static_cast<size_t>(y - destination.top) * image.width + x - destination.left)};
    if(!masked || colour) target.point(x, y, colour);
  }
}
void cockpit::install_palette(palette_rgb &palette) const { std::ranges::copy(colours, palette.begin() + 224 * 3); }
std::array<palette_fader, 3> cockpit::faders(int32_t const night, bool const night_vision) const {
  int32_t const light{night_vision ? 0 : night};
  int32_t const half{math::multiply_q31(light, 0x40000000)}, quarter{math::multiply_q31(light, 0x20000000)};
  return {{{false, 0, 224, 32, half}, {false, 0, 208, 16, quarter}, {false, 0, 192, 16, half}}};
}
void cockpit::draw(framebuffer &target, unsigned int const view, bool const new_view, cockpit_section const section) const {
  if(view > 19) throw std::runtime_error{"Unsupported cockpit view"};
  if(view >= 10) return;
  if(target.image.width != (mode ? 640u : 320u) || target.image.height != (mode ? 480u : 240u)) throw std::runtime_error{"Cockpit and framebuffer resolutions differ"};
  target.reset_clip();
  for(auto const &part : panel_parts) if(part.mode == mode && part.view == view && (section == cockpit_section::complete || part.section == section) && (!part.opaque || new_view)) target.blit(sprite(part.name), part.x, part.y, !part.opaque);
}
} // namespace apache::render
