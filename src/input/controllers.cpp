#include "controllers.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace apache::input {
namespace {
bool same_device(device_id const &a, device_id const &b) {
  return a.instance == b.instance && (a.guid.empty() ? a.name == b.name : a.guid == b.guid);
}
bool analogue(source_kind kind) { return kind == source_kind::axis || kind == source_kind::gamepad_axis; }
double shape(double value, double deadzone, double response) {
  auto const magnitude{std::clamp((std::abs(value) - deadzone) / (1 - deadzone), 0., 1.)};
  return std::copysign(std::pow(magnitude, response), value);
}
void write_binding(std::ostream &out, binding const &b) {
  out << static_cast<int>(b.kind) << ' ' << std::quoted(b.device.guid) << ' ' << std::quoted(b.device.name) << ' ' << b.device.instance << ' ' << b.index << ' ' << b.direction << ' ' << b.rest << ' ' << b.end << '\n';
}
binding read_binding(std::istream &in) {
  binding b;
  int kind{0};
  if(!(in >> kind >> std::quoted(b.device.guid) >> std::quoted(b.device.name) >> b.device.instance >> b.index >> b.direction >> b.rest >> b.end) || kind < 0 || kind > 5 || b.index > 4095 || b.device.instance > 15 || b.direction > 15 || !std::isfinite(b.rest) || !std::isfinite(b.end) || b.rest < -1 || b.rest > 1 || b.end < -1 || b.end > 1 || (analogue(static_cast<source_kind>(kind)) && std::abs(b.end - b.rest) < 0.1)) throw std::runtime_error{"Invalid controller binding"};
  b.kind = static_cast<source_kind>(kind);
  return b;
}
} // namespace
std::string controller_settings::serialise() const {
  std::ostringstream out;
  out << std::setprecision(17) << "APACHE_CONTROLS 1\n" << selected << ' ' << profiles.size() << '\n';
  for(auto const &p : profiles) {
    out << std::quoted(p.name) << ' ' << p.enabled << '\n';
    for(auto const &a : p.axes) {
      out << a.paired << ' ' << a.inverted << ' ' << a.minimum << ' ' << a.centre << ' ' << a.maximum << ' ' << a.deadzone << ' ' << a.response << '\n';
      write_binding(out, a.positive);
      write_binding(out, a.negative);
    }
    // Action names keep the file stable when new bindable actions are added.
    out << actions.size() << '\n';
    for(size_t i{0}; i < actions.size(); ++i) {
      out << actions[i].id << ' ';
      write_binding(out, p.buttons[i]);
    }
  }
  return out.str();
}
controller_settings controller_settings::parse(std::string_view text) {
  if(text.size() > 1024 * 1024) throw std::runtime_error{"Controller settings are too large"};
  std::istringstream in{std::string{text}};
  std::string header;
  unsigned int version{0};
  size_t count{0};
  controller_settings result;
  if(!(in >> header >> version >> result.selected >> count) || header != "APACHE_CONTROLS" || version != 1 || count == 0 || count > 16 || result.selected >= count) throw std::runtime_error{"Invalid controller profile file"};
  result.profiles.resize(count);
  for(auto &p : result.profiles) {
    if(!(in >> std::quoted(p.name) >> p.enabled) || p.name.size() > 64 || p.name.empty() || p.name.find_first_of("\r\n") != std::string::npos) throw std::runtime_error{"Invalid controller profile name"};
    for(auto &a : p.axes) {
      if(!(in >> a.paired >> a.inverted >> a.minimum >> a.centre >> a.maximum >> a.deadzone >> a.response) || !std::isfinite(a.minimum) || !std::isfinite(a.centre) || !std::isfinite(a.maximum) || !std::isfinite(a.deadzone) || !std::isfinite(a.response) || a.minimum < -1 || a.maximum > 1 || a.minimum >= a.centre || a.centre >= a.maximum || a.deadzone < 0 || a.deadzone > .5 || a.response < .25 || a.response > 4) throw std::runtime_error{"Invalid controller axis calibration"};
      a.positive = read_binding(in);
      a.negative = read_binding(in);
    }
    size_t buttons{0};
    if(!(in >> buttons) || buttons > 256) throw std::runtime_error{"Invalid controller action count"};
    for(size_t i{0}; i < buttons; ++i) {
      std::string id;
      in >> id;
      auto value{read_binding(in)};
      auto const found{std::ranges::find(actions, id, &action::id)};
      if(found != actions.end()) p.buttons[static_cast<size_t>(found - actions.begin())] = std::move(value);
    }
  }
  in >> std::ws;
  if(!in.eof()) throw std::runtime_error{"Unexpected controller profile data"};
  return result;
}
device_state const *find_device(binding const &source, std::span<device_state const> devices) {
  if(!source.assigned()) return nullptr;
  if(source.device.guid == "*") return devices.empty() ? nullptr : &devices.front();
  auto const found{std::ranges::find_if(devices, [&](auto const &d){return same_device(source.device, d.id);})};
  return found == devices.end() ? nullptr : &*found;
}
std::optional<double> raw_value(binding const &source, std::span<device_state const> devices) {
  auto const *d{find_device(source, devices)};
  if(!d) return {};
  auto const i{source.index};
  switch(source.kind) {
  case source_kind::axis: if(i < d->axes.size() && std::isfinite(d->axes[i])) return std::clamp(double(d->axes[i]), -1., 1.); break;
  case source_kind::gamepad_axis: if(i < d->gamepad_axes.size() && std::isfinite(d->gamepad_axes[i])) return std::clamp(double(d->gamepad_axes[i]), -1., 1.); break;
  case source_kind::button: if(i < d->buttons.size()) return d->buttons[i] ? 1. : 0.; break;
  case source_kind::gamepad_button: if(i < d->gamepad_buttons.size()) return d->gamepad_buttons[i] ? 1. : 0.; break;
  case source_kind::hat: if(i < d->hats.size()) return (d->hats[i] & source.direction) == source.direction ? 1. : 0.; break;
  default: break;
  }
  return {};
}
double activation(binding const &source, std::span<device_state const> devices) {
  auto const v{raw_value(source, devices)};
  if(!v) return 0;
  if(!analogue(source.kind)) return *v;
  auto const range{source.end - source.rest};
  return std::abs(range) < .1 ? 0 : std::clamp((*v - source.rest) / range, 0., 1.);
}
std::optional<double> axis_value(axis_binding const &axis, bool const collective, std::span<device_state const> devices) {
  double v{0};
  if(axis.paired) {
    if(!raw_value(axis.positive, devices) && !raw_value(axis.negative, devices)) return {};
    v = activation(axis.positive, devices) - activation(axis.negative, devices);
    v = shape(v, axis.deadzone, axis.response);
  } else {
    auto const raw{raw_value(axis.positive, devices)};
    if(!raw) return {};
    if(collective) {
      double const position{std::clamp((*raw - axis.minimum) / (axis.maximum - axis.minimum), 0., 1.)};
      double const dz{axis.deadzone * .5};
      v = std::pow(std::clamp((position - dz) / (1 - 2 * dz), 0., 1.), axis.response) * 2 - 1;
    } else {
      v = *raw >= axis.centre ? (*raw - axis.centre) / (axis.maximum - axis.centre) : (*raw - axis.centre) / (axis.centre - axis.minimum);
      v = shape(std::clamp(v, -1., 1.), axis.deadzone, axis.response);
    }
  }
  return axis.inverted ? -v : v;
}
std::string describe(binding const &source) {
  if(!source.assigned()) return "Unbound";
  std::string result{source.device.name.empty() ? "Joystick" : source.device.name};
  if(source.device.instance) result += " #" + std::to_string(source.device.instance + 1);
  result += ": ";
  switch(source.kind) {
  case source_kind::axis: result += "axis "; break;
  case source_kind::button: result += "button "; break;
  case source_kind::gamepad_axis: result += "pad axis "; break;
  case source_kind::gamepad_button: result += "pad button "; break;
  case source_kind::hat: result += "hat "; break;
  default: break;
  }
  result += std::to_string(source.index + 1);
  if(source.kind == source_kind::hat) result += source.direction == 1 ? " up" : source.direction == 2 ? " right" : source.direction == 4 ? " down" : " left";
  return result;
}
controller_profile joystick_defaults(device_state const &device, bool const gamepad) {
  controller_profile p;
  p.enabled = true;
  p.name = gamepad ? "Gamepad" : "Joystick";
  auto const axis{[&](unsigned int index){return binding{device.id, gamepad ? source_kind::gamepad_axis : source_kind::axis, index};}};
  auto const button{[&](unsigned int index){return binding{device.id, gamepad ? source_kind::gamepad_button : source_kind::button, index};}};
  if((gamepad ? device.gamepad_axes : device.axes).size() >= 2) {
    p.axes[3].positive = axis(0);
    p.axes[2].positive = axis(1);
  }
  if(gamepad && device.gamepad_axes.size() >= 6) {
    p.axes[1].paired = true;
    p.axes[1].negative = axis(4); // Left/right triggers both rest at -1.
    p.axes[1].positive = axis(5);
    p.axes[1].inverted = true; // Positive flight yaw is the left pedal.
    p.axes[0].paired = true;
    p.axes[0].positive = button(5); // Bumpers change collective, release holds it.
    p.axes[0].negative = button(4);
  }
  auto const set{[&](std::string_view id, binding b){p.buttons[static_cast<size_t>(std::ranges::find(actions, id, &action::id) - actions.begin())] = std::move(b);}};
  auto const count{(gamepad ? device.gamepad_buttons : device.buttons).size()};
  if(count > 0) set("fire", button(0));
  if(count > 1) set("weapon", button(1));
  if(count > 2) set("next_target", button(2));
  if(count > 3) set("lock", button(3));
  if(!gamepad && !device.hats.empty()) {
    set("pilot", {device.id, source_kind::hat, 0, 1});
    set("look_right", {device.id, source_kind::hat, 0, 2});
    set("gunner", {device.id, source_kind::hat, 0, 4});
    set("look_left", {device.id, source_kind::hat, 0, 8});
  } else if(gamepad && count >= 15) {
    set("pilot", button(11)); set("look_right", button(12));
    set("gunner", button(13)); set("look_left", button(14));
    set("pause", button(7));
  }
  return p;
}
bool resolve_imported_devices(controller_settings &settings, std::span<device_state const> devices) {
  if(devices.empty()) return false;
  bool changed{false};
  auto const resolve{[&](binding &b){if(b.device.guid == "*") { b.device = devices.front().id; changed = true; }}};
  for(auto &p : settings.profiles) {
    for(auto &a : p.axes) { resolve(a.positive); resolve(a.negative); }
    for(auto &b : p.buttons) resolve(b);
  }
  return changed;
}
void controller_mapper::reset() {
  armed.fill(false);
  previous.fill(false);
  collective.reset();
}
mapped_input controller_mapper::sample(controller_profile const &profile, std::span<device_state const> devices, bool const active) {
  mapped_input result;
  if(!profile.enabled) { reset(); return result; }
  bool const absolute{!profile.axes[0].paired && profile.axes[0].positive.assigned()};
  if(active) {
    for(size_t i{0}; i < result.axes.size(); ++i) result.axes[i] = axis_value(profile.axes[i], i == 0, devices);
    if(absolute && result.axes[0]) collective = result.axes[0];
  }
  // Keep an absolute lever's last position across focus loss/disconnection.
  // Pitch, roll and pedals fall back to keyboard/neutral instead of sticking.
  if(absolute && collective) { result.axes[0] = collective; result.absolute_collective = true; }
  for(size_t i{0}; i < actions.size(); ++i) {
    auto const value{activation(profile.buttons[i], devices)};
    bool const down{value > (previous[i] ? .4 : .6)};
    if(!active || !raw_value(profile.buttons[i], devices)) armed[i] = false;
    else if(!down) armed[i] = true;
    result.down[i] = active && armed[i] && down;
    result.pressed[i] = result.down[i] && !previous[i];
    previous[i] = result.down[i];
  }
  return result;
}
void binding_capture::begin(std::span<device_state const> devices, bool const only_axes) {
  baseline.assign(devices.begin(), devices.end());
  axes_only = only_axes;
}
std::optional<binding> binding_capture::poll(std::span<device_state const> devices) {
  std::optional<binding> axis;
  double largest{.45};
  for(auto const &d : devices) {
    auto previous{std::ranges::find_if(baseline, [&](auto const &p){return same_device(p.id, d.id);})};
    if(previous == baseline.end()) { baseline.push_back(d); continue; }
    for(size_t i{0}; i < std::min(previous->axes.size(), d.axes.size()); ++i) {
      double const delta{d.axes[i] - previous->axes[i]};
      if(std::isfinite(delta) && std::abs(delta) > largest) {
        largest = std::abs(delta);
        axis = binding{d.id, source_kind::axis, static_cast<unsigned int>(i), 1, previous->axes[i], delta > 0 ? 1. : -1.};
      }
    }
    if(!axes_only) {
      for(size_t i{0}; i < std::min(previous->buttons.size(), d.buttons.size()); ++i) {
        if(d.buttons[i] && !previous->buttons[i]) return binding{d.id, source_kind::button, static_cast<unsigned int>(i)};
      }
      for(size_t i{0}; i < std::min(previous->hats.size(), d.hats.size()); ++i) {
        unsigned int const changed{static_cast<unsigned int>(d.hats[i] & ~previous->hats[i])};
        for(unsigned int bit{1}; bit <= 8; bit <<= 1) if(changed & bit) return binding{d.id, source_kind::hat, static_cast<unsigned int>(i), bit};
      }
      // A held input becomes eligible once released during capture.
      for(size_t i{0}; i < std::min(previous->buttons.size(), d.buttons.size()); ++i) if(!d.buttons[i]) previous->buttons[i] = 0;
      for(size_t i{0}; i < std::min(previous->hats.size(), d.hats.size()); ++i) previous->hats[i] &= d.hats[i];
    }
  }
  return axis;
}
} // namespace apache::input
