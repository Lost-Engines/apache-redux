#include "controls_menu.h"
#include <algorithm>
#include <cmath>
#include <format>

namespace apache::frontend {
controls_menu::controls_menu(resources::data_store const &data, menu &interface, configuration::settings &settings)
  : interface{interface}, settings{settings}, font{resources::read_iff(data.read("FONTS/11X15.LBM"))} {
  int const preferences_parent{static_cast<int>(&interface.named("_preferences_controls_area") - interface.objects().data())};
  // The original panel only describes four DOS joystick axes. Replace its
  // children, leaving the rest of Preferences and its navigation intact.
  for(auto &w : interface.objects()) if(w.parent == preferences_parent) w.flags &= ~1u;
  interface.add({.parent{preferences_parent}, .name{}, .globals{"_redux_controls_open"}, .type{9}, .area{342, 263, 608, 291}, .flags{0x48149}, .action{"redux_controls_open"}, .text{"Controls..."}});
  interface.add({.parent{preferences_parent}, .name{}, .globals{"_redux_controls_summary"}, .type{12}, .area{342, 303, 608, 364}, .flags{0x40049}, .text{"Keyboard flight"}});
  int const root{interface.add({.parent{0}, .name{}, .globals{"_controls_screen"}, .type{16}, .area{0, 0, 639, 479}, .flags{0x48048}})};
  auto const label{[&](std::string name, render::rectangle area, std::string text){
    interface.add({.parent{root}, .name{}, .globals{std::move(name)}, .type{12}, .area{area}, .flags{0x40049}, .text{std::move(text)}});
  }};
  auto const button{[&](std::string id, render::rectangle area, std::string text){
    interface.add({.parent{root}, .name{}, .globals{"_controls_" + id}, .type{9}, .area{area}, .flags{0x48149}, .action{"redux_controls_" + id}, .text{std::move(text)}});
  }};
  label("_controls_title", {12, 9, 628, 30}, "CONTROLS");
  button("profile", {12, 36, 312, 59}, "Profile: Default");
  button("mode", {325, 36, 628, 59}, "Keyboard flight");
  button("new", {12, 65, 99, 87}, "Copy");
  button("rename", {107, 65, 193, 87}, "Rename");
  button("delete", {201, 65, 287, 87}, "Delete");
  button("presets", {325, 65, 628, 87}, "Restore defaults...");
  button("axes", {12, 99, 151, 121}, "Flight axes");
  button("buttons", {159, 99, 303, 121}, "Buttons & hats");
  label("_controls_hint", {315, 101, 628, 119}, "Keyboard shortcuts stay available.");
  interface.add({.parent{root}, .name{}, .globals{"_controls_list"}, .type{12}, .area{16, 132, 282, 388}, .flags{0x48059}, .action{"redux_controls_list"}, .text{""}});
  interface.add({.parent{root}, .name{}, .globals{"_controls_scroll"}, .type{13}, .area{288, 129, 303, 388}, .flags{0x48149}});
  interface.link_scroll("_controls_list", "_controls_scroll");
  label("_controls_selected", {315, 132, 628, 148}, "");
  label("_controls_binding", {315, 154, 628, 185}, "");
  button("bind", {315, 191, 466, 213}, "Bind axis");
  button("clear", {474, 191, 628, 213}, "Clear binding");
  button("negative", {315, 221, 466, 243}, "Bind minus");
  button("positive", {474, 221, 628, 243}, "Bind plus");
  button("invert", {315, 251, 466, 273}, "Invert: off");
  button("clear_all", {474, 251, 628, 273}, "Clear all");
  button("deadzone", {315, 281, 466, 303}, "Deadzone: 6%");
  button("response", {474, 281, 628, 303}, "Response: 1.5");
  button("calibrate", {315, 311, 628, 333}, "Calibrate axis");
  label("_controls_devices", {16, 401, 624, 433}, "No controllers connected.");
  label("_controls_status", {16, 441, 535, 476}, "");
  button("back", {550, 446, 628, 469}, "Done");
}
std::string controls_menu::fit(std::string text, int const width) const {
  if(font.width(text) <= width) return text;
  while(!text.empty() && font.width(text + "...") > width) text.pop_back();
  return text + "...";
}
void controls_menu::refresh() {
  auto const &p{settings.controllers.current()};
  interface.named("_redux_controls_summary").text = p.enabled ? "Controller + keyboard\n" + fit(p.name, 260) : "Keyboard flight\nControllers disabled";
  if(interface.current() != "_controls_screen") return;
  interface.named("_controls_profile").text = fit("Profile: " + (renaming ? edited_name + "_" : p.name), 285);
  interface.named("_controls_mode").text = p.enabled ? "Controller + keyboard" : "Keyboard only";
  auto &list{interface.named("_controls_list")};
  list.text.clear();
  if(buttons_tab) {
    for(size_t i{0}; i < input::actions.size(); ++i) list.text += (p.buttons[i].assigned() ? "+ " : "  ") + std::string{input::actions[i].name} + '\n';
  } else {
    for(size_t i{0}; i < input::axis_names.size(); ++i) {
      auto const &a{p.axes[i]};
      list.text += ((a.positive.assigned() || a.negative.assigned()) ? "+ " : "  ") + std::string{input::axis_names[i]} + '\n';
    }
  }
  list.selected_line = static_cast<int>(selection);
  interface.named("_controls_axes").on = !buttons_tab;
  interface.named("_controls_buttons").on = buttons_tab;
  interface.named("_controls_selected").text = std::string{buttons_tab ? input::actions[selection].name : input::axis_names[selection]};
  interface.named("_controls_bind").text = buttons_tab ? "Bind input" : "Bind axis";
  auto const visible{[&](std::string_view id, bool show){auto &w{interface.named(id)}; w.flags = (w.flags & ~1u) | static_cast<unsigned int>(show);}};
  for(auto name : {"_controls_negative", "_controls_positive", "_controls_invert", "_controls_deadzone", "_controls_response", "_controls_calibrate"}) visible(name, !buttons_tab);
  auto &description{interface.named("_controls_binding").text};
  if(buttons_tab) description = fit(input::describe(p.buttons[selection]), 310);
  else {
    auto const &a{p.axes[selection]};
    description = a.paired ? "- " + fit(input::describe(a.negative), 297) + "\n+ " + fit(input::describe(a.positive), 297) : fit(input::describe(a.positive), 310);
    interface.named("_controls_negative").text = std::array<std::string, 4>{"Bind decrease", "Bind right", "Bind forward", "Bind left"}[selection];
    interface.named("_controls_positive").text = std::array<std::string, 4>{"Bind increase", "Bind left", "Bind back", "Bind right"}[selection];
    interface.named("_controls_invert").text = a.inverted ? "Invert: on" : "Invert: off";
    interface.named("_controls_deadzone").text = std::format("Deadzone: {}%", std::lround(a.deadzone * 100));
    interface.named("_controls_response").text = std::format("Response: {}", a.response);
    interface.named("_controls_calibrate").text = calibrating ? "Finish calibration" : "Calibrate axis";
  }
  auto &connected{interface.named("_controls_devices").text};
  connected = devices.empty() ? "No controllers connected. Keyboard flight is available." : "Connected: ";
  for(auto const &d : devices) {
    if(connected != "Connected: ") connected += ", ";
    connected += d.id.name;
  }
  connected = fit(connected, 605);
  if(devices.size() == 1) connected += std::format("\n{} axes, {} buttons, {} hats", devices[0].axes.size(), devices[0].buttons.size(), devices[0].hats.size());
  interface.named("_controls_status").text = message.empty() ? "Select an item, then bind an axis, button or hat.\nEscape cancels input capture." : message;
}
void controls_menu::changed() {
  settings.controller_error.clear();
  settings.save();
  refresh();
}
void controls_menu::show() {
  learning = 0;
  calibrating = renaming = false;
  message = settings.controller_error.empty() ? "" : "Could not load controls.cfg. Choose defaults to replace it.";
  interface.show("_controls_screen");
  refresh();
}
void controls_menu::capture(int const mode) {
  if(devices.empty()) { message = "Connect a controller first."; refresh(); return; }
  learning = mode;
  calibrating = renaming = false;
  learner.begin(devices, mode == 1);
  message = mode == 1 ? "Move the axis through at least half its range.\nEscape cancels." : "Press a button, move a hat or squeeze a trigger.\nEscape cancels.";
  refresh();
}
void controls_menu::presets() {
  auto const offered{devices};
  std::vector<std::pair<std::string, int>> choices{{"Keyboard only (clear bindings)", -1}};
  for(size_t i{0}; i < devices.size(); ++i) choices.emplace_back(fit(devices[i].id.name, 460), static_cast<int>(i));
  interface.popup(std::move(choices), [this, offered](int index){
    auto const apply{[this, index, offered](int gamepad){
      auto const name{settings.controllers.current().name};
      settings.controllers.current() = index < 0 ? input::controller_profile{} : input::joystick_defaults(offered.at(static_cast<size_t>(index)), gamepad != 0);
      settings.controllers.current().name = name;
      message = index < 0 ? "Keyboard defaults restored." : "Preset applied. Bind any remaining axes below.";
      changed();
    }};
    if(index >= 0 && !offered.at(static_cast<size_t>(index)).gamepad_axes.empty()) interface.popup({{"Gamepad (triggers: rudder, bumpers: collective)", 1}, {"Raw joystick axes and buttons", 0}}, apply);
    else apply(0);
  });
}
void controls_menu::finish_calibration() {
  auto &a{settings.controllers.current().axes[selection]};
  auto const value{input::raw_value(a.positive, devices)};
  if(!value || maximum - minimum < .25 || (selection != 0 && (*value <= minimum + .03 || *value >= maximum - .03))) {
    message = "Move through the full range first.\nFor pitch/roll/pedals, release to centre before finishing.";
    refresh();
    return;
  }
  a.minimum = minimum;
  a.maximum = maximum;
  a.centre = selection == 0 ? (minimum + maximum) / 2 : *value;
  calibrating = false;
  message = "Calibration saved.";
  changed();
}
void controls_menu::poll(std::span<input::device_state const> samples, bool const focused) {
  devices.assign(samples.begin(), samples.end());
  refresh();
  if(interface.current() != "_controls_screen") { learning = 0; calibrating = renaming = false; return; }
  if(!focused) {
    if(learning || calibrating) { cancel(); message = "Capture cancelled when the window lost focus."; refresh(); }
    return;
  }
  if(learning) {
    if(auto source{learner.poll(devices)}) {
      auto &p{settings.controllers.current()};
      if(learning == 4) p.buttons[selection] = *source;
      else {
        auto &a{p.axes[selection]};
        if(learning == 1) {
          a = {};
          a.positive = *source;
          if(selection == 0) a.response = 1;
          if(selection == 1) a.inverted = true;
        } else {
          if(!a.paired) a = {};
          a.paired = true;
          (learning == 2 ? a.negative : a.positive) = *source;
        }
      }
      p.enabled = true;
      message = fit("Bound " + input::describe(*source), 510);
      auto const same{[&](input::binding const &b){return b.assigned() && b.device == source->device && b.kind == source->kind && b.index == source->index && (b.kind != input::source_kind::hat || b.direction == source->direction);}};
      unsigned int uses{0};
      for(auto const &a : p.axes) {
        uses += static_cast<unsigned int>(same(a.positive));
        if(a.paired) uses += static_cast<unsigned int>(same(a.negative));
      }
      for(auto const &b : p.buttons) uses += static_cast<unsigned int>(same(b));
      if(uses > 1) message += "\nThis input is also bound elsewhere in this profile.";
      learning = 0;
      changed();
    }
  } else if(calibrating) {
    if(auto value{input::raw_value(settings.controllers.current().axes[selection].positive, devices)}) {
      minimum = std::min(minimum, *value);
      maximum = std::max(maximum, *value);
    }
  }
}
bool controls_menu::cancel() {
  if(!learning && !calibrating && !renaming) return false;
  learning = 0;
  calibrating = renaming = false;
  message = "Cancelled.";
  refresh();
  return true;
}
bool controls_menu::text_input(unsigned int const value) {
  if(interface.current() != "_controls_screen") return false;
  if(renaming) {
    if(value == 27) cancel();
    else if(value == 10) {
      if(!edited_name.empty()) { settings.controllers.current().name = edited_name; renaming = false; message.clear(); changed(); }
    } else if(value == 8) { if(!edited_name.empty()) edited_name.pop_back(); }
    else if(value >= 32 && value <= 126 && edited_name.size() < 48) edited_name += static_cast<char>(value);
    refresh();
    return true;
  }
  if(value == 27) return cancel();
  if(calibrating && value == 10) finish_calibration();
  return learning || calibrating;
}
bool controls_menu::activate(widget &object, int const line) {
  auto const action{object.action};
  if(!action.starts_with("redux_controls_")) return false;
  auto const id{action.substr(15)};
  if(id == "open") { show(); return true; }
  if(id == "back") { if(!cancel()) interface.back(); return true; }
  if(learning || renaming || calibrating) {
    if(id == "calibrate" && calibrating) finish_calibration();
    else { cancel(); message = "Cancelled. Select the control again to continue."; refresh(); }
    return true;
  }
  auto &p{settings.controllers.current()};
  if(id == "axes" || id == "buttons") {
    buttons_tab = id == "buttons";
    selection = 0;
    interface.named("_controls_list").start_line = 0;
    message.clear();
  } else if(id == "list") {
    auto const count{buttons_tab ? input::actions.size() : input::axis_names.size()};
    if(line >= 0 && static_cast<size_t>(line) < count) selection = static_cast<size_t>(line);
  } else if(id == "profile") {
    std::vector<std::pair<std::string, int>> choices;
    for(size_t i{0}; i < settings.controllers.profiles.size(); ++i) choices.emplace_back(settings.controllers.profiles[i].name, static_cast<int>(i));
    interface.popup(std::move(choices), [this](int index){settings.controllers.selected = static_cast<size_t>(index); message.clear(); changed();});
  } else if(id == "new") {
    if(settings.controllers.profiles.size() >= 16) message = "Up to 16 profiles can be saved. Delete one first.";
    else {
      auto copy{p};
      copy.name = "Profile " + std::to_string(settings.controllers.profiles.size() + 1);
      settings.controllers.profiles.push_back(std::move(copy));
      settings.controllers.selected = settings.controllers.profiles.size() - 1;
      changed();
    }
  } else if(id == "rename") {
    renaming = true;
    edited_name = p.name;
    message = "Type a profile name. Enter saves; Escape cancels.";
  } else if(id == "delete") {
    interface.popup({{"Keep profile", 0}, {"Delete this profile", 1}}, [this](int yes){
      if(!yes) return;
      auto &c{settings.controllers};
      c.profiles.erase(c.profiles.begin() + static_cast<std::ptrdiff_t>(c.selected));
      if(c.profiles.empty()) c.profiles.emplace_back();
      c.selected = std::min(c.selected, c.profiles.size() - 1);
      changed();
    });
  } else if(id == "mode") { p.enabled = !p.enabled; changed(); }
  else if(id == "presets") presets();
  else if(id == "bind") capture(buttons_tab ? 4 : 1);
  else if(id == "negative" || id == "positive") capture(id == "negative" ? 2 : 3);
  else if(id == "clear") {
    if(buttons_tab) p.buttons[selection] = {};
    else p.axes[selection] = {};
    changed();
  } else if(id == "clear_all") {
    interface.popup({{"Keep bindings", 0}, {"Clear every binding in this profile", 1}}, [this](int yes){
      if(yes) { auto &p{settings.controllers.current()}; p.axes = {}; p.buttons = {}; changed(); }
    });
  } else if(id == "invert") { p.axes[selection].inverted = !p.axes[selection].inverted; changed(); }
  else if(id == "deadzone" || id == "response") {
    bool const deadzone{id == "deadzone"};
    std::vector<std::pair<std::string, int>> choices;
    if(deadzone) for(int value : {0, 2, 4, 6, 8, 10, 15, 20, 30, 40, 50}) choices.emplace_back(std::format("{}%", value), value);
    else for(int value : {50, 75, 100, 125, 150, 175, 200, 250, 300}) choices.emplace_back(std::format("{}{}", value / 100., value == 100 ? " (linear)" : ""), value);
    interface.popup(std::move(choices), [this, deadzone](int value){auto &a{settings.controllers.current().axes[selection]}; (deadzone ? a.deadzone : a.response) = value / 100.; changed();});
  } else if(id == "calibrate") {
    auto const &a{p.axes[selection]};
    if(a.paired || !input::raw_value(a.positive, devices)) message = "Bind a single connected axis first.\nTrigger pairs use their captured resting positions.";
    else {
      calibrating = true;
      minimum = maximum = *input::raw_value(a.positive, devices);
      message = "Move through full travel, then release to centre.\nClick Finish calibration (or Enter). Escape cancels.";
    }
  }
  refresh();
  return true;
}
void controls_menu::paint(render::framebuffer &frame, widget const &object) {
  if(&object == &interface.named("_preferences_controls_area")) {
    frame.fill(object.area, 249);
    frame.surround(object.area);
  }
  if(&object != &interface.named("_controls_screen")) return;
  frame.fill({0, 0, 639, 479}, 246);
  for(auto r : {render::rectangle{9, 126, 306, 392}, {310, 126, 631, 392}, {9, 397, 631, 437}}) {
    frame.fill(r, 249); frame.surround(r, true);
  }
  auto const &p{settings.controllers.current()};
  double value{0};
  bool connected{false};
  if(buttons_tab) {
    connected = input::raw_value(p.buttons[selection], devices).has_value();
    value = input::activation(p.buttons[selection], devices);
  } else {
    auto const sample{input::axis_value(p.axes[selection], selection == 0, devices)};
    connected = sample.has_value();
    value = sample.value_or(0);
  }
  font.draw(frame, connected ? std::format("Live input: {:+.2f}", value) : "Live input: not connected / unbound", 317, 342, 251);
  render::rectangle const bar{319, 363, 620, 379};
  frame.fill(bar, 246); frame.surround(bar, true);
  if(connected) {
    int const middle{(bar.left + bar.right) / 2};
    int const position{static_cast<int>(std::lround(middle + value * 146))};
    frame.fill({std::min(middle, position), 367, std::max(middle, position), 375}, 250);
    frame.fill({middle, 364, middle, 378}, 251);
  }
}
} // namespace apache::frontend
