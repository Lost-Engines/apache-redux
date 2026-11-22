#include "control_input.h"
#include <algorithm>
#include <stdexcept>

namespace apache::simulation {
void control_configuration::resolve() {
  if(cyclic > 4) cyclic = 0;
  if(collective != 6 || cyclic == 2) collective = 0;
  if(tail_rotor != 5) tail_rotor = 0;
  input_type = cyclic == 0 ? 1 : 2;
  button_mode = cyclic == 3 ? 1 : cyclic == 4 ? 2 : 0;
  absolute_throttle = collective == 6 || cyclic == 3 || cyclic == 4;
  rudder = tail_rotor == 5; analogue_hat = cyclic == 2;
}
joystick_curve::joystick_curve(unsigned int const deadband) {
  if(deadband >= 127) throw std::runtime_error{"Joystick deadband must be below 127"};
  unsigned int const step{0xb504 / (127 - deadband)};
  for(unsigned int i{deadband}; i != 128; ++i) {
    auto const distance{(i - deadband) * step};
    values[128 + i] = static_cast<int32_t>(distance * distance); values[127 - i] = -values[128 + i];
  }
}
int32_t joystick_curve::sample(unsigned int const mode, joystick_axis const &axis) const {
  int32_t const centre{mode >= 3 ? static_cast<int32_t>((int64_t{axis.minimum} + axis.maximum) / 2) : axis.centre};
  if(axis.minimum >= centre || centre >= axis.maximum) return 0;
  auto const value{std::clamp(axis.value, axis.minimum, axis.maximum)};
  if(mode == 1 || mode == 3) {
    if(value > centre) return -static_cast<int32_t>((int64_t{axis.maximum - value} * INT32_MAX) / (axis.maximum - centre));
    return static_cast<int32_t>((int64_t{value - axis.minimum} * INT32_MAX) / (centre - axis.minimum));
  }
  if(mode == 2 || mode == 4) {
    auto const index{value > centre ? 255 - (int64_t{axis.maximum - value} * 127) / (axis.maximum - centre) : (int64_t{value - axis.minimum} * 127) / (centre - axis.minimum)};
    return values[static_cast<size_t>(index)];
  }
  throw std::runtime_error{"Invalid joystick sampling mode"};
}
joystick_commands decode_joystick_buttons(unsigned int const mode, unsigned int const buttons, bool const analogue_hat, unsigned int const hat_position) {
  joystick_commands out; auto const emit{[&](uint32_t key){ if(key) out.keys.at(out.count++) = key; }};
  auto const nibble{buttons & 15};
  if(mode == 0) {
    if(nibble & 1) emit(0x39);
    if(nibble & 2) emit(0x1001c);
    if(nibble & 4) emit(0xe);
    if(nibble & 8) emit(0x10026);
  } else if(mode == 1 || mode == 2) {
    static std::array<uint32_t, 16> constexpr first{0, 0x39, 0xe, 0x4f, 0x10026, 0, 0, 0x47, 0x1001c, 0, 0, 0x51, 0, 0, 0, 0x49};
    static std::array<uint32_t, 16> constexpr second{0, 0xe, 0x39, 0x4f, 0x10026, 0, 0x33, 0x47, 0x1001c, 0x21, 0x23, 0x51, 0x14, 0, 0x34, 0x49};
    out.alternate = mode == 2 && nibble == 5;
    if(mode == 2 && nibble == 9) emit(0x2e);
    emit((mode == 1 ? first : second)[nibble]);
  } else throw std::runtime_error{"Invalid joystick button mode"};
  if(analogue_hat) {
    auto const position{static_cast<uint64_t>(hat_position) * 100 / 256};
    emit(position < 6 ? 0x49 : position < 31 ? 0x51 : position < 56 ? 0x47 : position < 81 ? 0x4f : 0);
  }
  return out;
}
joystick_commands control_input::button_commands() const {
  if(!joystick_present || configuration.input_type == 1) return {};
  unsigned int bits{buttons & 15}, position{255};
  if(configuration.button_mode && hat >= 1 && hat <= 4) bits |= std::array<unsigned int, 4>{15, 11, 7, 3}[hat - 1];
  if(configuration.analogue_hat) {
    if(hat_present && hat >= 1 && hat <= 4) position = (hat - 1) * 64;
    else if(!hat_present && axis_count >= 4) {
      auto const &axis{joystick[3]}; int32_t const centre{static_cast<int32_t>((int64_t{axis.minimum} + axis.maximum) / 2)};
      if(axis.minimum < centre && centre < axis.maximum) {
        int32_t const value{std::clamp(axis.value, axis.minimum, axis.maximum)};
        position = static_cast<unsigned int>(value > centre ? 255 - int64_t{axis.maximum - value} * 127 / (axis.maximum - centre) : int64_t{value - axis.minimum} * 127 / (centre - axis.minimum));
      }
    }
  }
  return decode_joystick_buttons(configuration.button_mode, bits, configuration.analogue_hat, position);
}
void control_input::key(unsigned int const scan, bool const down) {
  if(scan < held.size()) held[scan] = down;
}
void control_input::timer(uint32_t const ticks) {
  for(size_t i{0}; i != held.size(); ++i) if(held[i]) counts[i] += ticks;
}
void control_input::update(unsigned int const view_moves, bool const gunner, bool const joystick_alt, bool const throttle_reversed) {
  static std::array<std::array<unsigned int, 4>, 6> constexpr keys{{{0x10, 0x4e, 0x1e, 0x4a}, {0x2c, 0x52, 0x2d, 0x53}, {0x50, 0x50, 0x48, 0x48}, {0x4d, 0x4d, 0x4b, 0x4b}, {0x11, 0x11, 0x1f, 0x1f}, {0x34, 0x34, 0x33, 0x33}}};
  for(size_t i{0}; i != keys.size(); ++i) {
    auto const &k{keys[i]}; auto const positive{std::max(counts[k[0]], counts[k[1]])}, negative{std::max(counts[k[2]], counts[k[3]])};
    auto &v{keyboard[i]};
    if(positive || held[k[0]] || held[k[1]]) { v = math::add(std::max(v, 0), std::bit_cast<int32_t>(positive << 25)); if(v < 0) v = INT32_MAX; } else if(negative || held[k[2]] || held[k[3]]) { v = math::subtract(std::min(v, 0), std::bit_cast<int32_t>(negative << 25)); if(v > 0) v = -INT32_MAX; } else v = 0;
    for(auto key : k) counts[key] = 0;
  }
  std::array<int32_t, 4> axes; std::copy_n(keyboard.begin(), 4, axes.begin());
  if(configuration.input_type != 1) {
    axes[2] = curve.sample(2, joystick[1]); axes[3] = curve.sample(2, joystick[0]);
    if(configuration.absolute_throttle) { axes[0] = curve.sample(3, joystick[3]); if(throttle_reversed) axes[0] = math::negate(axes[0]); }
    if(configuration.rudder) axes[1] = math::negate(curve.sample(2, joystick[2]));
  }
  if(view_moves == 2 || (view_moves == 1 && (held[0x38] || joystick_alt))) external = axes;
  else { flight.controls = axes; external.fill(0); }
  flight.absolute_throttle = configuration.absolute_throttle; flight.type = configuration.input_type;
  if(!gunner) keyboard[4] = 0;
}
} // namespace apache::simulation
