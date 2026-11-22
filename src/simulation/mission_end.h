#pragma once

namespace apache::simulation {
// set_die_flag_ and the shared-connection shutdown at the end of a game frame.
struct mission_end {
  unsigned int cause{0};
  unsigned int countdown{0};
  unsigned int advance_frames{0};
  bool active{true};
  bool connected{false};
  bool quitting{false};
  // Returns true only when a newly accepted cause selects the crash view.
  bool request(unsigned int reason, bool invulnerable, bool no_crashes);
  void finish_frame();
};
} // namespace apache::simulation
