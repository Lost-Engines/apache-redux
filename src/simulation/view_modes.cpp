#include "view_modes.h"
#include <stdexcept>

namespace apache::simulation {
view_modes::view_modes(resources::executable_data const &executable, unsigned int const control_mode) {
  if(control_mode > 2) throw std::runtime_error{"Invalid crew control mode"};
  home = control_mode == 1 ? 5 : 0;
  auto const read{[&](auto &values, uint32_t address){ resources::reader input{executable.at(address, 80), "View mode table"}; for(auto &value : values) value = input.u32(); }};
  read(crews, 0x4ebabc); read(left, 0x4ebecc); read(right, 0x4ebf1c); read(inside, 0x4ebf6c); read(moves, 0x4ebfbc);
  resources::reader input{executable.at(0x4ebb0c, 960), "Primary visual viewports"};
  for(auto &views : viewports) for(auto &view : views) for(auto &value : view) value = input.i32();
}
bool view_modes::valid(unsigned int const requested, view_context const &c) const {
  if(mode == 19 && ((!c.network && !c.serial_connection) || c.crash_timer != INT32_MAX)) return false;
  switch(requested) {
    case 0: return c.control_mode != 1 && mode != 0;
    case 5: return c.control_mode != 0 && mode != 5;
    case 11: return c.drone && mode != 11;
    case 13: return c.control_mode != 0 && mode != 13;
    case 14: return mode == 15 || c.latest_weapon.has_value();
    case 15: {
      auto const round{mode == 14 ? c.current_weapon : c.latest_weapon};
      return round && c.has_target && c.has_target(*round) && c.has_lock && c.has_lock(*round);
    }
    case 16: case 17: return true;
    case 18: return c.target_kind == 1 || c.target_kind == 2;
    case 1: case 2: case 3: case 4: case 6: case 7: case 8: case 9: case 10: case 12: case 19: return requested != mode;
    default: return false;
  }
}
bool view_modes::select(unsigned int const requested, view_context const &context, view_events const &events) {
  if(context.following || !valid(requested, context)) return false;
  if(events.leave) events.leave(mode);
  if(events.enter) events.enter(requested);
  mode = requested; crew = crews[mode]; in_cockpit = inside[mode] != 0; movement = moves[mode]; changed = true; return true;
}
std::array<int32_t, 4> view_modes::viewport(unsigned int const video_mode) const {
  if(mode >= viewports.size() || video_mode >= 3) throw std::runtime_error{"Invalid view viewport"};
  return viewports[mode][video_mode];
}
optical_devices::optical_devices(resources::executable_data const &executable) {
  auto const text{[&](uint32_t address){ resources::reader input{executable.at(address, 16), "Optical caption"}; return input.terminated_text(16); }};
  resources::reader devices{executable.at(0x4eb638, 48), "Optical devices"};
  for(size_t i{0}; i != 3; ++i) {
    names[i] = text(devices.u32()); auto const count{devices.u32()}; devices.u32(); auto const address{devices.u32()};
    if(count > 16) devices.fail("Too many optical magnifications");
    resources::reader entries{executable.at(address, count * 12), "Optical magnifications"};
    for(size_t j{0}; j != count; ++j) { auto const distance{entries.i32()}, rate{entries.i32()}; levels[i].push_back({distance, rate, text(entries.u32())}); }
  }
}
bool optical_devices::select(unsigned int const id, uint32_t const faults) {
  if(id > 2 || (id == 1 && (faults & 0x8000)) || (id == 2 && (faults & 0x800))) return false;
  device = id; return true;
}
bool optical_devices::cycle(unsigned int const view, uint32_t const faults) {
  if(view != 13) return false;
  if(device == 0) return select(1, faults) || select(2, faults);
  if(device == 1) return select(2, faults) || select(0, faults);
  return select(0, faults);
}
void optical_devices::check(uint32_t const faults) {
  if(device == 1 && (faults & 0x8000)) { if(!select(2, faults)) select(0, faults); } else if(device == 2 && (faults & 0x800)) select(0, faults);
}
void optical_devices::zoom(unsigned int const view, bool const raise) {
  if(view != 13) return;
  auto &level{selected[device]};
  if(raise) { if(level + 1 < levels[device].size()) ++level; } else if(level) --level;
}
} // namespace apache::simulation
