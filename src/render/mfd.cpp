#include "mfd.h"
#include "landscape.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace apache::render {
mfd_renderer::mfd_renderer(cockpit const &artwork, resources::executable_data const &executable) : panels{artwork}, font{executable, artwork.video_mode() != 0}, high{artwork.video_mode() != 0} {}
rectangle mfd_renderer::area(mfd_panel const panel) const {
  std::array<std::array<int, 4>, 5> const low{{{75, 162, 69, 69}, {176, 162, 69, 69}, {23, 161, 69, 69}, {228, 161, 69, 69}, {135, 177, 50, 50}}};
  std::array<std::array<int, 4>, 5> const large{{{149, 323, 140, 140}, {351, 323, 140, 140}, {45, 321, 140, 140}, {455, 321, 140, 140}, {270, 354, 100, 100}}};
  auto const &value{(high ? large : low).at(static_cast<size_t>(panel))};
  return {value[0], value[1], value[0] + value[2] - 1, value[1] + value[3] - 1};
}
void mfd_renderer::corners(framebuffer &target, bool const gunner) const {
  target.reset_clip();
  for(unsigned int side{0}; side != 2; ++side) {
    auto const box{area(static_cast<mfd_panel>((gunner ? 2 : 0) + side))};
    if(high) {
      std::string const prefix{gunner ? (side ? "CPGMFDR" : "CPGMFDL") : (side ? "PLTMFDR" : "PLTMFDL")};
      target.blit(panels.sprite(prefix + "1.640"), box.left, box.top, true);
      target.blit(panels.sprite(prefix + "2.640"), box.right - 4, box.top, true);
      target.blit(panels.sprite(prefix + "3.640"), box.left, box.bottom - 4, true);
      target.blit(panels.sprite(prefix + "4.640"), box.right - 4, box.bottom - 4, true);
    } else {
      target.point(box.left, box.top, 226); target.point(box.right, box.top, 226);
      target.point(box.left, box.bottom, 227); target.point(box.right, box.bottom, 227);
    }
  }
}
void mfd_renderer::off(framebuffer &target, rectangle const area) const {
  target.reset_clip(); target.clip = intersect(target.clip, area); target.fill(area, 208);
}
void mfd_renderer::damaged(framebuffer &target, rectangle const area, uint32_t &seed) const {
  target.reset_clip(); target.clip = intersect(target.clip, area);
  uint32_t mask{0}, random{0};
  for(int y{area.top}; y <= area.bottom; ++y) {
    if(!mask) {
      seed = seed * 0x41c64e6du + 0x3039u; random = seed & 0x7fff0000u;
      seed = seed * 0x41c64e6du + 0x3039u; random |= (seed >> 16) & 0x7fffu;
      mask = 1;
    }
    target.fill({area.left, y, area.right, y}, (random & mask) ? 215 : 208);
    mask <<= 1;
  }
}
void mfd_renderer::engines(framebuffer &target, rectangle const area, engine_instruments const &state) const {
  target.reset_clip();
  panels.copy(target, high ? "PLT_1.640" : "PLT_1.320", high ? 48149 : 12235, area);
  std::array<double, 8> const values{state.torque_left, state.torque_right, state.temperature_left, state.temperature_right, state.rpm_left, state.rotor_rpm, state.rpm_right, state.collective};
  std::array<double, 8> const factors{1.0 / 130, 1.0 / 130, 0.001, 0.001, 1.0 / 110, 1.0 / 110, 1.0 / 110, 1.0 / 130};
  std::array<int, 8> const positions{high ? std::array{13, 25, 47, 59, 81, 91, 101, 121} : std::array{7, 13, 24, 30, 41, 46, 51, 61}};
  int const baseline{area.top + (high ? 110 : 55)};
  for(size_t i{0}; i != values.size(); ++i) {
    double const length{values[i] * (high ? 83.0 : 40.0) * factors[i]};
    if(!std::isfinite(length) || length < INT32_MIN || length > INT32_MAX) throw std::runtime_error{"Invalid engine instrument value"};
    int64_t const endpoint{int64_t{baseline} - static_cast<int>(length)};
    int const top{static_cast<int>(std::clamp<int64_t>(endpoint, 0, target.image.height - 1))};
    target.fill({area.left + positions[i], std::min(top, baseline), area.left + positions[i] + (high ? 1 : 0), std::max(top, baseline)}, 222);
  }
}
void mfd_renderer::flir(framebuffer &target, rectangle const area, landscape &world, model_instance *const object, math::vector3 const &aim, math::vector3 const &observer, sky_lighting const &lighting, target_preview_renderer const &preview) const {
  off(target, area);
  std::string_view const name{preview ? preview(area) : object ? world.draw_flir(target, *object, aim, observer, lighting) : "No Target"};
  if(preview && !target.coverage.empty()) {
    // The image was drawn directly into the native framebuffer. Only its label
    // and the bezel corners should be copied from the logical cockpit overlay.
    auto const box{target.clip};
    for(int y{box.top}; y <= box.bottom; ++y) for(int x{box.left}; x <= box.right; ++x)
      target.coverage[size_t(y) * target.image.width + x] = 0;
  }
  projection2 const plane{area, object ? window2{-0x100000, -0x100000, 0x100000, 0x100000} : window2{-1000, -1000, 1000, 1000}};
  auto const point{plane.project({0, object ? -0xccccc : -800})};
  font.draw(target, name, point.x - (font.width(name) / 2 - 1), point.y, 222);
}
void mfd_renderer::moving_map(framebuffer &target, rectangle const area, landscape const &world, int32_t const x, int32_t const y, uint32_t const heading, int32_t const range) const {
  if(range <= 0 || range > 0x3fffffff) throw std::runtime_error{"Invalid moving-map range"};
  world.draw_map(target, area, {math::subtract(x, range), math::subtract(y, range), math::add(x, range), math::add(y, range)}, heading);
  projection2 const labels{area, {-0x100000, -0x100000, 0x100000, 0x100000}};
  auto const point{labels.project({-0xe6666, -0xccccc})};
  font.draw(target, range == 0x1000000 ? "10" : range == 0x800000 ? "5" : range == 0x400000 ? "2.5" : range == 0x200000 ? "1.3" : range == 0x100000 ? "0.7" : "0.3", point.x, point.y, 208);
}
void mfd_renderer::status(framebuffer &target, rectangle const area, uint32_t const faults, bool const ground_radar, bool const air_radar) const {
  off(target, area);
  projection2 const plane{area, {-0x100000, -0x100000, 0x100000, 0x100000}};
  auto const text{[&](std::string_view const value, int32_t const x, int32_t const y, bool const right = false){
    auto const point{plane.project({x, y})}; font.draw(target, value, point.x - (right ? font.width(value) - 1 : 0), point.y, 222);
  }};
  auto const field{[&](std::string_view const name, std::string_view const value, int32_t const y){ text(name, -0xf3333, y); text(value, 0xf3333, y, true); }};
  text("Status:", -0xf3333, 0xe6666);
  if(auto const line{plane.clip_line({-0xf3333, 0xb3333}, {0xf3333, 0xb3333})}) rasterizer{target}.line((*line)[0], (*line)[1], 222);
  field("RADAR:", (faults & 0x200) ? "FAILED" : ground_radar ? "GROUND" : air_radar ? "AIR" : "OFF", 0x8cccc);
  int row{1};
  if(faults & 0x400) { field("LOW FUEL", "", 0x5851e); ++row; }
  struct warning { uint32_t bit; std::string_view name; };
  std::array const warnings{warning{0x80000, "GUN:"}, warning{0x4000, "PNVS:"}, warning{0x2000, "TADS:"}, warning{0x10000, "ECM:"}, warning{0x8000, "FLIR:"}, warning{0x800, "TV:"}, warning{0x20000, "COMMS:"}, warning{0x1000, "IHADSS:"}};
  for(auto const &warning : warnings) if(faults & warning.bit) {
    int32_t const y{static_cast<int32_t>(576716.8 - row * 214958.08)};
    field(warning.name, warning.bit == 0x1000 ? "DAMAGED" : "FAILED", y); ++row;
  }
}
void mfd_renderer::scores(framebuffer &target, rectangle const area, multiplayer_scores &state, int32_t const delta) const {
  off(target, area);
  auto const saved_clip{target.clip}; target.clip = area;
  projection2 const plane{area, {-0x100000, -0x100000, 0x100000, 0x100000}};
  auto const text{[&](std::string_view value, int32_t x, int32_t y, bool right = false){
    auto const point{plane.project({x, y})}; font.draw(target, value, point.x - (right ? font.width(value) - 1 : 0), point.y, 222);
  }};
  text("KILLS:", -0xf3333, 0xe6666);
  if(auto const line{plane.clip_line({-0xf3333, 0xb3333}, {0xf3333, 0xb3333})}) rasterizer{target}.line((*line)[0], (*line)[1], 222);
  bool const second{state.players.size() > 8 && state.timer >= 1000};
  if(state.players.size() > 8) {
    state.timer = math::add(state.timer, math::multiply_q31(delta, 200));
    if(second && state.timer > 2000) state.timer = 0;
  }
  size_t const first{second ? 8u : 0u};
  for(size_t i{first}; i < state.players.size(); ++i) {
    int32_t const y{static_cast<int32_t>(576716.8 - static_cast<double>(i - first) * 214958.08)};
    text(state.players[i].first, -0xf3333, y); text(std::to_string(state.players[i].second), 0xf3333, y, true);
  }
  target.clip = saved_clip;
}
} // namespace apache::render
