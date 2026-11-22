#include "lifecycle.h"
#include <bit>
#include "math/fixed.h"

namespace apache::multiplayer {
void lifecycle::notice(std::string const &text, int32_t const priority, int32_t const lifetime) const {
  if(events.notice) events.notice(text, priority, lifetime);
}
void lifecycle::lost_partner() {
  if(mode == game_mode::pilot_gunner) {
    state.active = false;
  } else if((mode == game_mode::single_missions || mode == game_mode::apache_vs_hind) && following) {
    state.active = false;
    following = false;
  }
}
void lifecycle::remove(std::optional<actor_link> const actor) const {
  if(actor && (!actor->local || actor->destroyed == 1) && events.remove) events.remove(actor->id);
}
bool lifecycle::receive(packet const &message, int32_t const now) {
  using enum message_type;
  auto const sender{message.sender()};
  if(sender >= state.players.size() || state.local >= state.players.size()) return false;
  auto const player{[&](uint32_t id){return id < state.players.size() && events.player ? events.player(id) : std::nullopt;}};
  auto const find{[&](int32_t serial, int32_t task){return events.find ? events.find(serial, task) : std::nullopt;}};
  bool const remote{sender != state.local};
  switch(message.type()) {
  case server_left:
  case client_left:
    {
    bool const server_departed{message.type() == server_left};
    if(state.active) {
      auto const departed{server_departed ? state.server : sender};
      if(server_departed) {
        if(events.unregister) events.unregister(departed);
        state.answer_deadline = math::add(now, 2000);
      }
      if(auto target{player(departed)}; target && target->destroyed == 0 && ((state.players[departed].flags >> 8) & 255) != 255) {
        if(events.unplace) events.unplace(target->id);
        state.players[departed].flags = (state.players[departed].flags & ~0xff00u) | 0xfe00u;
      }
      if(server_departed) state.server = events.next_server ? events.next_server(departed) : state.local;
      notice(message.name() + (server_departed ? " LEFT" : " left"), server_departed ? 3 : 2, 200);
    }
    if(server_departed) state.reset_server();
    return true;
  }
  case killed_player:
    {
    uint32_t const victim{message.word(396)};
    if(victim >= state.players.size()) return false;
    if(state.client_server && state.server == state.local) {
      notice(state.players[sender].name + " Scored", 3, 200);
      state.players[sender].score = math::add(state.players[sender].score, 1);
      if(events.scores) events.scores();
    }
    if(!remote || !state.active) return true;
    auto const target{player(victim)};
    if(!target || target->destroyed != 0) return true;
    if(victim == state.local) {
      if(events.player_killed) events.player_killed();
      if(mode == game_mode::pilot_gunner) state.active = false;
      return true;
    }
    if(state.client_server) notice(state.players[victim].name + " killed", 3, 200);
    if(mode == game_mode::pilot_gunner) {
      state.active = false;
    } else if(mode == game_mode::single_missions || mode == game_mode::apache_vs_hind) {
      if(following) {
        lost_partner();
      } else if(events.remove) events.remove(target->id);
    }
    return true;
  }
  case died:
    if(state.active && remote) {
      lost_partner();
      // The receiver uses the sender's connection number, rather than the
      // otherwise transmitted drone word. Flow-7 senders can differ here.
      remove(find(static_cast<int32_t>(sender), std::bit_cast<int32_t>(message.word(400))));
    }
    return true;
  case left:
    if(state.active && remote && !state.client_server) {
      other_players = math::subtract(other_players, 1);
      notice(message.name() + " left", 3, 400);
      notice(other_players == 0 ? "No players!" : std::to_string(other_players) + " players", 3, 400);
      if(mode == game_mode::combat) state.active = false;
      lost_partner();
      if(mode != game_mode::pilot_gunner) {
        if(auto target{player(sender)}; target && target->destroyed == 0 && events.leave) events.leave(target->id);
      }
    }
    return true;
  case disconnected:
    if(state.active && remote) {
      notice(message.name() + " LEFT", 3, 400);
      notice(other_players == 0 ? "No players!" : std::to_string(other_players) + " players", 3, 400);
      state.players[sender].flags = (state.players[sender].flags & ~0xff00u) | 0xff00u;
      if(mode == game_mode::pilot_gunner) state.active = false;
    }
    return true;
  case restarted:
    if(state.active) {
      if(mode == game_mode::pilot_gunner) state.active = false;
      if(remote) {
        if(auto target{player(sender)}) {
          if(target->destroyed != 1 && events.remove) events.remove(target->id);
          if(events.restart) events.restart(target->id, message.pose(), state.players[sender].aircraft.model);
          notice(message.name() + " restored", 2, 200);
        }
      }
    }
    return true;
  case crashed:
    if(state.active && remote) {
      if(mode == game_mode::combat || mode == game_mode::multiplayer || mode == game_mode::capture_the_flag) {
        notice(message.name() + " crashed", 2, 200);
      } else if(mode == game_mode::pilot_gunner) {
        if(events.death) events.death(3);
      } else {
        lost_partner();
      }
      remove(player(sender));
    }
    return true;
  case collision:
    if(state.active && remote) {
      if(message.word(396) == state.local) {
        if(events.player_killed) events.player_killed();
      } else {
        notice(message.name() + " collided", 2, 200);
        remove(find(std::bit_cast<int32_t>(message.word(396)), std::bit_cast<int32_t>(message.word(400))));
      }
    }
    return true;
  case chaff:
  case flare:
    if(state.active && remote) {
      if(auto target{player(mode == game_mode::pilot_gunner ? state.local : sender)}; target && events.countermeasure) events.countermeasure(target->id, message.type() == flare);
    }
    return true;
  case mobile_kills:
    {
    auto const bytes{message.bytes()};
    size_t const count{bytes[24]};
    if(count > 63) return false;
    resources::reader records{bytes.subspan(26, count * 4)};
    for(size_t i{0}; i < count; ++i) {
      auto const drone{std::bit_cast<int16_t>(records.u16())};
      auto const task{records.u8()};
      auto const source{records.u8()};
      if(auto target{find(drone, task)}) {
        if(target->local) {
          if(events.death) events.death(4);
        } else if(events.mobile_killed) events.mobile_killed(target->id, source);
      }
    }
    return true;
  }
  case scenery_kills:
    {
    auto const bytes{message.bytes()};
    size_t const count{bytes[24]};
    if(count > 28) return false;
    resources::reader records{bytes.subspan(28, count * 12)};
    for(size_t i{0}; i < count; ++i) {
      math::vector3 const position{records.i32(), records.i32(), 0};
      auto const source{records.u8()};
      records.skip(3);
      if(events.scenery_killed) events.scenery_killed(position, source);
    }
    return true;
  }
  case flag_won:
    state.active = false;
    if(events.flag_won) events.flag_won(sender);
    return true;
  default:
    return false;
  }
}
} // namespace apache::multiplayer
