#include <algorithm>
#include <bit>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include "input/controllers.h"
#include "simulation/control_input.h"

namespace input = apache::input;
namespace simulation = apache::simulation;
namespace {
void require(bool const value, char const *message) { if(!value) throw std::runtime_error{message}; }
bool close(double const a, double const b, double const epsilon = 1e-6) { return std::abs(a - b) < epsilon; }
size_t action(std::string_view id) { return static_cast<size_t>(std::ranges::find(input::actions, id, &input::action::id) - input::actions.begin()); }
input::device_state device(std::string name, std::string guid) {
  return {{std::move(guid), std::move(name), 0}, std::vector<float>(12), std::vector<unsigned char>(40), {0, 0}, {0, 0, 0, 0, -1, -1}, std::vector<unsigned char>(15)};
}
input::binding axis(input::device_state const &d, unsigned int const i) { return {d.id, input::source_kind::axis, i}; }
input::binding button(input::device_state const &d, unsigned int const i) { return {d.id, input::source_kind::button, i}; }
}
int main() try {
  auto stick{device("HOTAS stick", "stick-guid")};
  auto throttle{device("Throttle", "throttle-guid")};
  auto pedals{device("Pedals", "pedals-guid")};
  input::controller_profile p;
  require(!p.enabled, "a connected joystick must not enable controller flight by default");
  p.enabled = true;
  p.axes[2].positive = axis(stick, 7);
  p.axes[3].positive = axis(stick, 9);
  p.axes[1].positive = axis(pedals, 4);
  p.axes[0].positive = axis(throttle, 8);
  for(auto &a : p.axes) { a.response = 1; a.deadzone = .1; }
  p.buttons[action("fire")] = button(stick, 31);
  p.buttons[action("weapon")] = button(throttle, 27);
  p.buttons[action("look_left")] = {stick.id, input::source_kind::hat, 1, 8};
  p.buttons[action("pilot")] = {stick.id, input::source_kind::hat, 1, 1};
  std::vector devices{throttle, pedals, stick};
  devices[0].axes[8] = -.6f;
  devices[1].axes[4] = .55f;
  devices[2].axes[7] = -.55f;
  devices[2].axes[9] = .55f;
  input::controller_mapper mapper;
  auto value{mapper.sample(p, devices, true)};
  require(close(*value.axes[1], .5) && close(*value.axes[2], -.5) && close(*value.axes[3], .5), "split devices and axes beyond the old four-axis limit");
  auto reordered{devices};
  std::ranges::reverse(reordered);
  require(mapper.sample(p, reordered, true).axes == value.axes, "bindings must survive device enumeration order changes");
  devices[2].axes[7] = .08f;
  require(*mapper.sample(p, devices, true).axes[2] == 0, "stick noise inside deadzone");
  devices[2].axes[7] = .55f;
  p.axes[2].response = 2;
  p.axes[2].inverted = true;
  require(close(*mapper.sample(p, devices, true).axes[2], -.25), "axis curve and inversion");
  p.axes[2].minimum = -.6; p.axes[2].centre = .1; p.axes[2].maximum = .8;
  devices[2].axes[7] = .1f;
  require(close(*mapper.sample(p, devices, true).axes[2], 0), "calibrated off-centre stick");
  devices[2].axes[7] = .8f;
  require(close(*mapper.sample(p, devices, true).axes[2], -1), "calibrated reduced axis travel");
  devices[2].buttons[31] = devices[0].buttons[27] = 1;
  value = mapper.sample(p, devices, true);
  require(value.pressed[action("fire")] && value.pressed[action("weapon")], "high-numbered buttons on different devices");
  value = mapper.sample(p, devices, true);
  require(value.down[action("fire")] && !value.pressed[action("weapon")], "held fire, one weapon cycle per button press");
  value = mapper.sample(p, devices, false);
  require(!value.down[action("fire")] && !value.axes[2] && value.absolute_collective, "focus loss releases actions, neutralises cyclic and holds collective");
  require(!mapper.sample(p, devices, true).down[action("fire")], "focus return must wait for button release");
  devices[2].buttons[31] = 0;
  mapper.sample(p, devices, true);
  devices[2].buttons[31] = 1;
  require(mapper.sample(p, devices, true).pressed[action("fire")], "button rearms after release");
  devices[2].hats[1] = 9;
  value = mapper.sample(p, devices, true);
  require(value.down[action("look_left")] && value.down[action("pilot")], "second hat and diagonal directions");
  auto const collective{value.axes[0]};
  value = mapper.sample(p, {}, true);
  require(!value.down[action("fire")] && !value.axes[2] && value.axes[0] == collective && value.absolute_collective, "unplug must release buttons without jumping an absolute collective lever");
  require(!mapper.sample(p, devices, true).pressed[action("fire")], "reconnect with held fire must not shoot");
  p.enabled = false;
  value = mapper.sample(p, devices, true);
  require(!value.absolute_collective && !value.axes[0] && !value.axes[2], "keyboard-only profile ignores all controller movement");

  auto pad{device("Xbox controller", "pad-guid")};
  auto gamepad{input::joystick_defaults(pad, true)};
  std::vector pads{pad};
  mapper.reset();
  value = mapper.sample(gamepad, pads, true);
  require(value.axes[1] == 0 && value.axes[0] == 0 && !value.absolute_collective, "released gamepad triggers and bumpers are neutral");
  pads[0].gamepad_axes[5] = 1;
  require(mapper.sample(gamepad, pads, true).axes[1] == -1, "right trigger operates right pedal");
  pads[0].gamepad_axes[4] = 1;
  require(mapper.sample(gamepad, pads, true).axes[1] == 0, "equal trigger input cancels");
  pads[0].gamepad_axes[5] = -1;
  require(mapper.sample(gamepad, pads, true).axes[1] == 1, "left trigger operates left pedal");
  pads[0].gamepad_buttons[5] = 1;
  value = mapper.sample(gamepad, pads, true);
  require(value.axes[0] == 1 && !value.absolute_collective, "bumper collective is a rate input, not an absolute jump");
  pads[0].gamepad_buttons[5] = 0;
  require(mapper.sample(gamepad, pads, true).axes[0] == 0, "releasing bumpers stops changing collective");
  for(int i{0}; i < 100; ++i) require(mapper.sample(gamepad, pads, true).axes[1] == 1, "sampling rate must not change analogue control strength");

  input::binding_capture capture;
  pads[0].axes[3] = -1; // Released trigger; a different throttle is already full forward.
  pads[0].axes[4] = 1;
  pads[0].buttons[30] = 1;
  capture.begin(pads, false);
  require(!capture.poll(pads), "capture ignores resting nonzero axes and buttons already held");
  pads[0].axes[5] = .1f;
  require(!capture.poll(pads), "capture ignores small jitter");
  pads[0].axes[3] = .2f;
  auto learned{capture.poll(pads)};
  require(learned && learned->index == 3 && learned->rest == -1 && learned->end == 1, "trigger capture records the released endpoint");
  capture.begin(pads, false);
  pads[0].buttons[30] = 0;
  require(!capture.poll(pads), "releasing a held button does not bind it");
  pads[0].buttons[30] = 1;
  require(capture.poll(pads)->index == 30, "released button can subsequently be bound");
  capture.begin(pads, true);
  pads[0].buttons[29] = 1;
  require(!capture.poll(pads), "flight axis capture does not bind a button by mistake");
  pads[0].axes[10] = -.8f;
  learned = capture.poll(pads);
  require(learned && learned->index == 10 && learned->end == -1 && learned->rest == 0, "negative half-axis capture");
  pads[0].axes[10] = 1;
  input::axis_binding paired;
  paired.paired = true; paired.deadzone = 0; paired.response = 1;
  paired.positive = *learned;
  pads[0].axes[10] = -.5f;
  require(close(*input::axis_value(paired, false, pads), .5), "centred stick half can be used as a paired input");

  input::controller_settings settings;
  settings.profiles = {p, gamepad}; settings.selected = 1;
  settings.profiles[0].name = "HOTAS \\\"setup\\\"";
  auto const serialized{settings.serialise()};
  require(input::controller_settings::parse(serialized) == settings, "all profiles, GUIDs, calibration and bindings survive a save/load roundtrip");
  for(auto const text : {"", "APACHE_CONTROLS 2\n0 1", "APACHE_CONTROLS 1\n0 99999999", "APACHE_CONTROLS 1\n10 1"}) {
    bool failed{false}; try { input::controller_settings::parse(text); } catch(std::exception const &) { failed = true; }
    require(failed, "corrupt/unsupported profile accepted");
  }
  auto bad{serialized}; bad += "garbage";
  bool failed{false}; try { input::controller_settings::parse(bad); } catch(std::exception const &) { failed = true; }
  require(failed, "trailing profile corruption accepted");
  settings.profiles[0].axes[0].positive.device.guid = "*";
  require(input::resolve_imported_devices(settings, devices), "legacy first-joystick reference resolves");
  require(settings.profiles[0].axes[0].positive.device == devices[0].id, "legacy reference freezes to device GUID");
  require(!input::resolve_imported_devices(settings, devices), "legacy migration runs only once");

  simulation::control_input controls;
  input::mapped_input mapped;
  mapped.axes[1] = .5;
  mapped.axes[0] = -1;
  mapped.absolute_collective = true;
  controls.controller(mapped);
  controls.key(0x4d, true);
  controls.timer(10);
  controls.update(0, false);
  require(controls.flight.controls[1] > INT32_MAX / 2 - 2 && controls.flight.controls[3] > 0, "pedals combine with keyboard cyclic");
  require(controls.flight.absolute_throttle && std::bit_cast<uint32_t>(controls.flight.controls[0]) == UINT32_MAX, "absolute collective minimum uses the flight model's full unsigned encoding");
  mapped.axes[0] = 0;
  controls.controller(mapped); controls.update(0, false);
  require(std::bit_cast<uint32_t>(controls.flight.controls[0]) == 0x80000000u, "absolute collective midpoint");
  mapped.axes[0] = 1;
  controls.controller(mapped); controls.update(0, false);
  require(controls.flight.controls[0] == 0, "absolute collective maximum");
  controls.key(0x39, true);
  mapped.down[action("fire")] = true;
  controls.controller(mapped);
  controls.key(0x39, false);
  require(controls.held[0x39], "keyboard release must not cancel held joystick fire");
  controls.key(0x39, true);
  mapped.down[action("fire")] = false;
  controls.controller(mapped);
  require(controls.held[0x39], "joystick release must not cancel held keyboard fire");
  controls.clear_keys(); controls.controller({});
  require(!controls.held[0x39] && !controls.held[0x4d], "focus/help clear all held sources");
  controls.key(0x4d, true); controls.timer(10); controls.update(1, false, true);
  require(controls.external[3] > 0, "mapped controls preserve external camera routing");
  std::cout << "Controller mapping, capture, persistence and flight integration checks passed\n";
  return 0;
} catch(std::exception const &error) { std::cerr << error.what() << '\n'; return 1; }
