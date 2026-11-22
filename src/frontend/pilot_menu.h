#pragma once
#include "configuration/profiles.h"
#include "menu.h"

namespace apache::frontend {
class pilot_menu {
  menu &interface;
  configuration::profiles &profiles;
  std::span<resources::mission_briefing const> missions;
  std::array<std::string, 7> ranks;
  std::array<std::string, 10> statuses;
  unsigned int editing{0};
  void visible(std::string_view name, bool show);
  void error(std::string text);
  void rename(unsigned int kind, std::string_view title);
public:
  pilot_menu(resources::data_store const &data, menu &interface, configuration::profiles &profiles, std::span<resources::mission_briefing const> missions);
  void update();
  void show();
  bool activate(widget &object, int line);
  bool key(unsigned int character);
  bool can_exit();
};
} // namespace apache::frontend
