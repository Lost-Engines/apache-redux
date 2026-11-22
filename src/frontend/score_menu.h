#pragma once
#include "configuration/settings.h"
#include "menu.h"

namespace apache::frontend {
struct score_entry {
  int32_t score{0};
  std::string name{"Blank"};
};
class score_menu {
  menu &interface;
  configuration::settings &settings;
  render::menu_font font;
  std::array<size_t, 10> names{};
  std::array<size_t, 10> numbers{};
  bool editing{false};
  void save();
  void update();
public:
  std::array<score_entry, 10> entries;
  int last{-1};
  score_menu(resources::data_store const &data, menu &interface, configuration::settings &settings);
  void load();
  void record(int32_t score, std::string_view pilot);
  void show();
  void close() {
    editing = false;
    settings.save();
  }
  bool key(unsigned int character);
};
} // namespace apache::frontend
