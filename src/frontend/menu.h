#pragma once
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "render/framebuffer.h"
#include "render/instrument_font.h"
#include "render/menu_font.h"
#include "resources/data_store.h"
#include "widget.h"

namespace apache::frontend {
enum class pointer_action {
  move,
  left_down,
  left_up,
  right_down,
  right_up
};
class menu {
  resources::data_store const &data;
  render::menu_font font;
  render::instrument_font number_font;
  int number_capture{-1};
  int number_click{3};
  bool number_moved{false};
  double number_clock{0};
  int64_t number_repeat{0};
  void number_push(size_t index, bool on);
  void number_adjust(size_t index, int delta);
  std::vector<widget> widgets{initial_widgets()};
  std::map<std::string, resources::indexed_image, std::less<>> images;
  std::map<std::string, resources::indexed_image, std::less<>> sprites;
  std::map<std::string, size_t, std::less<>> globals;
  std::vector<size_t> screens;
  std::vector<render::rectangle> clips;
  int hovered{-1};
  int pressed{-1};
  int pointer_capture{-1};
  int moving_box{-1};
  int moving_x{0};
  int moving_y{0};
  int resizing_widget{-1};
  int resize_dx{0};
  int resize_dy{0};
  std::vector<size_t> raised;
  std::vector<size_t> paint_order;
  void bounds(size_t index, render::rectangle area);
  int cursor_x{0};
  int cursor_y{0};
  struct popup_state {
    std::vector<std::pair<std::string, int>> choices;
    std::function<void(int)> selected;
    render::rectangle area;
    int hovered{-1};
  };
  std::optional<popup_state> active_popup;
  int drag_y{0};
  int drag_position{0};
  int scroll_direction{0};
  std::vector<std::pair<size_t, size_t>> scroll_links;
  void scroll_changed(size_t index);
  bool quitting{false};
  resources::indexed_image const &image(std::string const &name);
  void draw_widget(render::framebuffer &target, widget const &object);
  void activate(size_t index);
  int hit(int x, int y) const;

public:
  menu(resources::data_store const &data);
  std::function<bool(widget &, int)> activated;
  std::function<void(widget &, bool)> number_changed;
  void tick(double seconds);
  std::function<void(render::framebuffer &, widget const &)> paint;
  std::function<void(render::framebuffer &)> finish;
  std::function<bool(widget &, pointer_action, int, int)> pointer;
  widget &named(std::string_view global);
  std::span<widget> objects() {
    return widgets;
  }
  std::string_view current() const;
  void show(std::string_view global);
  void popup(std::vector<std::pair<std::string, int>> choices, std::function<void(int)> selected);
  bool dismiss_popup();
  void link_scroll(std::string_view text, std::string_view scrollbar);
  void bounds(std::string_view global, render::rectangle area);
  void back();
  void move(int x, int y);
  void button(bool down, unsigned int button = 0);
  void raise(std::string_view global);
  void toggle_quick(bool visible);
  void press_named(std::string_view name);
  void render(render::framebuffer &target);
  bool quit_requested() const;
};
} // namespace apache::frontend
