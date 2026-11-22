#include "mfd_state.h"
#include <stdexcept>

namespace apache::simulation {
namespace {
std::array<std::array<uint32_t, 2>, 2> constexpr fault_bits{{{0x10, 0x20}, {0x40, 0x80}}};
mfd_page next(mfd_page const current, bool const forwards) {
  auto const value{static_cast<unsigned int>(current)};
  if(value < 2 || value > 9) throw std::runtime_error{"Invalid MFD cycle page"};
  return static_cast<mfd_page>(forwards ? (value == 9 ? 2 : value + 1) : (value == 2 ? 9 : value - 1));
}
} // anonymous namespace
mfd_state::mfd_state(unsigned int const game_mode) {
  initialise(game_mode);
}
void mfd_state::initialise(unsigned int const game_mode) {
  reset();
  if(game_mode == 1 || game_mode == 4 || game_mode == 5) {
    pages[0][1] = mfd_page::tactical; pages[1] = {mfd_page::flight_plan, mfd_page::map};
  }
}
void mfd_state::reset() {
  pages = {{{mfd_page::radar, mfd_page::flight_plan}, {mfd_page::weapons, mfd_page::tactical}}};
}
mfd_page mfd_state::exclusive(bool const gunner, bool const right, mfd_page const current) const {
  auto const page{next(current, forwards)};
  return page == pages[gunner][!right] ? next(page, forwards) : page;
}
void mfd_state::cycle(bool const gunner, bool const right, bool const forward) {
  forwards = forward;
  auto &page{pages[gunner][right]};
  if(page != mfd_page::off && page != mfd_page::damaged) page = exclusive(gunner, right, page);
}
void mfd_state::toggle(bool const gunner, bool const right, uint32_t const faults) {
  auto &page{pages[gunner][right]};
  if(page != mfd_page::off) page = mfd_page::off;
  else page = (faults & fault_bits[gunner][right]) ? mfd_page::damaged : exclusive(gunner, right, mfd_page::map);
}
void mfd_state::show_radar(bool const gunner, uint32_t const faults) {
  auto &seat{pages[gunner]};
  if(!(faults & fault_bits[gunner][0])) {
    if(seat[0] != mfd_page::radar && seat[1] != mfd_page::radar) seat[0] = mfd_page::radar;
  } else if(!(faults & fault_bits[gunner][1]) && seat[1] != mfd_page::radar) seat[1] = mfd_page::radar;
}
void mfd_state::apply_faults(bool const gunner, uint32_t const faults) {
  for(size_t side{0}; side != 2; ++side) if((faults & fault_bits[gunner][side]) && pages[gunner][side] != mfd_page::off) pages[gunner][side] = mfd_page::damaged;
}
} // namespace apache::simulation
