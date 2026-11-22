#pragma once
#include "map_view.h"
#include "resources/missions.h"

namespace apache::frontend {
// Editable planning routes. Aircraft and task identities stay in the mission definition.
class map_editor {
  resources::mission_task *task{nullptr};
  int32_t start_x{-1}, start_y{-1};
public:
  size_t group{0};
  int selected{-1};
  bool modified{false};
  void attach(resources::mission_task &source, size_t actor);
  void detach() {
    task = nullptr;
    group = 0;
    selected = -1;
    modified = false;
  }
  resources::mission_actor *actor();
  resources::mission_actor const *actor() const;
  resources::route_waypoint *point();
  resources::route_waypoint const *point() const;
  bool fixed() const;
  void select(size_t number);
  void click(int32_t x, int32_t y, map_view const &view, math::trigonometry const &trig);
  void drag(int32_t x, int32_t y, int32_t zoom);
  void insert();
  void erase();
  void recalculate();
  int display_type() const;
  void cycle_type();
  void position(int32_t x, int32_t y, int32_t z);
  void speed(int32_t knots);
  void time(int32_t seconds);
  static int32_t grid(int32_t world);
  static int32_t world(int32_t grid);
};
std::vector<uint8_t> waypoint_letters(std::span<resources::route_waypoint const> route);
} // namespace apache::frontend
