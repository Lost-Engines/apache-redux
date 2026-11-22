#pragma once
#include "configuration/settings.h"
#include "map_camera.h"
#include "map_editor.h"
#include "map_layers.h"
#include "map_routes.h"
#include "menu.h"
#include "mission_session.h"

namespace apache::frontend {
class briefing_map {
  resources::data_store const &data;
  menu &interface;
  configuration::settings &settings;
  mission_session *mission{nullptr};
  render::instrument_font font;
  render::framebuffer image{640, 480}, overlay{640, 480}, preview_frame{239, 181};
  resources::indexed_image marker;
  std::unique_ptr<render::landscape> preview_world;
  bool preview_dirty{true};
  void preview_layout();
  void preview();
  std::vector<map_route_command> commands;
  bool dirty{true};
  int active_tool{0}, dragging{0}, start_x{0}, start_y{0}, half_width{20}, half_height{15};
  uint32_t start_angle{0};
  render::vertex2 cursor_world;
  void visible(std::string_view name, bool show);
  void show(std::string_view name);
  size_t task_index{0};
  std::array<int, 5> number_values{};
  void route_controls();
  void route_information();
  void routes();
  void fit();
  void refresh();
  void payload();
  void weather();
  uint32_t cursor_angle() const;
public:
  map_view view;
  map_editor editor;
  void number_changed(widget &object, bool final);
  map_camera preview_camera;
  void tick(double seconds);
  std::pair<size_t, size_t> preview_issues() const {
    return preview_world ? std::pair{preview_world->missing_normals, preview_world->invalid_texture_samples} : std::pair<size_t, size_t>{};
  }
  briefing_map(resources::data_store const &data, menu &interface, configuration::settings &settings);
  void open(mission_session &session);
  void close() {
    mission = nullptr;
    dragging = 0;
    preview_world.reset();
    editor.detach();
  }
  void paint(render::framebuffer &target, widget const &object);
  void finish(render::framebuffer &target);
  bool activate(widget &object);
  bool pointer(widget &object, pointer_action action, int x, int y);
};
} // namespace apache::frontend
