#pragma once
#include <array>
#include <cstdint>

namespace apache::simulation {
enum class mfd_page : unsigned int {
  off,
  damaged,
  map,
  radar,
  tactical,
  flir,
  flight_plan,
  weapons,
  engines,
  status
};
class mfd_state {
  mfd_page exclusive(bool gunner, bool right, mfd_page current) const;
public:
  std::array<std::array<mfd_page, 2>, 2> pages;
  bool forwards{false};
  explicit mfd_state(unsigned int game_mode = 0);
  void initialise(unsigned int game_mode);
  void reset();
  void cycle(bool gunner, bool right, bool forward);
  void toggle(bool gunner, bool right, uint32_t faults);
  void show_radar(bool gunner, uint32_t faults);
  void apply_faults(bool gunner, uint32_t faults);
};
} // namespace apache::simulation
