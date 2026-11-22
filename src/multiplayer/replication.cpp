#include "replication.h"
#include <algorithm>
#include <bit>
#include <cmath>

namespace apache::multiplayer {
namespace {
int32_t signed_word(packet const &message, size_t offset) {
  return std::bit_cast<int32_t>(message.word(offset));
}
double real_word(packet const &message, size_t offset) {
  return std::bit_cast<double>(uint64_t{message.word(offset)} | (uint64_t{message.word(offset + 4)} << 32));
}
}
void replication::reset_server() {
  all_synced = false;
  server_in_game = false;
  new_server_data = true;
  server_waiting = false;
}
void replication::begin_frame() {
  synced.fill(false);
  number_synced = 0;
}
bool replication::receive(packet const &message, int32_t const now) {
  if(local >= players.size() || server >= players.size()) return false;
  uint32_t const sender{message.sender()};
  if(sender >= players.size()) return false;
  auto &player{players[sender]};
  bool const remote{sender != local};
  switch(message.type()) {
    case message_type::weather:
      wind_strength = signed_word(message, 68); wind_direction = signed_word(message, 72); return true;
    case message_type::initial_state:
      if(client_server || !remote || !active || initial_synced[sender]) return false;
      initial_synced[sender] = true; ++number_initial_synced; player.name = message.name();
      player.flags = (player.flags & ~0xff00u) | (sender << 8);
      for(size_t i{0}; i < 3; ++i) player.aircraft.location.position[i] = signed_word(message, 24 + i * 4);
      // The original sender never initializes the matrix in this message.
      if(sender == 0) { war_seed = message.word(88); weapon_seed = message.word(92); }
      return true;
    case message_type::position:
      if(client_server || !remote || !active || synced[sender]) return false;
      synced[sender] = true; ++number_synced; player.aircraft.location = message.pose(); player.aircraft.speed = signed_word(message, 72);
      if(player.assigned) {
        auto const kind{message.word(392)}; player.target = {};
        if(kind == 1 || kind == 2) { player.target.kind = kind; for(size_t i{0}; i < (kind == 1 ? 3u : 2u); ++i) player.target.data[i] = signed_word(message, 396 + i * 4); }
      }
      return true;
    case message_type::fired:
      if(!remote || !active) return false;
      player.weapon = message.word(96); player.shot_seed = message.word(112); player.shot_target.kind = message.word(392);
      if(player.weapon == 1 || player.weapon == 19 || player.shot_target.kind == 1) {
        for(size_t i{0}; i < 3; ++i) player.shot_target.data[i] = signed_word(message, 396 + i * 4);
      } else if(player.shot_target.kind == 2) {
        for(size_t i{0}; i < 2; ++i) player.shot_target.data[i] = signed_word(message, 396 + i * 4);
      }
      return true;
    case message_type::crew: {
      if(client_server || !remote || !active || dead || synced[sender]) return false;
      auto const decoded{decode_crew(seat == crew_role::pilot ? crew_role::gunner : crew_role::pilot, message.bytes().subspan(24, 104))};
      if(!decoded) return false;
      synced[sender] = true; ++number_synced;
      if(crew_received) crew_received(sender, *decoded);
      return true;
    }
    case message_type::client_state: {
      if(!remote || !active) return false;
      auto const delta{real_word(message, 80)}; if(!std::isfinite(delta) || !std::isfinite(ping_timer)) return false;
      player.aircraft.location = message.pose(); player.aircraft.speed = signed_word(message, 72); player.frame_time = delta;
      // The reference reads its clock past the packet. Use the bounded arrival clock.
      player.updated_at = now; player.flags |= 0x40000;
      client_frames = math::add(client_frames, 1); player.accumulated_frame_time += delta - ping_timer; return true;
    }
    case message_type::server_state: {
      auto const decoded{server_state(message)};
      for(size_t i{0}; i < players.size(); ++i) {
        if(active) players[i].aircraft = decoded[i]; else players[i].aircraft.model = decoded[i].model;
      }
      new_server_data = true; answer_deadline = math::add(now, 2000); return true;
    }
    case message_type::server_ready:
      server_in_game = true; answer_deadline = math::add(now, 2000); return true;
    case message_type::server_waiting:
      server_waiting = true; return true;
    case message_type::nominate_server: {
      if(!active) return false;
      auto const clock{real_word(message, 80)}; if(!std::isfinite(clock)) return false;
      server = local; answer_deadline = math::add(now, 2000); new_server_data = true; client_frames = 0;
      ping_timer = std::max(ping_timer, clock); for(auto &entry : players) entry.accumulated_frame_time = 0;
      extrapolate(now); return true;
    }
    case message_type::scores:
      if(server == local) return false;
      for(size_t i{0}; i < players.size(); ++i) {
        auto const score{signed_word(message, 24 + i * 4)};
        if(players[i].score != score && score_changed) score_changed(static_cast<uint32_t>(i), score);
        players[i].score = score;
      }
      return true;
    case message_type::synchronise:
      if(server != local || !(players[local].flags & 0x20000) || (player.flags & 0x20000)) return false;
      player.flags |= 0x20000; player.aircraft.model = signed_word(message, 76); return true;
    default: return false;
  }
}
void replication::extrapolate(int32_t const now) {
  if(local >= players.size() || server != local) return;
  for(size_t i{0}; i < players.size(); ++i) {
    auto &player{players[i]}; if(!player.assigned || i == server) continue;
    if(!std::isfinite(player.updated_at)) { player.updated_at = now; continue; }
    interpolated_time = (static_cast<double>(now) - player.updated_at) * 0.005; player.updated_at = now;
    for(size_t j{0}; j < 3; ++j) {
      auto const velocity{math::multiply_q31(player.aircraft.location.rotation[3 + j], player.aircraft.speed)};
      auto const movement{static_cast<long double>(interpolated_time) * 0.005 * velocity};
      auto const bounded{std::clamp(movement, -2147483648.0L, 2147483647.0L)};
      player.aircraft.location.position[j] = math::add(player.aircraft.location.position[j], static_cast<int32_t>(bounded));
    }
  }
}
} // namespace apache::multiplayer
