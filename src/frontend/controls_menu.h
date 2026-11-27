#pragma once
#include "configuration/settings.h"
#include "input/controllers.h"
#include "menu.h"

namespace apache::frontend {
class controls_menu {
  menu &interface;
  configuration::settings &settings;
  render::menu_font font;
  std::vector<input::device_state> devices;
  input::binding_capture learner;
  bool buttons_tab{false};
  size_t selection{0};
  int learning{0}; // 1: axis, 2/3: negative/positive half, 4: action.
  bool calibrating{false};
  double minimum{1}, maximum{-1};
  bool renaming{false};
  std::string edited_name;
  std::string message;
  void refresh();
  void changed();
  void capture(int mode);
  void presets();
  void finish_calibration();
  std::string fit(std::string text, int width) const;
public:
  controls_menu(resources::data_store const &data, menu &interface, configuration::settings &settings);
  void poll(std::span<input::device_state const> samples, bool focused);
  void paint(render::framebuffer &frame, widget const &object);
  bool activate(widget &object, int line);
  bool text_input(unsigned int value);
  bool cancel();
  void show();
};
} // namespace apache::frontend
