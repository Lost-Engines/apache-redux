#include <format>
#include <stdexcept>
#include "mfd.h"

namespace apache::render {
namespace {
void mark(framebuffer &target, projection2 plane, radar_contact const &contact, int32_t const length, bool const tactical) {
  if(!contact.symbol || contact.symbol > 6 || (tactical && contact.symbol == 4)) return;
  plane.origin = {contact.x, contact.y};
  if(tactical) {
    plane.origin = {math::rounded_q31(math::product_bits(plane.cosine, contact.x) - math::product_bits(plane.sine, contact.y)), math::rounded_q31(math::product_bits(plane.cosine, contact.y) + math::product_bits(plane.sine, contact.x))};
    plane.cosine = INT32_MAX;
    plane.sine = 0;
  }
  uint8_t const colour{static_cast<uint8_t>(tactical || contact.selected ? 222 : 215)};
  auto const line{[&](int32_t const x1, int32_t const y1, int32_t const x2, int32_t const y2){
    if(auto const points{plane.clip_line({x1, y1}, {x2, y2})}) rasterizer{target}.line((*points)[0], (*points)[1], colour);
  }};
  int32_t const n{-length};
  int32_t const p{length};
  switch(contact.symbol) {
  case 1:
    line(n, p, p, n);
    line(n, n, p, p);
    break;
  case 2:
    line(n, p, p, p);
    line(n, n, p, n);
    line(n, p, n, n);
    line(p, p, p, n);
    break;
  case 3:
    line(n, 0, p, 0);
    line(0, n, 0, p);
    break;
  case 4:
    line(n, 0, 0, p);
    line(0, p, p, 0);
    line(p, 0, 0, n);
    line(0, n, n, 0);
    break;
  case 5:
    line(0, p, 0, n);
    line(n, p, p, p);
    break;
  case 6:
    line(0, p, 0, n);
    line(n, n, p, n);
    break;
  }
}
} // anonymous namespace
void mfd_renderer::radar(framebuffer &target, rectangle const area, radar_instruments const &state, std::span<radar_contact const> const contacts, math::trigonometry const &trig) const {
  if(state.range < 1 || state.range > 0x10000000 || state.corner_range < 0 || state.corner_range > 0x20000000 || state.mark_length < 0 || state.mark_length > 0x10000000 || state.range_index > 2 || state.threat < 1 || state.threat > 4 || state.target_distance < 0) throw std::runtime_error{"Invalid radar instrument state"};
  off(target, area);
  projection2 const labels{area, {-0x100000, -0x100000, 0x100000, 0x100000}};
  auto const text{[&](std::string_view const value, int32_t const x, int32_t const y, int const alignment = 0){
    auto const point{labels.project({x, y})};
    int const offset{alignment == 1 ? font.width(value) / 2 - 1 : alignment == 2 ? font.width(value) - 1 : 0};
    font.draw(target, value, point.x - offset, point.y, 222);
  }};
  if(state.mode == radar_mode::inactive) {
    text("RADAR INACTIVE", 0, 0, 1);
    return;
  }
  int32_t const range{state.range};
  int32_t const step{0x17bc00};
  projection2 plane{area, {-range, -range, range, range}};
  rasterizer raster{target};
  auto const line{[&](int32_t const x1, int32_t const y1, int32_t const x2, int32_t const y2, uint8_t const colour){
    if(auto const points{plane.clip_line({x1, y1}, {x2, y2})}) raster.line((*points)[0], (*points)[1], colour);
  }};
  auto const circle{[&](int32_t const radius){
    auto const centre{plane.project({0, 0})};
    raster.circle_outline(centre.x, centre.y, plane.radius(radius), 211);
  }};
  bool const tactical{state.mode == radar_mode::tactical};
  bool const ground{state.mode == radar_mode::ground};
  if(ground) {
    for(int32_t y{-range}; y < range; y += step) line(-range, y, range, y, 209);
    for(int32_t x{0}; x < range; x += step) {
      line(-x, -range, -x, range, 209);
      line(x, -range, x, range, 209);
    }
    line(0, -range, -range, 0, 211);
    line(0, -range, range, 0, 211);
  } else if(tactical) {
    for(int32_t x{0}; x < range; x += step) {
      line(x, -range, x, range, 209);
      line(-x, -range, -x, range, 209);
      line(-range, x, range, x, 209);
      line(-range, -x, range, -x, 209);
    }
    for(int32_t radius{step}; radius < state.corner_range; radius += step) circle(radius);
  } else {
    for(int32_t y{-step * 5}; y <= step * 5; y += step) line(-step * 5, y, step * 5, y, 209);
    for(int32_t x{-step * 5}; x <= step * 5; x += step) line(x, -step * 5, x, step * 5, 209);
    for(int32_t i{1}; i <= 6; ++i) circle(i * step);
  }
  if(!ground) {
    plane.cosine = trig.cosine(state.heading);
    plane.sine = trig.sine(state.heading);
  }
  if(tactical) {
    for(auto const &contact : contacts) mark(target, plane, contact, state.mark_length, true);
    text(range == 3888640 ? "2.5" : "8", -0xe6666, -0xccccc);
    return;
  }
  if(state.failed) {
    text("FAILED", 0, 0, 1);
  } else {
    for(auto const &contact : contacts) {
      if(!contact.selected) mark(target, plane, contact, state.mark_length, false);
    }
    // The selected contact is painted last so other marks cannot cover it.
    for(auto const &contact : contacts) {
      if(contact.selected) mark(target, plane, contact, state.mark_length, false);
    }
  }
  if(state.frozen) text("FROZEN", 0, 0x9eb85, 1);
  if(ground) {
    text(std::array<std::string_view, 3>{"2.5", "5", "10"}[state.range_index], -0xe6666, -0xccccc);
    text(state.tads ? (state.locked ? "TADS" : "BORE") : std::array<std::string_view, 4>{"LOW", "MED", "HIGH", "ALL"}[state.threat - 1], 0x66666, -0xccccc);
  }
  if(!state.tads) {
    text(state.has_target ? state.target_name : "NO TGT", -0xe6666, 0xccccc);
    text(state.has_target ? std::format("{:02}.{:02}", state.target_distance / step, state.target_distance % step / 0x3cc2) : "--.--", 0xe6666, 0xccccc, 2);
  }
  if(!ground) text(state.scan_all ? "ALL" : "ENEMY", 0x66666, -0xccccc);
}
} // namespace apache::render
