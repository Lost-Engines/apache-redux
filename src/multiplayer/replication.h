#pragma once
#include <functional>
#include "crew.h"
#include "packet.h"

namespace apache::multiplayer {
struct player_replica {
  aircraft_state aircraft;
  std::string name;
  bool assigned{false};
  uint32_t flags{0};
  int32_t score{0};
  double updated_at{0}, frame_time{0}, accumulated_frame_time{0};
  uint32_t weapon{0}, shot_seed{0};
  target_state target, shot_target;
};
// APNET's flight-state exchange, independent of transport and scene ownership.
class replication {
public:
  std::array<player_replica, 16> players;
  uint32_t local{0}, server{0};
  bool active{false}, client_server{false}, dead{false};
  crew_role seat{crew_role::pilot};
  std::array<bool, 16> synced{}, initial_synced{};
  uint32_t number_synced{0}, number_initial_synced{0};
  int32_t wind_strength{0}, wind_direction{0};
  uint32_t war_seed{1}, weapon_seed{1};
  bool server_in_game{false}, server_waiting{false}, new_server_data{true}, all_synced{false};
  int32_t answer_deadline{0}, client_frames{0};
  double ping_timer{0}, interpolated_time{0};
  std::function<void(uint32_t, crew_state const &)> crew_received;
  std::function<void(uint32_t, int32_t)> score_changed;
  void reset_server();
  void begin_frame();
  // Returns false for unhandled messages or messages ignored in this state.
  bool receive(packet const &message, int32_t now);
  void extrapolate(int32_t now);
};
} // namespace apache::multiplayer
