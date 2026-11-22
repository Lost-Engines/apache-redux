#include <algorithm>
#include <format>
#include <stdexcept>
#include "mfd.h"

namespace apache::render {
void mfd_renderer::flight_plan(framebuffer &target, rectangle const area, std::span<waypoint_mark const> const route, size_t const current, int32_t const x, int32_t const y, uint32_t const heading, int32_t const airspeed, math::trigonometry const &trig) const {
  off(target, area);
  if(route.empty()) return;
  if(current >= route.size()) throw std::runtime_error{"Invalid flight-plan instrument state"};
  auto const distance{[&](waypoint_mark const &point){ int64_t const dx{math::subtract(point.x, x)}, dy{math::subtract(point.y, y)}; return math::integer_sqrt(static_cast<uint64_t>(dx * dx) + static_cast<uint64_t>(dy * dy)); }};
  auto const to_waypoint{distance(route[current])};
  uint32_t range{to_waypoint};
  if(current + 1 < route.size()) range = std::max(range, distance(route[current + 1]));
  else if(current) range = std::max(range, distance(route[current - 1]));
  // A coincident route has no drawable extent. Keep its empty page bounded.
  if(!range || range > 0x3fffffff) return;
  int32_t const extent{static_cast<int32_t>(range)};
  int32_t step{0x3b56000};
  for(auto const &[limit, spacing] : std::array<std::array<int32_t, 2>, 6>{{{0x76ac00, 0x17bc00}, {0xed5800, 0x3b5600}, {0x1dab000, 0x76ac00}, {0x3b56000, 0xed5800}, {0x76ac000, 0x1dab000}, {0xed58000, 0x3b56000}}}) if(extent < limit) { step = spacing; break; }
  projection2 plane{area, {math::subtract(x, extent), math::subtract(y, extent), math::add(x, extent), math::add(y, extent)}};
  rasterizer raster{target};
  auto const line{[&](int32_t const ax, int32_t const ay, int32_t const bx, int32_t const by, uint8_t const colour){ if(auto const points{plane.clip_line({ax, ay}, {bx, by})}) raster.line((*points)[0], (*points)[1], colour); }};
  for(int32_t offset{0}; offset < extent; offset += step) {
    line(math::subtract(x, offset), math::subtract(y, extent), math::subtract(x, offset), math::add(y, extent), 209);
    line(math::add(x, offset), math::subtract(y, extent), math::add(x, offset), math::add(y, extent), 209);
    line(math::subtract(x, extent), math::subtract(y, offset), math::add(x, extent), math::subtract(y, offset), 209);
    line(math::subtract(x, extent), math::add(y, offset), math::add(x, extent), math::add(y, offset), 209);
  }
  plane.cosine = trig.cosine(heading); plane.sine = trig.sine(heading);
  for(size_t i{1}; i != route.size(); ++i) line(route[i - 1].x, route[i - 1].y, route[i].x, route[i].y, 211);
  int const vertical{high ? -3 : -2};
  auto const label{[&](waypoint_mark const &waypoint, uint8_t const colour){ std::string const value(1, static_cast<char>(waypoint.label)); auto const point{plane.project({waypoint.x, waypoint.y})}; font.draw(target, value, point.x - (font.width(value) / 2 - 1), point.y + vertical, colour); }};
  for(size_t i{0}; i != route.size(); ++i) label(route[i], i == current ? 222 : 217);
  label(route[current], 222); label({x, y, '+'}, 222);
  projection2 const labels{area, {-0x100000, -0x100000, 0x100000, 0x100000}};
  auto const text{[&](std::string_view const value, int32_t const px, int32_t const py, bool const right){ auto const point{labels.project({px, py})}; font.draw(target, value, point.x - (right ? font.width(value) - 1 : 0), point.y + vertical, 221); }};
  auto const degrees{[](uint32_t const angle){ int const value{math::multiply_q31(std::bit_cast<int32_t>(angle), 180)}; return value < 0 ? value + 360 : value; }};
  text(std::format("{:03}", degrees(trig.arctangent(math::subtract(route[current].x, x), math::subtract(route[current].y, y)))), 0xe6666, 0xccccc, true);
  text(std::format("{:03}", degrees(heading)), -0xe6666, 0xccccc, false);
  text(std::format("{:02}.{:02}", to_waypoint / 0x17bc00, to_waypoint % 0x17bc00 / 0x3cc2), -0xe6666, -0xccccc, false);
  int32_t const seconds{airspeed ? static_cast<int32_t>(to_waypoint / (int64_t{airspeed} * 432)) : 0};
  text(airspeed ? std::format("{:02}:{:02}", seconds / 60, seconds % 60) : "--:--", 0xe6666, -0xccccc, true);
}
} // namespace apache::render
