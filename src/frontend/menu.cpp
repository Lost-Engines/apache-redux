#include "menu.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace apache::frontend {
menu::menu(resources::data_store const &data) : data{data}, font{resources::read_iff(data.read("FONTS/11X15.LBM"))}, number_font{resources::executable_data{data}, true} {
  for(size_t i{0}; i != widgets.size(); ++i) for(auto const &name : widgets[i].globals) globals[name] = i;
  clips.resize(widgets.size());
  show("_main_screen");
}
widget &menu::named(std::string_view const global) {
  auto const found{globals.find(global)};
  if(found == globals.end()) throw std::runtime_error{"Unknown menu widget: " + std::string{global}};
  return widgets[found->second];
}
std::string_view menu::current() const {
  return screens.empty() ? std::string_view{} : widgets[screens.back()].globals.front();
}
resources::indexed_image const &menu::image(std::string const &name) {
  auto found{images.find(name)};
  if(found == images.end()) found = images.emplace(name, resources::read_iff(data.read(name))).first;
  return found->second;
}
void menu::link_scroll(std::string_view const text, std::string_view const scrollbar) {
  scroll_links.emplace_back(globals.at(std::string{text}), globals.at(std::string{scrollbar}));
}
void menu::scroll_changed(size_t const index) {
  for(auto const &[text, bar] : scroll_links) if(bar == index) widgets[text].start_line = static_cast<unsigned int>(std::max(0, widgets[bar].range[2]));
}
void menu::bounds(std::string_view const global, render::rectangle const area) {
  auto const index{globals.at(std::string{global})};
  bounds(index, area);
}
void menu::bounds(size_t const index, render::rectangle const area) {
  auto const old{widgets[index].area};
  int const dx{area.left - old.left}, dy{area.top - old.top};
  for(size_t i{index + 1}; i < widgets.size(); ++i) {
    int parent{widgets[i].parent};
    while(parent >= 0 && static_cast<size_t>(parent) != index) parent = widgets[static_cast<size_t>(parent)].parent;
    if(parent >= 0) { auto &r{widgets[i].area}; r.left += dx; r.right += dx; r.top += dy; r.bottom += dy; }
  }
  widgets[index].area = area;
  for(size_t i{index + 1}; i < widgets.size(); ++i) if(widgets[i].parent == static_cast<int>(index) && widgets[i].type == 11) {
    auto &r{widgets[i].area}; int const width{r.right - r.left}, height{r.bottom - r.top};
    r = {area.right - width, area.bottom - height, area.right, area.bottom};
  }
}
void menu::raise(std::string_view const global) {
  auto const index{globals.at(std::string{global})}; std::erase(raised, index); raised.push_back(index);
}
void menu::show(std::string_view const global) {
  active_popup.reset();
  auto const found{globals.find(global)};
  if(found == globals.end()) throw std::runtime_error{"Unknown menu screen: " + std::string{global}};
  auto const index{found->second};
  auto const stacked{std::ranges::find(screens, index)};
  if(stacked != screens.end()) screens.erase(stacked, screens.end());
  screens.push_back(index);
  for(auto &object : widgets) if(object.parent == 0) object.flags &= ~1u;
  widgets[index].flags |= 1;
  hovered = pressed = pointer_capture = moving_box = resizing_widget = number_capture = -1;
}
void menu::back() {
  if(dismiss_popup()) return;
  if(!screens.empty() && screens.back() == globals.at("_main_screen") && (widgets[globals.at("_quick_start_options")].flags & 1)) {
    toggle_quick(false);
    return;
  }
  if(screens.size() <= 1) {
    show("_exit_screen");
    return;
  }
  screens.pop_back();
  auto const index{screens.back()};
  for(auto &object : widgets) if(object.parent == 0) object.flags &= ~1u;
  widgets[index].flags |= 1;
  hovered = pressed = pointer_capture = moving_box = resizing_widget = number_capture = -1;
}
void menu::popup(std::vector<std::pair<std::string, int>> choices, std::function<void(int)> selected) {
  if(choices.empty()) return;
  int width{0}; for(auto const &choice : choices) width = std::max(width, font.width(choice.first) + 8);
  int const height{static_cast<int>(choices.size()) * 19 + 3};
  int const x{std::clamp(cursor_x - width / 2, 0, std::max(0, 639 - width))};
  int const y{std::clamp(cursor_y - height / 2, 0, std::max(0, 479 - height))};
  active_popup = popup_state{std::move(choices), std::move(selected), {x, y, x + width, y + height}};
  move(cursor_x, cursor_y);
}
bool menu::dismiss_popup() {
  if(!active_popup) return false;
  active_popup.reset();
  return true;
}
void menu::toggle_quick(bool const visible) {
  auto &object{widgets.at(globals.at("_quick_start_options"))};
  object.flags = (object.flags & ~1u) | static_cast<unsigned int>(visible);
}
void menu::press_named(std::string_view const name) {
  for(auto &object : widgets) if(object.type == 9 || object.type == 10) object.on = object.name == name;
}
void menu::draw_widget(render::framebuffer &target, widget const &object) {
  auto const &r{object.area};
  bool const button{object.type == 3 || object.type == 9 || object.type == 10};
  bool const clear{(object.flags & 0x4000) != 0};
  if(object.type == 16 || (button && !object.picture.empty() && object.on)) {
    if(!object.picture.empty()) {
      auto const &picture{image(object.picture)};
      target.blit(picture, r.left, r.top);
      if(object.type == 16) {
        unsigned int start{0}, count{246};
        switch(object.palette_part) {
        case 1: start = 168; count = 78; break;
        case 2: count = 64; break;
        case 3: start = 64; count = 182; break;
        case 4: count = 0; break;
        case 5: count = 192; break;
        case 6: count = 112; break;
        case 7: start = 112; count = 134; break;
        default: break;
        }
        std::copy_n(picture.colours.begin() + start, count, target.image.colours.begin() + start);
      }
    } else target.fill(r, 0);
  }
  if(object.type == 5) target.fill(r, 0);
  if(object.type == 0 && !clear) target.fill(r, object.colour);
  if(object.type == 2 && !clear) {
    target.fill(r, object.colour);
    target.surround(r, object.on);
  }
  if(button) {
    if(object.picture.empty()) {
      if(!clear) target.fill(r, object.type == 9 ? (object.on ? 247 : 249) : object.colour);
      if(!(object.flags & 0x400000)) target.surround(r, object.on);
    }
    if(!object.text.empty()) {
      int const x{(r.left + r.right) / 2 - font.width(object.text) / 2};
      int const y{(r.top + r.bottom) / 2 - (static_cast<int>(object.font_height + 1) / 2 - 1) - (object.picture.empty() ? 0 : 3)};
      if(object.shadow) font.draw(target, object.text, r.left + 1, r.top + 1, 0, static_cast<int>(object.font_height + 1));
      font.draw(target, object.text, object.shadow ? r.left : x, object.shadow ? r.top : y, object.text_colour, static_cast<int>(object.font_height + 1));
    }
    if(object.flags & 0x10000) {
      for(int row{0}; row != 4; ++row) {
        target.fill({r.right - 9 - row, r.top + 3 + row, r.right - 9 + row, r.top + 3 + row}, object.text_colour);
        target.fill({r.right - 12 + row, r.top + 9 + row, r.right - 6 - row, r.top + 9 + row}, object.text_colour);
      }
    }
  }
  if(object.type == 12 && object.font_height) {
    int const padding{(object.flags & 0x20000) ? 3 : 0};
    auto const lines{font.lines(object.text, r.right - r.left - padding, (object.flags & 0x100000) != 0)};
    int y{r.top};
    for(size_t i{object.start_line}; i < lines.size() && y <= r.bottom; ++i, y += static_cast<int>(object.font_height + 1)) {
      if((object.flags & 0x10) && static_cast<int>(i) == object.selected_line) target.fill({r.left, y, r.right, y + static_cast<int>(object.font_height) - 4}, 248);
      if(object.shadow) font.draw(target, lines[i], r.left + padding + 1, y + 1, 0);
      font.draw(target, lines[i], r.left + padding, y, object.text_colour);
    }
  }
  if(object.type == 15 && object.number) {
    target.fill(r, object.colour); target.surround(r, object.on);
    if(object.on) {
      if(number_click != 0) target.surround({r.left, r.top, r.right, r.top + 9});
      if(number_click != 1) target.surround({r.left, r.bottom - 9, r.right, r.bottom});
    }
    auto const code{static_cast<uint8_t>('0' + (*object.number / object.number_divisor) % 10)};
    number_font.character(target, code, r.left + 3, r.top + (object.on ? 12 : 3), 251);
  }
  if(object.type == 13) {
    if(!clear) target.fill(r, 249);
    target.surround(r);
    int const length{r.bottom - r.top - 4};
    int const denominator{std::max(1, object.range[1] - object.range[0] + 1)};
    int top{r.top + 2 + (object.range[2] - object.range[0]) * length / denominator};
    int bottom{top + std::max(12, object.range[3] * length / denominator)};
    if(bottom > r.bottom - 2) { bottom = r.bottom - 2; if(top + 12 > bottom) top = r.bottom - 14; }
    target.surround({r.left + 2, top, r.right - 2, bottom}, object.on);
  }
  if(object.type == 14) {
    if(!clear) target.fill(r, 249);
    target.surround(r);
    int const length{r.right - r.left - 4};
    int const denominator{std::max(1, object.range[1] - object.range[0] + 1)};
    int left{r.left + 2 + (object.range[2] - object.range[0]) * length / denominator};
    int right{left + std::max(12, object.range[3] * length / denominator)};
    if(right > r.right - 2) {
      right = r.right - 2;
      if(left + 12 > right) left = r.right - 14;
    }
    for(int i{1}; i != 10; ++i) {
      int const x{r.left + 2 + length / 10 * i};
      target.point(x, (r.top + r.bottom) / 2, 251);
      target.point(x, (r.top + r.bottom) / 2 + 1, 251);
    }
    target.surround({left, r.top + 2, right, r.bottom - 2}, object.on);
  }
  if(object.type == 4 || object.type == 11) {
    std::string const name{object.type == 4 ? (object.on ? "UIDATA/CLOSEON.BM" : "UIDATA/CLOSEOFF.BM") : (object.on ? "UIDATA/RESIZING.BM" : "UIDATA/RESIZE.BM")};
    auto found{sprites.find(name)};
    if(found == sprites.end()) {
      auto raw{data.read(name)};
      if(raw.size() != 256) throw std::runtime_error{"Invalid window control sprite"};
      for(auto &pixel : raw) pixel = static_cast<uint8_t>(pixel + 4);
      found = sprites.emplace(name, resources::indexed_image{16, 16, std::move(raw), {}}).first;
    }
    target.blit(found->second, r.left, r.top, true);
  }
  if(object.type == 17 && !object.sprite.empty()) {
    auto found{sprites.find(object.sprite)};
    if(found == sprites.end()) {
      auto const &source{image(object.sprite)};
      auto const &g{object.sprite_geometry};
      render::framebuffer sprite{static_cast<unsigned int>(g[0]), static_cast<unsigned int>(g[1])};
      sprite.clip = {0, 0, g[4] - 1, g[5] - 1};
      sprite.blit(source, 0, 0);
      found = sprites.emplace(object.sprite, std::move(sprite.image)).first;
    }
    target.blit(found->second, r.left - object.sprite_geometry[2], r.top - object.sprite_geometry[3], true);
  }
}
void menu::render(render::framebuffer &target) {
  unsigned int const width{current() == "_help_screen" ? 800u : 640u}, height{width == 800 ? 600u : 480u};
  if(target.image.width != width || target.image.height != height) target = render::framebuffer{width, height};
  widgets.front().area = {0, 0, static_cast<int>(width) - 1, static_cast<int>(height) - 1};
  for(auto const &[text, bar] : scroll_links) {
    auto const &object{widgets[text]};
    auto const count{font.lines(object.text, object.area.right - object.area.left - ((object.flags & 0x20000) ? 3 : 0), (object.flags & 0x100000) != 0).size()};
    widgets[bar].range = {0, std::max(0, static_cast<int>(count) - 1), static_cast<int>(object.start_line), (object.area.bottom - object.area.top) / static_cast<int>(object.font_height + 1)};
  }
  target.reset_clip();
  auto const screen_clip{target.clip};
  target.clear();
  paint_order.clear();
  auto const raised_ancestor{[&](size_t i){
    int parent{static_cast<int>(i)};
    while(parent >= 0) { if(std::ranges::find(raised, static_cast<size_t>(parent)) != raised.end()) return parent; parent = widgets[static_cast<size_t>(parent)].parent; }
    return -1;
  }};
  for(size_t i{0}; i != widgets.size(); ++i) if(raised_ancestor(i) < 0) paint_order.push_back(i);
  for(auto top : raised) for(size_t i{0}; i != widgets.size(); ++i) if(raised_ancestor(i) == static_cast<int>(top)) paint_order.push_back(i);
  std::ranges::fill(clips, render::rectangle{});
  for(auto i : paint_order) {
    auto const &object{widgets[i]};
    if(!(object.flags & 1)) continue;
    auto const parent_clip{object.parent < 0 ? screen_clip : clips[static_cast<size_t>(object.parent)]};
    clips[i] = render::intersect(parent_clip, object.area);
    if(clips[i].left > clips[i].right || clips[i].top > clips[i].bottom) continue;
    target.clip = clips[i];
    draw_widget(target, object);
    if(paint) paint(target, object);
  }
  if(finish) finish(target);
  // Original UI palette, shared by every menu and independent of the backdrop.
  resources::palette &palette{target.image.colours};
  std::array<resources::rgb, 7> constexpr colours{{{66, 97, 100}, {76, 112, 116}, {135, 169, 171}, {102, 151, 154}, {178, 203, 204}, {255, 255, 255}, {0, 127, 127}}};
  std::ranges::copy(colours, palette.begin() + 246);
  target.reset_clip();
  if(active_popup) {
    auto const &popup{*active_popup}; auto const &r{popup.area};
    target.fill(r, 249); target.surround(r);
    for(size_t i{0}; i != popup.choices.size(); ++i) {
      int const y{r.top + 2 + static_cast<int>(i) * 19};
      if(static_cast<int>(i) == popup.hovered) target.surround({r.left + 2, y, r.right - 2, y + 18});
      font.draw(target, popup.choices[i].first, r.left + 4, y + 2, 251);
    }
  }
}
int menu::hit(int const x, int const y) const {
  for(auto it{paint_order.rbegin()}; it != paint_order.rend(); ++it) if((widgets[*it].flags & 0x8000) && clips[*it].contains(x, y)) return static_cast<int>(*it);
  return -1;
}
void menu::move(int const x, int const y) {
  cursor_x = x; cursor_y = y;
  if(active_popup) {
    auto &p{*active_popup};
    p.hovered = p.area.contains(x, y) && y >= p.area.top + 2 && y < p.area.bottom - 1 ? (y - p.area.top - 2) / 19 : -1;
    return;
  }
  if(number_capture >= 0) {
    auto const &object{widgets[static_cast<size_t>(number_capture)]};
    if(hit(x, y) != number_capture) number_click = 3;
    else if(y < object.area.top + 9) number_click = 0;
    else if(y > object.area.bottom - 9) number_click = 1;
    else if(number_click != 2) number_click = 3;
    return;
  }
  if(resizing_widget >= 0) {
    auto const parent{widgets[static_cast<size_t>(resizing_widget)].parent};
    auto const area{widgets[static_cast<size_t>(parent)].area};
    int const right{std::max(area.left + 64, std::clamp(x, 0, 639) + resize_dx)}, bottom{std::max(area.top + 64, std::clamp(y, 0, 479) + resize_dy)};
    bounds(static_cast<size_t>(parent), {area.left, area.top, right, bottom});
    return;
  }
  if(moving_box >= 0) {
    auto const index{static_cast<size_t>(moving_box)}; auto const &box{widgets[index]};
    auto const parent{box.parent >= 0 ? widgets[static_cast<size_t>(box.parent)].area : render::rectangle{0, 0, 639, 479}};
    int const left{std::clamp(x, parent.left, parent.right) - moving_x}, top{std::clamp(y, parent.top, parent.bottom) - moving_y};
    bounds(index, {left, top, left + box.area.right - box.area.left, top + box.area.bottom - box.area.top});
    return;
  }
  hovered = hit(x, y);
  int const recipient{pointer_capture >= 0 ? pointer_capture : hovered};
  if(recipient >= 0 && pointer && pointer(widgets[static_cast<size_t>(recipient)], pointer_action::move, x, y)) return;
  if(pressed >= 0 && widgets[static_cast<size_t>(pressed)].type == 14 && scroll_direction == 0) {
    auto &bar{widgets[static_cast<size_t>(pressed)]};
    bar.range[2] = std::clamp(drag_position + x - drag_y, bar.range[0], bar.range[1]);
    if(activated) activated(bar, bar.range[2]);
  }
  if(pressed >= 0 && widgets[static_cast<size_t>(pressed)].type == 13 && scroll_direction == 0) {
    auto &bar{widgets[static_cast<size_t>(pressed)]};
    bar.range[2] = std::clamp(drag_position + (y - drag_y) * (bar.range[1] - bar.range[0] + 1) / std::max(1, bar.area.bottom - bar.area.top - 4), bar.range[0], bar.range[1]);
    scroll_changed(static_cast<size_t>(pressed));
  }
  hovered = hit(x, y);
  for(size_t i{0}; i != widgets.size(); ++i) if(widgets[i].type == 9 && !widgets[i].picture.empty()) widgets[i].on = static_cast<int>(i) == hovered;
}
void menu::button(bool const down, unsigned int const button) {
  if(button > 1) return;
  if(active_popup) {
    if(!down) { auto selected{std::move(active_popup->selected)}; auto const line{active_popup->hovered}; auto choices{std::move(active_popup->choices)}; active_popup.reset(); if(line >= 0 && static_cast<size_t>(line) < choices.size()) selected(choices[static_cast<size_t>(line)].second); }
    return;
  }
  if(number_capture >= 0) {
    if(!down && !button) {
      auto const index{static_cast<size_t>(number_capture)}; auto &object{widgets[index]};
      number_capture = -1;
      if(number_moved && number_changed) number_changed(object, true);
      if(hit(cursor_x, cursor_y) != static_cast<int>(index) || number_click == 3) number_push(index, false);
      number_click = 3;
    }
    return;
  }
  if(hovered >= 0 && widgets[static_cast<size_t>(hovered)].type == 15 && widgets[static_cast<size_t>(hovered)].number) {
    if(!down) return;
    auto const index{static_cast<size_t>(hovered)}; auto &object{widgets[index]};
    if(button) {
      number_push(index, false); number_click = 3;
      std::vector<std::pair<std::string, int>> choices; for(int i{0}; i != 10; ++i) choices.emplace_back(std::to_string(i), i);
      popup(std::move(choices), [this, index](int digit){
        auto &o{widgets[index]}; auto const value{*o.number};
        *o.number = std::clamp(value - ((value / o.number_divisor) % 10) * o.number_divisor + digit * o.number_divisor, o.number_min, o.number_max);
        if(number_changed) { number_changed(o, false); number_changed(o, true); }
      });
    } else {
      number_capture = hovered; number_repeat = static_cast<int64_t>(number_clock) + 100; number_moved = false;
      if(!object.on) number_click = 2;
      else if(cursor_y < object.area.top + 9) { number_click = 0; number_adjust(index, object.number_divisor); } else if(cursor_y > object.area.bottom - 9) { number_click = 1; number_adjust(index, -object.number_divisor); } else number_click = 1;
      number_push(index, true);
    }
    return;
  }
  if(resizing_widget >= 0) { if(!down && !button) { widgets[static_cast<size_t>(resizing_widget)].on = false; resizing_widget = -1; } return; }
  if(moving_box >= 0) { if(!down && !button) moving_box = -1; return; }
  int const recipient{pointer_capture >= 0 ? pointer_capture : hovered};
  if(recipient >= 0 && pointer && pointer(widgets[static_cast<size_t>(recipient)], button ? (down ? pointer_action::right_down : pointer_action::right_up) : (down ? pointer_action::left_down : pointer_action::left_up), cursor_x, cursor_y)) {
    if(!button) pointer_capture = down ? recipient : -1;
    return;
  }
  if(button) return;
  if(down) {
    pressed = hovered;
    if(pressed >= 0) {
      auto const &box{widgets[static_cast<size_t>(pressed)]};
      if(box.type == 2 && (box.flags & 8)) { moving_box = pressed; moving_x = cursor_x - box.area.left; moving_y = cursor_y - box.area.top; if(!box.globals.empty()) raise(box.globals.front()); pressed = -1; return; }
      if(box.type == 11 && box.parent >= 0) {
        resizing_widget = pressed; resize_dx = box.area.right - cursor_x; resize_dy = box.area.bottom - cursor_y;
        widgets[static_cast<size_t>(pressed)].on = true; auto const &parent{widgets[static_cast<size_t>(box.parent)]}; if(!parent.globals.empty()) raise(parent.globals.front()); pressed = -1; return;
      }
      if(box.type == 4 || box.type == 11) widgets[static_cast<size_t>(pressed)].on = true;
    }
    if(pressed >= 0 && widgets[static_cast<size_t>(pressed)].type == 9) widgets[static_cast<size_t>(pressed)].on = true;
    if(pressed >= 0 && (widgets[static_cast<size_t>(pressed)].type == 13 || widgets[static_cast<size_t>(pressed)].type == 14)) {
      auto &bar{widgets[static_cast<size_t>(pressed)]}; auto const &r{bar.area};
      bool const horizontal{bar.type == 14};
      int const start{horizontal ? r.left : r.top}, end{horizontal ? r.right : r.bottom}, cursor{horizontal ? cursor_x : cursor_y};
      int const length{end - start - 4}, denominator{std::max(1, bar.range[1] - bar.range[0] + 1)};
      int top{start + 2 + (bar.range[2] - bar.range[0]) * length / denominator};
      int bottom{top + std::max(12, bar.range[3] * length / denominator)};
      if(bottom > end - 2) { bottom = end - 2; if(top + 12 > bottom) top = end - 14; }
      scroll_direction = cursor < top ? -1 : cursor > bottom ? 1 : 0;
      drag_y = cursor; drag_position = bar.range[2]; bar.on = scroll_direction == 0;
    }
  } else {
    if(pressed >= 0 && (widgets[static_cast<size_t>(pressed)].type == 13 || widgets[static_cast<size_t>(pressed)].type == 14)) {
      auto &bar{widgets[static_cast<size_t>(pressed)]};
      if(pressed == hovered && scroll_direction) bar.range[2] = std::clamp(bar.range[2] + scroll_direction * bar.range[3], bar.range[0], bar.range[1]);
      bar.on = false; scroll_changed(static_cast<size_t>(pressed));
      if(bar.type == 14 && activated) activated(bar, bar.range[2]);
    } else if(pressed >= 0) {
      auto &object{widgets[static_cast<size_t>(pressed)]};
      if((object.type == 9 && object.picture.empty()) || object.type == 4 || object.type == 11) object.on = false;
      if(pressed == hovered) activate(static_cast<size_t>(pressed));
    }
    pressed = -1;
  }
}
void menu::number_push(size_t const index, bool const on) {
  for(size_t i{0}; i != widgets.size(); ++i) {
    auto &object{widgets[i]}; if(object.type != 15 || object.parent != widgets[index].parent) continue;
    bool const active{on && i == index}; if(active == object.on) continue;
    object.on = active; object.area.top += active ? -9 : 9; object.area.bottom += active ? 9 : -9;
  }
}
void menu::number_adjust(size_t const index, int const delta) {
  auto &object{widgets[index]};
  *object.number = static_cast<int>(std::clamp(int64_t{*object.number} + delta, int64_t{object.number_min}, int64_t{object.number_max}));
  number_moved = true; if(number_changed) number_changed(object, false);
}
void menu::tick(double const seconds) {
  number_clock += std::clamp(seconds, 0.0, 0.5) * 200;
  if(number_capture < 0 || number_repeat >= static_cast<int64_t>(number_clock)) return;
  number_repeat = static_cast<int64_t>(number_clock) + 20;
  auto const index{static_cast<size_t>(number_capture)};
  if(number_click == 0 || number_click == 1) number_adjust(index, widgets[index].number_divisor * (number_click ? -1 : 1));
}
void menu::activate(size_t const index) {
  auto &object{widgets[index]};
  if(object.type == 4 && object.parent >= 0) { widgets[static_cast<size_t>(object.parent)].flags &= ~1u; return; }
  int const line{(cursor_y - object.area.top) / static_cast<int>(object.font_height + 1) + static_cast<int>(object.start_line)};
  if(activated && activated(object, line)) return;
  auto const action{object.action};
  if(action == "notify_quit_game_button_") quitting = true;
  else if(action == "notify_cancel_quit_game_" || action == "notify_exit_button_" || action == "notify_preferences_exit_") back();
  else if(action == "notify_quick_start_button_") toggle_quick(true);
  else if(action == "notify_flight_button_") show("_flight_screen");
  else if(action == "notify_preferences_button_") show("_preferences_screen");
  else if(action == "notify_log_button_") show("_log_screen");
  else if(action == "notify_quick_start_topten_button_") show("_top_ten_screen");
  else if(action == "notify_quick_start_arcade_button_" || action == "notify_quick_start_invin_button_") show("_world_screen");
}
bool menu::quit_requested() const {
  return quitting;
}
} // namespace apache::frontend
