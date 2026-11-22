#include "map_editor.h"
#include <algorithm>
#include <cstdlib>

namespace apache::frontend {
namespace {
int32_t multiply(int32_t a, int32_t b) {
  return std::bit_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b));
}
}
std::vector<uint8_t> waypoint_letters(std::span<resources::route_waypoint const> const route) {
  std::vector<uint8_t> out; out.reserve(route.size()); uint8_t normal{'A'}, attack{'X'};
  for(auto const &point : route) {
    uint8_t letter{0};
    if(point.type == 4 || point.type == 6) { letter = attack++; if(attack == '[') attack = 'x'; } else if(point.type >= 0 && point.type <= 8 && point.type != 5) { letter = normal++; if(normal == 'T') normal = 'a'; }
    out.push_back(letter);
  }
  return out;
}
void map_editor::attach(resources::mission_task &source, size_t const index) {
  task = &source; group = index; selected = actor() && !actor()->waypoints.empty() ? 0 : -1;
  start_x = start_y = -1; modified = false; recalculate();
}
resources::mission_actor *map_editor::actor() {
  return task && group < task->actors.size() ? &task->actors[group] : nullptr;
}
resources::mission_actor const *map_editor::actor() const {
  return task && group < task->actors.size() ? &task->actors[group] : nullptr;
}
resources::route_waypoint *map_editor::point() {
  auto *a{actor()};
  return a && selected >= 0 && static_cast<size_t>(selected) < a->waypoints.size() ? &a->waypoints[static_cast<size_t>(selected)] : nullptr;
}
resources::route_waypoint const *map_editor::point() const {
  auto const *a{actor()};
  return a && selected >= 0 && static_cast<size_t>(selected) < a->waypoints.size() ? &a->waypoints[static_cast<size_t>(selected)] : nullptr;
}
bool map_editor::fixed() const {
  return !point() || selected == 0 || static_cast<size_t>(selected + 1) == actor()->waypoints.size();
}
void map_editor::select(size_t const number) {
  if(actor() && number < actor()->waypoints.size()) selected = static_cast<int>(number);
}
void map_editor::recalculate() {
  if(!task) return;
  for(auto &a : task->actors) for(size_t i{0}; i < a.waypoints.size(); ++i) {
    auto &p{a.waypoints[i]};
    if(!i) { p.time = p.distance = p.interval = 0; continue; }
    auto const &previous{a.waypoints[i - 1]}; uint64_t squared{0};
    for(size_t axis{0}; axis != 3; ++axis) { int64_t const d{math::subtract(previous.position[axis], p.position[axis])}; squared += static_cast<uint64_t>(d * d); }
    p.distance = std::bit_cast<int32_t>(math::integer_sqrt(squared));
    if(p.speed < 1) p.time = p.interval = -1;
    else { p.interval = multiply(p.distance / p.speed, 200); p.time = math::add(previous.time, p.interval); }
  }
}
void map_editor::click(int32_t x, int32_t y, map_view const &view, math::trigonometry const &trig) {
  if(!task) return;
  x = std::clamp(x, 0, 0xfffffff); y = std::clamp(y, 0, 0xfffffff);
  auto const projection{view.projection(trig)}; auto const cursor{projection.project({x, y})};
  auto const hit{[&](size_t g){
    auto const &route{task->actors[g].waypoints};
    for(size_t i{0}; i < route.size(); ++i) {
      auto const p{projection.project({route[i].position[0], route[i].position[1]})};
      if(std::abs(int64_t{p.x} - cursor.x) < 12 && std::abs(int64_t{p.y} - cursor.y) < 12) { group = g; selected = static_cast<int>(i); start_x = x; start_y = y; return true; }
    }
    return false;
  }};
  if(actor() && hit(group)) return;
  for(size_t g{0}; g != task->actors.size(); ++g) if(g != group && hit(g)) return;
  if(!point()) return;
  auto &route{actor()->waypoints};
  if(route.size() < 39) route.push_back({.type{2}, .speed{route.back().speed}});
  selected = static_cast<int>(route.size() - 1);
  point()->position[0] = x; point()->position[1] = y;
  start_x = start_y = -1; modified = true; recalculate();
}
void map_editor::drag(int32_t x, int32_t y, int32_t const zoom) {
  if(fixed()) return;
  x = std::clamp(x, 0, 0xfffffff); y = std::clamp(y, 0, 0xfffffff);
  if(start_x != -1 && std::abs(int64_t{x} - start_x) <= int64_t{zoom} * 4 && std::abs(int64_t{y} - start_y) <= int64_t{zoom} * 4) return;
  start_x = start_y = -1; point()->position[0] = x; point()->position[1] = y; modified = true; recalculate();
}
void map_editor::insert() {
  if(fixed() || actor()->waypoints.size() >= 40) return;
  auto &route{actor()->waypoints}; auto const i{static_cast<size_t>(selected)};
  resources::route_waypoint p{.type{2}, .speed{route[i - 1].speed}};
  for(size_t axis{0}; axis != 2; ++axis) p.position[axis] = math::add(route[i - 1].position[axis], route[i].position[axis]) / 2;
  route.insert(route.begin() + selected, p); modified = true; recalculate();
}
void map_editor::erase() {
  if(fixed()) return;
  auto &route{actor()->waypoints}; route.erase(route.begin() + selected); modified = true; recalculate();
}
int map_editor::display_type() const {
  auto const *p{point()}; if(!p) return 0; int type{p->type};
  if(type == 2) {
    auto const &route{actor()->waypoints}; auto const i{static_cast<size_t>(selected)};
    if(i && route[i - 1].type == 3) type = 10;
    if(i + 1 == route.size()) type = 7;
    else { if(route[i + 1].type == 4) type = 9; if(i + 2 == route.size()) type = 8; }
  }
  return type;
}
void map_editor::cycle_type() {
  if(!point() || display_type() == 1) return;
  switch(display_type()) { case 3: point()->type = 2; break; case 4: point()->type = 6; break; case 6: point()->type = 3; break; default: point()->type = 4; break; }
  modified = true;
}
void map_editor::position(int32_t const x, int32_t const y, int32_t const z) {
  if(!fixed()) { point()->position = {world(x), world(y), multiply(z, 256)}; modified = true; recalculate(); }
}
void map_editor::speed(int32_t const knots) {
  if(point()) { point()->speed = multiply(knots, 432); modified = true; recalculate(); }
}
void map_editor::time(int32_t const seconds) {
  if(actor()) { actor()->start_time = multiply(seconds, 200); modified = true; }
}
int32_t map_editor::grid(int32_t const world) {
  return static_cast<int32_t>((int64_t{world} + 128) * 1000 / 0xcd0d7);
}
int32_t map_editor::world(int32_t const grid) {
  return static_cast<int32_t>(int64_t{grid} * 0xcd0d7 / 1000);
}
} // namespace apache::frontend
