#pragma once
#include "mission_network.h"

namespace apache::simulation {
// Shared pilot/gunner state applied to the same aircraft used by the cockpit.
class mission_crew {
  mission_runtime &runtime;
  mission_network &network;
public:
  multiplayer::crew_state outgoing;
  std::array<std::array<std::string, 10>, 2> messages;
  std::array<std::array<uint8_t, 10>, 2> samples{};
  bool leaving{false};
  uint8_t next_frame_ticks{0};
  std::function<void(uint16_t)> key;
  std::function<void(uint8_t)> landing;
  std::function<void(unsigned int)> death;
  std::function<void(std::string_view, uint8_t)> radio;
  mission_crew(mission_runtime &mission, mission_network &connection);
  ~mission_crew();
  mission_crew(mission_crew const &) = delete;
  mission_crew &operator=(mission_crew const &) = delete;
  bool notify_key(uint16_t code) {
    return outgoing.notify_key(code);
  }
  void receive(multiplayer::crew_state const &state);
  multiplayer::packet transmit(unsigned int ticks);
};
} // namespace apache::simulation
