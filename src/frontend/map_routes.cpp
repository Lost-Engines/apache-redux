#include "map_routes.h"
#include <stdexcept>
#include "map_editor.h"

namespace apache::frontend {
std::vector<map_route_command> map_route_commands(resources::mission_task const &task, size_t const selected, bool const others) {
  std::vector<map_route_command> out{{11}};
  auto const emit{[&](uint32_t kind, resources::route_waypoint const &point){ out.push_back({kind, point.position[0], point.position[1]}); }};
  auto const path{[&](std::span<resources::route_waypoint const> route){
    for(size_t i{0}; i != route.size(); ++i) emit(i ? 2u : 1u, route[i]);
  }};
  auto const symbols{[&](std::span<resources::route_waypoint const> route, bool clear, bool labels){
    auto const letters{waypoint_letters(route)};
    for(size_t i{0}; i != route.size(); ++i) {
      auto const &point{route[i]}; bool const objective{point.type == 4 || point.type == 6};
      emit((i ? (objective ? 3u : 4u) : 5u) + (clear ? 3u : 0u), point);
      if(labels) out.push_back({9, letters[i]});
    }
  }};
  if(others) {
    for(size_t i{0}; i != task.actors.size(); ++i) if(i != selected) path(task.actors[i].waypoints);
    for(size_t i{0}; i != task.actors.size(); ++i) if(i != selected) symbols(task.actors[i].waypoints, true, false);
    for(size_t i{0}; i != task.actors.size(); ++i) if(i != selected) symbols(task.actors[i].waypoints, false, false);
  }
  if(selected < task.actors.size()) {
    auto const &route{task.actors[selected].waypoints};
    out.push_back({10}); path(route); symbols(route, true, false); symbols(route, false, true);
  }
  if(out.size() >= 3072) throw std::runtime_error{"Briefing route exceeds the original command capacity"};
  return out;
}
void draw_map_routes(render::framebuffer &target, map_view const &map, std::span<map_route_command const> const commands, math::trigonometry const &trig, render::instrument_font const &font, bool const primary, bool const others) {
  auto const view{map.projection(trig, static_cast<int>(target.image.width), static_cast<int>(target.image.height))};
  render::rasterizer raster{target};
  bool enabled{false}; uint8_t colour{251}; render::vertex2 previous;
  int32_t const scale{map.position.zoom};
  auto const rotate{[&](int32_t x, int32_t y, uint32_t angle){
    int32_t const sine{trig.sine(angle)}, cosine{trig.cosine(angle)};
    return render::vertex2{math::rounded_q31(math::product_bits(x, cosine) - math::product_bits(y, sine)), math::rounded_q31(math::product_bits(y, cosine) + math::product_bits(x, sine))};
  }};
  auto const line{[&](render::vertex2 const &a, render::vertex2 const &b){ if(auto clipped{view.clip_line(a, b)}) raster.line((*clipped)[0], (*clipped)[1], colour); }};
  auto const polygon{[&](render::vertex2 const &centre, std::span<render::vertex2 const> shape, uint32_t angle, bool clear){
    std::vector<render::vertex2> points; points.reserve(shape.size());
    for(auto const &v : shape) { auto p{rotate(v.x, v.y, angle)}; p.x = math::add(p.x, centre.x); p.y = math::add(p.y, centre.y); points.push_back(p); }
    if(clear) raster.polygon(view.clip(points), 0);
    else for(size_t i{0}; i != points.size(); ++i) line(points[i], points[(i + 1) % points.size()]);
  }};
  for(size_t i{0}; i < commands.size(); ++i) {
    auto const &command{commands[i]}; auto const kind{command.kind}; render::vertex2 const point{command.x, command.y};
    if(!kind) break;
    if(kind == 10 || kind == 11) { enabled = kind == 10 ? primary : others; colour = kind == 10 ? 251 : 248; continue; }
    if(kind == 1) { previous = point; continue; }
    if(kind == 13) continue;
    if(enabled) {
      if(kind == 2) line(previous, point);
      else if(kind == 3 || kind == 6) {
        std::array<render::vertex2, 3> const triangle{{{-scale * 10, -scale * 20 / 3}, {0, scale * 40 / 3}, {scale * 10, -scale * 20 / 3}}};
        polygon(point, triangle, 0u - map.position.rotation, kind == 6);
      } else if(kind == 4 || kind == 7) {
        auto const p{view.project(point)}; int const radius{view.radius(scale * 8)};
        if(kind == 4) raster.circle_outline(p.x, p.y, radius, colour);
        else raster.circle(p.x, p.y, radius, {.colours{0}, .remapping{}, .intensity_map{}});
      } else if(kind == 5 || kind == 8) {
        std::array<render::vertex2, 4> const square{{{-scale * 8, -scale * 8}, {-scale * 8, scale * 8}, {scale * 8, scale * 8}, {scale * 8, -scale * 8}}};
        polygon(point, square, 0u - map.position.rotation, kind == 8);
      } else if(kind == 9) {
        auto const p{view.project(previous)}; font.character(target, static_cast<uint8_t>(command.x), p.x - 2, p.y - 3, colour);
      } else if(kind == 12 && i + 1 < commands.size() && commands[i + 1].kind == 13) {
        std::array<render::vertex2, 4> const shape{{{-0xf00, -0x1e00}, {-0xf00, 0x1e00}, {0xf00, 0x1e00}, {0xf00, -0x1e00}}};
        polygon(point, shape, 0u - (static_cast<uint32_t>(commands[i + 1].x) + map.position.rotation), false);
      } else throw std::runtime_error{"Invalid briefing route drawing command"};
    }
    if(kind >= 2 && kind <= 8) previous = point;
  }
}
} // namespace apache::frontend
