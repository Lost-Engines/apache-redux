#pragma once
#include "mission_session.h"
#include "multiplayer/peer_group.h"

namespace apache::frontend {
// Connects the verified APNET receivers to a live group. The clock and event
// pump are supplied by the application, so waiting remains testable headlessly.
class mission_link {
  multiplayer::peer_group &peers;
  mission_session &mission;
  std::function<int32_t()> clock;
  int32_t deadline{0};
  unsigned int attempts{0};
  uint32_t synchronised_server{0};
  bool complete{false};
  bool closed{false};
  bool transferring{false};
  void receive(multiplayer::packet const &message, int32_t now);
  void initial_state();
  void initialised();
  uint32_t next_server(uint32_t previous) const;
public:
  std::function<bool()> yield;
  mission_link(multiplayer::peer_group &peers, mission_session &mission, std::function<int32_t()> clock);
  ~mission_link();
  mission_link(mission_link const &) = delete;
  mission_link &operator=(mission_link const &) = delete;
  void poll();
  void wait();
  void close();
  bool ready() const {
    return complete && !transferring;
  }
};
} // namespace apache::frontend
