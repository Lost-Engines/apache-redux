#include "helmet_display.h"
#include <algorithm>
#include <bit>
#include <format>
#include <stdexcept>

namespace apache::render {
namespace {
int32_t approximate_range(int32_t const x, int32_t const y) {
  auto const a{x < 0 ? math::negate(x) : x}, b{y < 0 ? math::negate(y) : y};
  return math::add(std::bit_cast<int32_t>(static_cast<uint32_t>(std::max(a, b)) * 4), std::min(a, b)) >> 2;
}
class helmet_drawing {
  framebuffer &target;
  instrument_font const &font;
  math::trigonometry const &trig;
  helmet_instruments const &state;
  bool high;
  rectangle viewport;
  rectangle sight_viewport;
  projection2 plane;
  rasterizer raster;
  void line(int32_t const x, int32_t const y, int32_t const xx, int32_t const yy, uint32_t const pattern = UINT32_MAX) {
    if(auto const clipped{plane.clip_line({x, y}, {xx, yy})}) {
      if(pattern == UINT32_MAX) raster.line((*clipped)[0], (*clipped)[1], state.colour);
      else {
        // The source line mask rotates one bit per plotted pixel, including
        // both endpoints. Clipping starts a fresh mask at the clipped point.
        auto a{(*clipped)[0]}; auto const b{(*clipped)[1]}; uint32_t mask{pattern};
        int const dx{std::abs(b.x - a.x)}, dy{std::abs(b.y - a.y)}, sx{b.x < a.x ? -1 : 1}, sy{b.y < a.y ? -1 : 1};
        bool const horizontal{dx > dy}; int const major{horizontal ? dx : dy}, minor{horizontal ? dy : dx}; int error{(major + 1) / 2};
        for(int i{0}; i <= major; ++i) {
          mask = std::rotl(mask, 1); if(mask & 1) target.point(a.x, a.y, state.colour);
          if(horizontal) a.x += sx; else a.y += sy;
          error -= minor; if(error <= 0) { error += major; if(horizontal) a.y += sy; else a.x += sx; }
        }
      }
    }
  }
  void text(std::string_view const value, int32_t const x, int32_t const y, int const align, int const y_adjust) {
    auto point{plane.project({x, y})};
    if(align == 1) point.x -= font.width(value);
    else if(align == 2) point.x -= font.width(value) / 2 - 1;
    font.draw(target, value, point.x, point.y + y_adjust, state.colour);
  }
  void box(int32_t const left, int32_t const bottom, int32_t const right, int32_t const top) {
    line(left, bottom, right, bottom); line(left, top, right, top); line(left, bottom, left, top); line(right, bottom, right, top);
  }
  int vertical() const {
    return high ? -3 : -2;
  }
  void rotate(uint32_t const angle) {
    plane.cosine = trig.cosine(angle);
    plane.sine = trig.sine(angle);
  }
public:
  helmet_drawing(framebuffer &destination, instrument_font const &letters, math::trigonometry const &angles, helmet_instruments const &values, bool const large, rectangle const area, rectangle const sight) : target{destination}, font{letters}, trig{angles}, state{values}, high{large}, viewport{area}, sight_viewport{sight}, plane{area, {-0x100000, -0xc0000, 0x100000, 0xc0000}}, raster{destination} {
    target.clip = area;
  }
  void reticle() {
    plane.origin = {0, 0};
    line(0, 0, 0x2147a, 0, 0x3fffffff); line(0, 0, -0x2147a, 0, 0x3fffffff);
    line(0, 0, 0, 0x2147a, 0x3fffffff); line(0, 0, 0, -0x2147a, 0x3fffffff);
  }
  void heading() {
    plane.origin = {0, 0x99999}; line(0, 0, 0, -0xcccc);
    int value{static_cast<int>(state.heading / 0x71c71c7) - 9}; if(value < 0) value += 36;
    int third{value % 3};
    int32_t x{-0x7332c - static_cast<int32_t>(int64_t{state.heading % 0x71c71c7} * 0xcccc / 0x71c71c7)};
    for(int i{0}; i != 19; ++i, x += 0xcccc) {
      line(x, 0xf5c2, x, third ? 0x7ae1 : 0x28f6);
      if(!third) {
        std::string const label{value == 0 ? "N" : value == 9 ? "E" : value == 18 ? "S" : value == 27 ? "W" : std::format("{}", value)};
        auto point{plane.project({x, 0xf5c2})}; point.x += label.size() == 1 ? (high ? -2 : -1) : (high ? -5 : -3);
        font.draw(target, label, point.x, point.y + (high ? -8 : -6), state.colour);
      }
      if(++value == 36) value = 0;
      if(++third == 3) third = 0;
    }
    int32_t const delta{std::clamp(std::bit_cast<int32_t>(state.command_heading - state.heading), -0x40000000, 0x40000000)};
    x = static_cast<int32_t>(int64_t{delta} * 0x7332c / 0x40000000);
    line(x, 0, x - 0x6666, -0x6666); line(x, 0, x + 0x6666, -0x6666);
  }
  void pitch() {
    uint32_t const angle{state.pitch + 0x40000000u};
    int const offset{-static_cast<int>(int64_t{angle % 0x71c71c7} * 0x2eb85 / 0x71c71c7)};
    plane.origin = {0, offset}; rotate(state.roll);
    int direction{-1};
    auto const limit{[&](int value){ if(value > 9) { value = 18 - value; direction = -direction; } else if(value < -9) { value = -18 - value; direction = -direction; } return value; }};
    int value{limit(static_cast<int>(angle / 0x71c71c7) - 7)};
    for(int i{0}, y{offset + 0xbae14}, upper{offset + 0xc7ae0}, lower{offset + 0xae148}; i != 5; ++i, y -= 0x5d70a, upper -= 0x5d70a, lower -= 0x5d70a, value = limit(value + direction)) {
      if(value < -9 || value > 9) continue;
      if(value == 0 || value == 9) {
        int const end{value ? 0x46666 : 0x4cccc}; line(0x19999, y, end, y); line(-0x19999, y, -end, y);
      } else if(value < 0) {
        bool const reverse{direction != -1 && value != -9};
        for(auto const &[a, b] : std::array<std::array<int, 2>, 4>{{{0x19999, 0x20000}, {0x26666, 0x2cccc}, {0x33333, 0x39999}, {0x40000, 0x46666}}}) {
          if(reverse) { line(-a, y, -b, y); line(a, y, b, y); } else { line(a, y, b, y); line(-a, y, -b, y); }
        }
        if(value != -9) {
          int const side{direction == -1 ? 1 : -1}, end{direction == -1 ? lower : upper};
          line(side * 0x19999, y, side * 0x19999, end); line(-side * 0x19999, y, -side * 0x19999, end);
          text(std::format("{}0", value), side * 0x40000, end, 2, vertical());
        }
      } else {
        int const side{direction == -1 ? 1 : -1}, end{direction == -1 ? lower : upper};
        line(side * 0x19999, y, side * 0x46666, y); line(-side * 0x19999, y, -side * 0x46666, y);
        line(side * 0x19999, y, side * 0x19999, end); line(-side * 0x19999, y, -side * 0x19999, end);
        text(std::format("{}0", value), side * 0x40000, direction == -1 ? upper : lower, 2, vertical());
      }
    }
    rotate(0);
  }
  void bank() {
    if(!high) return;
    plane.origin = {0, 0};
    int32_t const roll{std::clamp(std::bit_cast<int32_t>(state.roll), -0x15555555, 0x15555555)};
    auto const point{[&](int32_t const angle, int32_t const radius){ uint32_t const rotated{static_cast<uint32_t>(roll) - static_cast<uint32_t>(angle)}; return vertex2{math::rounded_q31(0ull - math::product_bits(trig.sine(rotated), radius)), math::multiply_q31(trig.cosine(rotated), radius)}; }};
    for(int const direction : {1, -1}) {
      int32_t angle{-direction * 0x15555555}; auto previous{point(angle, 0x8cccc)};
      for(int i{0}; i != 6; ++i) { angle += direction * 0x38e38e4; auto const next{point(angle, 0x8cccc)}; line(previous.x, previous.y, next.x, next.y); previous = next; }
    }
    std::array<int32_t, 13> constexpr ticks{-0x15555555, -0x11c71c72, -0xe38e38e, -0xaaaaaab, -0x71c71c7, -0x38e38e4, 0, 0x38e38e4, 0x71c71c7, 0xaaaaaab, 0xe38e38e, 0x11c71c72, 0x15555555};
    for(size_t i{0}; i != ticks.size(); ++i) {
      int32_t const inner{i % 6 == 0 ? 0x86666 : i % 2 == 0 ? 0x88f5c : 0x8a3d7}, outer{i % 6 == 0 ? 0x93332 : i % 2 == 0 ? 0x90a3c : 0x8f5c1};
      auto const a{point(ticks[i], inner)}, b{point(ticks[i], outer)}; line(a.x, a.y, b.x, b.y);
    }
  }
  void altitude() {
    plane.origin = {0xccccc, 0x147ae};
    for(int i{0}, y{0x66666}; i != 5; ++i, y -= 0x33333) { line(0, y, -0x7ae1, y); line(0x51eb, y, 0xcccc, y); }
    for(int i{1}; i != 5; ++i) line(0, i * 0xa3d7, -0x51eb, i * 0xa3d7);
    for(int i{1}; i != 5; ++i) line(0, -i * 0xa3d7, -0x51eb, -i * 0xa3d7);
    for(int i{0}, y{-0x3d70a}; i != 4; ++i, y -= 0xa3d7) line(0x51eb, y, 0xa3d6, y);
    int32_t const climb{static_cast<int32_t>(int64_t{std::clamp(state.climb_rate, -0x6400, 0x6400)} * 0x66666 / 0x6400)};
    std::array<vertex2, 3> const arrow{{{-0x7ae1, climb}, {-0xe147, climb - 0x6666}, {-0xe147, climb + 0x6666}}}; raster.polygon(plane.clip(arrow), state.colour * 0x01010101u);
    int32_t const altitude{static_cast<int32_t>(int64_t{std::clamp(state.radar_altitude, 0, 0xc800)} * 0xccccc / 0xc800) - 0x66666};
    std::array<vertex2, 4> const bar{{{0x147a, -0x66666}, {0x147a, altitude}, {0x3d70, altitude}, {0x3d70, -0x66666}}}; raster.polygon(plane.clip(bar), state.colour * 0x01010101u);
    text(std::format("{}", state.radar_altitude / 256), -0xf5c2, 0, 1, vertical());
    text(std::format("{}", state.barometric_altitude / 256), -0xf5c2, 0x59999, 1, vertical());
  }
  void waypoint() {
    plane.origin = {0, 0};
    text(std::format("{}{}.{}", static_cast<char>(state.waypoint_letter), state.waypoint_distance / 0x17bc00, int64_t{state.waypoint_distance % 0x17bc00} * 10 / 0x17bc00), -0x4cccc, -0x48f5c, 1, vertical());
    text(std::format("{}:{:02}", state.waypoint_time / 12000, int64_t{state.waypoint_time % 12000} * 60 / 12000), -0x4cccc, -0x5c28f, 1, vertical());
  }
  void airspeed() {
    plane.origin = {0, 0};
    text(std::format("{}", state.airspeed), -0xb9999, 0xcccc, 1, vertical());
  }
  void engines() {
    plane.origin = {0, 0};
    text(std::format("{}%", state.torque), -0xa6666, 0x6cccc, 1, vertical());
  }
  void device() {
    if(state.view != 13) return;
    box(-0xe6666, 0x80000, -0x80000, 0x99999);
    text(state.viewing_device, -0xe147b, 0x8cccd, 0, vertical()); text(state.magnification, -0x80000, 0x8cccd, 1, vertical());
  }
  void field_of_view() {
    bool const helmet{state.view == 12};
    window2 const window{helmet ? window2{-0x4e38e39, -0x2aaaaab, 0x4e38e38, 0x471c71c} : window2{-0x6e38e39, -0x38e38e4, 0x6e38e38, 0x238e38e}};
    plane = projection2{sight_viewport, window}; target.clip = sight_viewport;
    line(window.left, window.bottom, window.right, window.bottom); line(window.right, window.bottom, window.right, window.top); line(window.right, window.top, window.left, window.top); line(window.left, window.top, window.left, window.bottom);
    int32_t const x{state.sight_heading >> 4}, y{state.sight_pitch >> 4}; box(x - 0xe38e38, y - 0xe38e38, x + 0xe38e38, y + 0xe38e38);
    line(-0x71c71d, 0, 0x71c71c, 0); line(0, -0x71c71d, 0, 0x71c71c);
    plane = projection2{viewport, {-0x100000, -0xc0000, 0x100000, 0xc0000}}; target.clip = viewport;
  }
  void point_direction(math::vector3 const &position) {
    math::vector3 delta; for(size_t i{0}; i != 3; ++i) delta[i] = math::subtract(position[i], state.aircraft.position[i]);
    auto const relative{math::transform(state.camera.rotation, delta)};
    int32_t const range{approximate_range(relative[0], relative[2])}; if(range <= 0) return;
    int32_t x{0}, y{0};
    if(relative[2] == range) y = 0x66666;
    else if(relative[2] == -range) y = -0x66666;
    else if(relative[0] == range) x = 0x66666;
    else if(relative[0] == -range) x = -0x66666;
    else {
      int32_t const rx{static_cast<int32_t>(int64_t{relative[0]} * 0x80000000ll / range)}, ry{static_cast<int32_t>(int64_t{relative[2]} * 0x80000000ll / range)};
      x = math::negate(math::rounded_q31(0ull - math::product_bits(rx, 0x66666)));
      y = math::multiply_q31(ry, 0x66666);
    }
    line(0, 0, x, y, relative[1] > 0 ? UINT32_MAX : 0xf0f0f0f0);
  }
  bool target_in_view(int32_t const limit = 0x100000) const {
    return state.weapons.target_valid && std::abs(int64_t{state.weapons.target.x}) <= limit && std::abs(int64_t{state.weapons.target.y}) <= limit;
  }
  void aiming_box() {
    auto const &w{state.weapons};
    if(target_in_view()) box(w.target.x - 0xcccc, w.target.y - 0xcccc, w.target.x + 0xcccc, w.target.y + 0xcccc);
    else if(w.target_exists && w.selected != 1) point_direction(w.target_position);
  }
  void impact() {
    auto const &w{state.weapons};
    // The original gates this marker on the target's projection, even though
    // the coordinates being drawn belong to the predicted gun impact.
    if(target_in_view()) {
      line(w.impact.x - 0x147ae, w.impact.y - 0x147ae, w.impact.x + 0x147ae, w.impact.y + 0x147ae);
      line(w.impact.x - 0x147ae, w.impact.y + 0x147ae, w.impact.x + 0x147ae, w.impact.y - 0x147ae);
      line(0, 0, w.impact.x, w.impact.y);
    } else if(w.target_exists) point_direction(w.impact_position);
  }
  void targeting_box() {
    auto const angles{[&](math::matrix3 const &rotation){ int32_t const x{rotation[3] >> 16}, y{rotation[4] >> 16}, z{rotation[5] >> 16}; return std::array{trig.arctangent(x, y), trig.arctangent(z, approximate_range(x, y))}; }};
    auto const aircraft{angles(state.aircraft.rotation)}, camera{angles(state.camera.rotation)};
    plane.origin = {math::rounded_q31(0ull - math::product_bits(trig.sine(camera[0] - aircraft[0]), 0x233333)), math::rounded_q31(0ull - math::product_bits(trig.sine(camera[1] - aircraft[1]), 0x233333))};
    rotate(state.roll); int32_t const size{state.weapons.box_half_size};
    // This path selects a line texture but calls the solid-line routine.
    line(size, size, -size, size); line(-size, size, -size, -size); line(-size, -size, size, -size); line(size, -size, size, size);
    plane.origin = {0, 0}; rotate(0);
  }
  void rocket_sight() {
    plane.origin = {0, 0};
    line(0x28f5, 0, 0x147ae, 0); line(-0x28f5, 0, -0x147ae, 0);
    line(0x28f5, 0, 0x28f5, -0x5c28f); line(-0x28f5, 0, -0x28f5, -0x5c28f);
    line(0x28f5, -0x5c28f, 0x147ae, -0x5c28f); line(-0x28f5, -0x5c28f, -0x147ae, -0x5c28f);
    int32_t const y{static_cast<int32_t>(static_cast<double>(state.weapons.rocket_elevation) * -0x1.2p-27 * -0x1.70a3cp+18)};
    line(-0x170a3, y, 0x170a3, y); line(0, y - 0x170a3, 0, y + 0x170a3);
  }
  void intercept() {
    auto const &w{state.weapons}; if(!w.target_exists || !target_in_view()) return;
    line(w.intercept.x - 0xcccc, w.intercept.y, w.intercept.x + 0xcccc, w.intercept.y);
    line(w.intercept.x, w.intercept.y - 0xcccc, w.intercept.x, w.intercept.y + 0xcccc);
  }
  void lock_diamond() {
    auto const &w{state.weapons}; int32_t x{0}, y{0};
    if(w.lock_status == 3) { if(!target_in_view(0xfffff)) return; x = w.target.x; y = w.target.y; }
    line(x - 0xcccc, y, x, y + 0xcccc); line(x, y + 0xcccc, x + 0xcccc, y);
    line(x + 0xcccc, y, x, y - 0xcccc); line(x, y - 0xcccc, x - 0xcccc, y);
  }
  void weapons() {
    auto const &w{state.weapons}; plane.origin = {0, 0};
    std::string ammunition{std::format("{}:{}", w.name, w.rounds)};
    if(w.impact_time != -1) ammunition += std::format(":{}", w.impact_time / 200);
    text(ammunition, 0x26666, -0xa6666, 0, 0);
    std::string status{(state.faults & 0x80000) && w.selected == 1 ? "Failed" : w.landed ? "Safe" : w.armed ? "Armed" : "UnArmed"};
    if(w.target_exists) status += w.target_designated ? ":Designated" : w.target_locked ? ":Locked" : ":Selected";
    text(status, 0x26666, -0x947ae, 0, 0);
    std::string const label{!w.target_exists ? "No Target" : w.target_range < 0x2f7800 ? std::format("{}:{}", w.target_name, w.target_range / 256) : std::format("{}:{:.1f}", w.target_name, static_cast<double>(w.target_range) * 0x1.592744p-21)};
    text(label, -0x26666, -0x947ae, 1, 0);
    std::array<std::string_view, 10> constexpr locks{"Invalid lock data", "No Lock", "No Lock", "Acquired", "Out of Range", "Under Range", "Target Obscured", "No Lock", "Out of Lock Cone", "No IR Signature"};
    text(locks.at(w.lock_status), -0x26666, -0xa6666, 1, 0);
    if(!w.target_exists || !w.target_aircraft) return;
    auto const number{[](char prefix, int32_t value){ if(value < 0) return std::format("{}-{:03}", prefix, -int64_t{value}); return std::format("{}{:03}", prefix, value); }};
    text(number('B', static_cast<int32_t>(trig.arctangent(math::subtract(w.target_position[0], state.aircraft.position[0]), math::subtract(w.target_position[1], state.aircraft.position[1])) / 0xb60b61)), -0xccccc, -0x33333, 0, 0);
    text(number('A', w.target_position[2] / 0x6400), -0xccccc, -0x4cccc, 0, 0);
    text(number('S', w.target_speed / 0x1b0), -0xccccc, -0x66666, 0, 0);
    text(number('H', static_cast<int32_t>(trig.arctangent(w.target_heading[0], w.target_heading[1]) / 0xb60b61)), -0xccccc, -0x80000, 0, 0);
  }
  void targeting() {
    auto const &w{state.weapons}; if(!w.armed) return;
    auto const origin{plane.origin}; plane = projection2{viewport, {-0x100000, -0xc0000, 0x100000, 0xc0000}}; plane.origin = origin; target.clip = viewport;
    switch(w.selected) {
    case 1: impact(); break;
    case 2: case 3: targeting_box(); break;
    case 4: case 5: rocket_sight(); intercept(); break;
    case 6: lock_diamond(); intercept(); break;
    default: throw std::runtime_error{"Invalid helmet weapon selection"};
    }
    if(w.target_exists) aiming_box();
  }
  void tads_box() {
    auto const &w{state.weapons}; if(w.tads_active && w.tads_valid) box(w.tads.x - 0xcccc, w.tads.y - 0xcccc, w.tads.x + 0xcccc, w.tads.y + 0xcccc);
  }
  void complete() {
    if(state.faults & 0x1000) return;
    layout();
    if(state.mode == 0 || state.mode == 2) {
      if(state.mode == 2 && state.weapons.target_exists) aiming_box();
      reticle(); heading(); pitch(); bank(); altitude(); waypoint(); airspeed(); engines(); device(); weapons();
    } else if(state.mode == 1 || state.mode == 3) {
      if(state.mode == 1) { pitch(); airspeed(); waypoint(); engines(); }
      heading(); altitude(); if(state.weapons.selected != 4 && state.weapons.selected != 5) reticle();
      device(); targeting(); weapons();
    } else return;
    if(state.view == 12 || state.view == 13) field_of_view();
  }
  void layout() {
    if(!state.layout_grid) return;
    plane.origin = {0, 0};
    for(auto const &edge : std::array<std::array<vertex2, 2>, 4>{{{{{-0x100000, -0xc0000}, {0x100000, -0xc0000}}}, {{{-0x100000, 0xc0000}, {0x100000, 0xc0000}}}, {{{-0x100000, -0xc0000}, {-0x100000, 0xc0000}}}, {{{0x100000, -0xc0000}, {0x100000, 0xc0000}}}}}) {
      if(auto const points{plane.clip_line(edge[0], edge[1])}) raster.line((*points)[0], (*points)[1], 1);
    }
    for(int x{-0xffffa}; x < 0xffffb; x += 0x19999) for(int y{-0xffffa}; y < 0xffffb; y += 0x19999) { auto const point{plane.project({x, y})}; target.point(point.x, point.y, 1); }
  }
};
} // anonymous namespace
helmet_display::helmet_display(resources::executable_data const &executable, math::trigonometry const &angles, unsigned int const video_mode) : font{executable, video_mode != 0}, trig{angles}, high{video_mode != 0} {
  if(video_mode > 1) throw std::runtime_error{"Invalid helmet display resolution"};
  auto const rectangle_at{[&](uint32_t const address){ resources::reader input{executable.at(address, 16)}; int const x{input.i32()}, y{input.i32()}, width{input.i32()}, height{input.i32()}; return rectangle{x, y, x + width - 1, y + height - 1}; }};
  for(size_t i{0}; i != viewports.size(); ++i) viewports[i] = rectangle_at(0x4eb668 + static_cast<uint32_t>(i) * 48 + video_mode * 16);
  sight_viewport = rectangle_at(0x4eba28 + video_mode * 16);
}
void helmet_display::draw(framebuffer &target, helmet_instruments const &state) const {
  helmet_drawing drawing{target, font, trig, state, high, viewports.at(state.view), sight_viewport}; drawing.complete();
}
vertex2 helmet_display::unproject_point(unsigned int const view, int32_t const x, int32_t const y) const {
  return projection2{viewports.at(view), {-0x100000, -0xc0000, 0x100000, 0xc0000}}.unproject(x, y);
}
std::optional<vertex2> helmet_display::project_point(unsigned int const view, math::pose const &camera, projection const &scene, math::vector3 const &position) const {
  math::vector3 delta; for(size_t i{0}; i != 3; ++i) delta[i] = math::subtract(position[i], camera.position[i]);
  auto const point{scene.project_unclipped(math::transform(camera.rotation, delta))};
  if(!point || point->x < 0 || point->x >= 800 || point->y < 0 || point->y >= 600) return {};
  return unproject_point(view, point->x, point->y);
}
void helmet_display::draw_component(framebuffer &target, helmet_instruments const &state, helmet_component const component) const {
  helmet_drawing drawing{target, font, trig, state, high, viewports.at(state.view), sight_viewport};
  switch(component) {
  case helmet_component::reticle: drawing.reticle(); break;
  case helmet_component::heading: drawing.heading(); break;
  case helmet_component::pitch: drawing.pitch(); break;
  case helmet_component::bank: drawing.bank(); break;
  case helmet_component::altitude: drawing.altitude(); break;
  case helmet_component::waypoint: drawing.waypoint(); break;
  case helmet_component::airspeed: drawing.airspeed(); break;
  case helmet_component::engines: drawing.engines(); break;
  case helmet_component::viewing_device: drawing.device(); break;
  case helmet_component::field_of_view: drawing.field_of_view(); break;
  case helmet_component::layout: drawing.layout(); break;
  case helmet_component::aiming_box: drawing.aiming_box(); break;
  case helmet_component::impact: drawing.impact(); break;
  case helmet_component::targeting_box: drawing.targeting_box(); break;
  case helmet_component::rocket_sight: drawing.rocket_sight(); break;
  case helmet_component::intercept: drawing.intercept(); break;
  case helmet_component::lock_diamond: drawing.lock_diamond(); break;
  case helmet_component::point_direction: drawing.point_direction(state.weapons.target_position); break;
  case helmet_component::weapons: drawing.weapons(); break;
  case helmet_component::targeting: drawing.targeting(); break;
  case helmet_component::tads_box: drawing.tads_box(); break;
  }
}
} // namespace apache::render
