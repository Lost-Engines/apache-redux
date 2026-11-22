#include "mission_end.h"
#include <limits>

namespace apache::simulation {
bool mission_end::request(unsigned int const reason, bool const invulnerable, bool const no_crashes) {
  if(cause) return false;
  switch(reason) {
  case 2:
  case 3:
  case 12:
    if(invulnerable || no_crashes) return false;
    cause = reason;
    return true;
  case 4:
  case 11:
    if(invulnerable) return false;
    cause = reason;
    return true;
  case 10:
    cause = reason;
    return true;
  case 5:
  case 6:
  case 7:
    active = false;
    [[fallthrough]];
  case 1:
  case 8:
  case 9:
    cause = reason;
    if(!connected) {
      active = false;
    } else if(!countdown) countdown = advance_frames > std::numeric_limits<unsigned int>::max() - 2 ? std::numeric_limits<unsigned int>::max() : advance_frames + 2;
    return false;
  default:
    return false;
  }
}
void mission_end::finish_frame() {
  if(!countdown) return;
  quitting = true;
  if(!--countdown) active = false;
}
} // namespace apache::simulation
