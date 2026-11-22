#pragma once
#include <array>
#include "menu.h"

namespace apache::frontend {
class help_menu {
  menu &interface;
  std::array<bool, 10000> available{};
  int current_page{203};
  void move(int direction);
public:
  help_menu(resources::data_store const &data, menu &interface);
  int page() const {
    return current_page;
  }
  void select(int page);
  void generic(int page);
  bool activate(widget const &object);
};
} // namespace apache::frontend
