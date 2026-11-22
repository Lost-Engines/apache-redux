#include "help_menu.h"
#include <algorithm>
#include <charconv>
#include <cstdio>

namespace apache::frontend {
help_menu::help_menu(resources::data_store const &data, menu &interface) : interface{interface} {
  for(auto const &name : data.names()) {
    if(name.size() != 19 || !name.starts_with("MANUAL/PAGE") || !name.ends_with(".LBM")) continue;
    int number{0}; auto const result{std::from_chars(name.data() + 11, name.data() + 15, number)};
    if(result.ec == std::errc{} && result.ptr == name.data() + 15 && number >= 0 && number < 10000) available[static_cast<size_t>(number)] = true;
  }
  select(current_page);
}
void help_menu::select(int const number) {
  current_page = std::clamp(number, 0, 9999);
  if(!available[static_cast<size_t>(current_page)]) return;
  char path[32]; std::snprintf(path, sizeof(path), "MANUAL/PAGE%04d.LBM", current_page);
  interface.named("_help_picture").picture = path;
  for(auto &object : interface.objects()) if(object.action == "notify_interactive_contents_page_selection_") {
    bool const show{current_page == (object.area.left == 210 ? 203 : 204)};
    object.flags = (object.flags & ~1u) | static_cast<unsigned int>(show);
  }
}
void help_menu::generic(int const page) {
  for(int i{page}; i < 9999; i += 100) if(i >= 0 && available[static_cast<size_t>(i)]) { select(i); return; }
}
void help_menu::move(int const direction) {
  int next{current_page};
  if(direction == -2) {
    int const first{current_page / 100 * 100};
    for(int i{first}; i < first + 99; ++i) if(available[static_cast<size_t>(i)] && i < current_page) { select(i); return; }
    next = std::max(0, first - 100);
  } else next = direction == 2 ? (current_page / 100 + 1) * 100 : current_page + direction;
  int const step{direction == -1 ? -1 : 1};
  for(; next >= 0 && next < (direction == -2 ? 9999 : 10000); next += step) if(available[static_cast<size_t>(next)]) { select(next); return; }
}
bool help_menu::activate(widget const &object) {
  if(object.action == "notify_help_contents_") select(203);
  else if(object.action == "notify_help_index_") select(2285);
  else if(object.action == "notify_control_summary_") select(3001);
  else if(object.action == "notify_interactive_contents_page_selection_") generic(object.item);
  else if(object.action == "notify_help_move_") {
    for(auto const &[name, direction] : std::array<std::pair<std::string_view, int>, 4>{{{"_help_back_one_chapter_button", -2}, {"_help_back_one_page_button", -1}, {"_help_forward_one_page_button", 1}, {"_help_forward_one_chapter_button", 2}}}) {
      if(&object == &interface.named(name)) move(direction);
    }
  } else if(object.action == "notify_help_exit_button_") interface.back();
  else return false;
  return true;
}
} // namespace apache::frontend
