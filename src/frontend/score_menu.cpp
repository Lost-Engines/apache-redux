#include "score_menu.h"
#include <algorithm>
#include <stdexcept>

namespace apache::frontend {
score_menu::score_menu(resources::data_store const &data, menu &interface, configuration::settings &settings)
  : interface{interface}, settings{settings}, font{resources::read_iff(data.read("FONTS/11X15.LBM"))} {
  size_t n{0}, s{0};
  auto const widgets{interface.objects()};
  for(size_t i{0}; i != widgets.size(); ++i) {
    if(widgets[i].name == "List of Top Ten names") names.at(n++) = i;
    if(widgets[i].name == "List of Top Ten scores") numbers.at(s++) = i;
  }
  if(n != 10 || s != 10) throw std::runtime_error{"Incomplete score table widgets"};
  load();
}
void score_menu::load() {
  for(size_t i{0}; i != entries.size(); ++i) {
    auto const suffix{std::to_string(i + 1)};
    entries[i] = {settings.values.integer("Quick Start Score " + suffix).value_or(0), std::string{settings.values.get("Quick Start Name " + suffix).value_or("Blank").substr(0, 30)}};
  }
  last = -1; editing = false;
}
void score_menu::save() {
  for(size_t i{0}; i != entries.size(); ++i) {
    auto const suffix{std::to_string(i + 1)};
    settings.values.set("Quick Start Score " + suffix, entries[i].score);
    settings.values.set("Quick Start Name " + suffix, entries[i].name);
  }
  settings.save();
}
void score_menu::update() {
  auto const digit{font.width("0")};
  for(size_t i{0}; i != entries.size(); ++i) {
    auto &name{interface.objects()[names[i]]}; auto &number{interface.objects()[numbers[i]]};
    name.text = entries[i].name; name.selected_line = static_cast<int>(i) == last ? 0 : -1;
    number.text = std::to_string(entries[i].score);
    auto const width{static_cast<int>(number.text.size()) * digit};
    int const x{404 + 10 * digit - width}, y{160 + static_cast<int>(i) * 20};
    number.area = {x, y, x + width + 3, y + 20};
  }
  save();
}
void score_menu::record(int32_t const score, std::string_view const pilot) {
  last = -1; editing = false;
  if(score <= 0) return;
  auto const place{std::ranges::find_if(entries, [score](auto const &entry){ return score > entry.score; })};
  if(place == entries.end()) return;
  std::move_backward(place, entries.end() - 1, entries.end());
  *place = {score, std::string{pilot.substr(0, 30)}};
  last = static_cast<int>(place - entries.begin()); editing = true;
}
void score_menu::show() {
  update();
  interface.toggle_quick(false);
  interface.show("_top_ten_screen");
  editing = last >= 0 && static_cast<size_t>(last) < entries.size();
}
bool score_menu::key(unsigned int const character) {
  if(interface.current() != "_top_ten_screen" || !editing || last < 0 || static_cast<size_t>(last) >= entries.size()) return false;
  auto &name{entries[static_cast<size_t>(last)].name};
  if(character == 8) { if(!name.empty()) name.pop_back(); } else if(character == 10 || character == 13) editing = false;
  else if(character == 27) name.clear();
  else if(character >= 32 && character <= 126 && name.size() < 30) name += static_cast<char>(character);
  update(); return true;
}
} // namespace apache::frontend
