#include <algorithm>
#include <format>
#include "mfd.h"

namespace apache::render {
void mfd_renderer::weapons(framebuffer &target, rectangle const area, weapon_instruments const &state) const {
  target.reset_clip();
  std::string_view const image{high ? "CPG_1.640" : "CPG_1.320"};
  panels.copy(target, image, high ? 52295 : 13348, area);
  auto const sprite{[&](int const x, int const y, int const width, int const height, size_t const offset){
    panels.copy(target, image, offset, {area.left + x, area.top + y, area.left + x + width - 1, area.top + y + height - 1}, true);
  }};
  for(size_t side{0}; side != 2; ++side) {
    auto const rounds{state.wingtip[side]};
    int const x{high ? (side ? 124 : 5) : (side ? 62 : 2)};
    if(rounds == 2) sprite(x, high ? 51 : 27, high ? 11 : 5, high ? 15 : 6, high ? 57652 : 14427);
    if(rounds == 1 || rounds == 2) sprite(x, high ? 66 : 34, high ? 11 : 5, high ? 15 : 6, high ? 57652 : 14427);
  }
  unsigned int hydra255{0}, hydra261{0};
  for(size_t pair{0}; pair != 2; ++pair) {
    auto const &pod{state.pairs[pair]};
    for(size_t side{0}; side != 2; ++side) {
      if(pod.type == 2 || pod.type == 3) {
        int const x{high ? (pair ? (side ? 91 : 35) : (side ? 108 : 18)) : (pair ? (side ? 46 : 16) : (side ? 54 : 8))};
        unsigned int const count{std::min(pod.rounds[side], 4u)};
        for(unsigned int slot{4 - count}; slot != 4; ++slot) sprite(x + static_cast<int>(slot % 2) * (high ? 8 : 4), slot < 2 ? (high ? 50 : 26) : (high ? 66 : 34), high ? 6 : pair ? 3 : 4, high ? 15 : 7, high ? 57663 : 14113);
      } else if(pod.type == 4 || pod.type == 5) {
        int const x{high ? (pair ? (side ? 90 : 34) : (side ? 108 : 16)) : (pair ? (side ? 46 : 16) : (side ? 54 : 8))};
        sprite(x, high ? 52 : 27, high ? 16 : 7, high ? 27 : 13, high ? 58318 : 14441);
        (pod.type == 4 ? hydra255 : hydra261) += pod.rounds[side];
      }
    }
  }
  projection2 const plane{area, {0, 0, area.right - area.left + 1, area.bottom - area.top + 1}};
  auto const text{[&](std::string_view const value, int const x, int const y){ auto const point{plane.project({x, y})}; font.draw(target, value, point.x, point.y, 219); }};
  text(std::format("{:04}", state.gun), high ? 59 : 27, high ? 75 : 38);
  text(std::format("{}{:02}", high ? "C:" : "", state.chaff), high ? 59 : 31, high ? 62 : 31);
  text(std::format("{}{:02}", high ? "F:" : "", state.flares), high ? 59 : 31, high ? 51 : 25);
  // Both labels are gated by HYDRA261 ammunition in the original display.
  if(hydra261) {
    text("HYDRA255", high ? 11 : 1, high ? 31 : 14);
    text(std::format("{:02}", hydra255), high ? 29 : 13, high ? 21 : 8);
    text("HYDRA261", high ? 84 : 38, high ? 31 : 14);
    text(std::format("{:02}", hydra261), high ? 102 : 50, high ? 21 : 8);
  }
  text(state.armed ? "ARMED" : "SAFE", high ? (state.armed ? 53 : 59) : 25, high ? 132 : 65);
  text(state.auto_chaff ? "AUTO" : "MAN", high ? (state.auto_chaff ? 10 : 16) : (state.auto_chaff ? 2 : 6), high ? 105 : 51);
  text(state.auto_ecm ? "AUTO" : "OFF", high ? (state.auto_ecm ? 102 : 108) : (state.auto_ecm ? 49 : 53), high ? 105 : 51);
}
} // namespace apache::render
