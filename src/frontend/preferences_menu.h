#pragma once
#include "menu.h"
#include "render/landscape.h"
#include <memory>
#include "configuration/settings.h"

namespace apache::frontend {
class preferences_menu {
  resources::data_store const &data;
  menu &interface;
  std::unique_ptr<render::landscape> world;
  render::framebuffer preview_frame{290, 156};
  int preview_zone{0};
  bool preview_visible{false};
  void preview();
  configuration::settings &settings;
  void dropdown(widget const &button, std::string const &key, std::vector<std::pair<std::string, int>> choices);
public:
  int zone{2};
  preferences_menu(resources::data_store const &data, menu &interface, configuration::settings &settings) : data{data}, interface{interface}, settings{settings} {}
  std::pair<size_t, size_t> preview_issues() const { return world ? std::pair{world->missing_normals, world->invalid_texture_samples} : std::pair<size_t, size_t>{}; }
  void update();
  void calibration(std::array<int32_t, 4> values, unsigned int hat);
  void paint(render::framebuffer &frame, widget const &object);
  void show();
  bool activate(widget &object);
};
} // namespace apache::frontend
