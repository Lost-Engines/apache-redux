#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <thread>
#include "multiplayer/connections.h"
#include "multiplayer/peer_group.h"
#include "multiplayer/transport.h"

namespace {
void require(bool value, char const *description) {
  if(!value) throw std::runtime_error{description};
}
auto wait(apache::multiplayer::transport &transport) {
  auto const deadline{std::chrono::steady_clock::now() + std::chrono::seconds{2}};
  std::vector<apache::multiplayer::received_packet> messages;
  do {
    messages = transport.poll();
    if(!messages.empty()) break;
    std::this_thread::sleep_for(std::chrono::milliseconds{1});
  } while(std::chrono::steady_clock::now() < deadline);
  return messages;
}
} // anonymous namespace
int main() try {
  namespace platform = apache::platform;
  using apache::multiplayer::aircraft_state;
  using apache::multiplayer::connections;
  using apache::multiplayer::crew;
  using apache::multiplayer::event;
  using apache::multiplayer::identification;
  using apache::multiplayer::menu_selection;
  using apache::multiplayer::message_type;
  using apache::multiplayer::packet;
  using apache::multiplayer::peer_group;
  using apache::multiplayer::server_state;
  using apache::multiplayer::transport;
  using apache::multiplayer::two_player_settings;
  platform::network_address constexpr loopback{0x7f000001, 0};
  require(platform::network_address::resolve("127.0.0.1:4661", 0) == platform::network_address{0x7f000001, 4661}, "numeric endpoint");
  require(platform::network_address::resolve("localhost", 4661).host == loopback.host, "local host name");
  require(platform::network_address{0x7f000001, 4661}.text() == "127.0.0.1:4661", "endpoint presentation");
  for(auto text : {"127.0.0.1:65536", "127.0.0.1:-1", "127.0.0.1:1x", "127.0.0.1:"}) {
    bool rejected{false};
    try {
      platform::network_address::resolve(text, 0);
    } catch(std::invalid_argument const&) {
      rejected = true;
    }
    require(rejected, "invalid port rejected");
  }
  transport first{{
    .bind{loopback},
    .broadcast{false},
    .peers{},
    .node{11}
  }};
  transport second{{
    .bind{loopback},
    .broadcast{false},
    .peers{first.address()},
    .node{22}
  }};
  require(first.address().port && second.address().port && first.address() != second.address(), "independent ephemeral loopback sockets");
  require(first.poll().empty(), "empty poll is nonblocking");
  bool occupied{false};
  try {
    platform::datagram_socket duplicate{first.address()};
  } catch(std::runtime_error const&) {
    occupied = true;
  }
  require(occupied, "binding cannot silently steal another socket");
  std::vector<packet> messages{
    identification(message_type::discover, UINT32_MAX, "PILOT", 64),
    two_player_settings({3, 1, 2, 1, 0}),
    server_state(std::array<aircraft_state, 16>{}),
    crew(1, "GUNNER", std::array<uint8_t, 104>{}),
    event(message_type::collision, 1, "GUNNER", 3, 7)
  };
  for(auto const &message : messages) {
    require(first.send(second.address(), message), "loopback send");
    auto incoming{wait(second)};
    require(incoming.size() == 1, "one datagram per APNET message");
    require(incoming[0].node == 11 && incoming[0].source == first.address(), "transport source identity");
    require(std::ranges::equal(incoming[0].message.bytes(), message.bytes()), "all APNET payload bytes survive UDP");
    second.discover(message);
    auto reply{wait(first)};
    require(reply.size() == 1 && reply[0].node == 22 && std::ranges::equal(reply[0].message.bytes(), message.bytes()), "direct discovery peers and reverse delivery");
  }
  platform::datagram_socket noise{loopback};
  auto const probe{messages.front()};
  apache::resources::bytes envelope{'A', 'P', 'C', 'H', 'U', 'D', 'P', 1, 33, 0, 0, 0, 0, 0, 0, 0};
  envelope.insert(envelope.end(), probe.bytes().begin(), probe.bytes().end());
  auto bad_magic{envelope};
  bad_magic[0] = 0;
  noise.send(first.address(), bad_magic);
  auto own{envelope};
  own[8] = 11;
  noise.send(first.address(), own);
  auto zero{envelope};
  zero[8] = 0;
  noise.send(first.address(), zero);
  noise.send(first.address(), std::span{envelope}.first(19));
  auto truncated{envelope};
  truncated.pop_back();
  noise.send(first.address(), truncated);
  auto oversized{envelope};
  oversized.resize(2048, 0);
  noise.send(first.address(), oversized);
  auto wrong_type{envelope};
  wrong_type[16] = 255;
  noise.send(first.address(), wrong_type);
  auto wrong_sender{envelope};
  wrong_sender[32] = 16;
  wrong_sender[33] = wrong_sender[34] = wrong_sender[35] = 0;
  noise.send(first.address(), wrong_sender);
  // A valid trailing packet ensures all earlier invalid packets were actually read.
  noise.send(first.address(), envelope);
  auto accepted{wait(first)};
  require(accepted.size() == 1 && accepted[0].node == 33, "invalid, foreign and self packets discarded without losing valid traffic");
  connections roster{50, first.address()};
  require(roster.local() == 0 && roster.other_count() == 0, "empty lobby includes its local player");
  require(roster.observe(70, second.address(), 100) == 1, "peer discovered");
  require(roster.observe(20, noise.address(), 100) == 0 && roster.local() == 1 && roster.find(70) == 2, "lobby sorts identities consistently");
  require(roster.observe(70, second.address(), 200) == 2 && roster.other_count() == 2, "repeated discovery refreshes existing slot");
  roster.sorted = false;
  require(roster.remove(20) && roster.local() == 1 && roster.find(70) == 2, "flight departure does not renumber aircraft");
  require(roster.observe(10, noise.address(), 200) == 0 && roster.local() == 1, "new player reuses a vacant flight slot");
  require(!roster.observe(70, noise.address(), 200), "active endpoint cannot migrate through a stray packet");
  require(!roster.expire(10000), "lobby aging is disabled during flight");
  roster.sorted = true;
  require(roster.expire(5000) && roster.local() == 0 && roster.other_count() == 0, "stale lobby peers expire");
  require(!roster.remove(50), "local slot retained");
  roster.observe(70, second.address(), UINT32_MAX - 10);
  require(!roster.expire(5, 20) && roster.expire(11, 20), "lobby aging handles clock wrap");
  for(uint64_t i{1}; i <= 15; ++i) require(roster.observe(100 + i, second.address(), 0).has_value(), "sixteen-player capacity");
  require(!roster.observe(999, noise.address(), 0) && roster.other_count() == 15, "full lobby refuses an extra slot");
  peer_group leader{{
    .bind{loopback},
    .broadcast{false},
    .peers{},
    .node{100}
  }, "LEADER"};
  peer_group wingman{{
    .bind{loopback},
    .broadcast{false},
    .peers{leader.address()},
    .node{200}
  }, "WINGMAN"};
  auto pump{[&](uint32_t tick, auto complete){
    auto const deadline{std::chrono::steady_clock::now() + std::chrono::seconds{2}};
    do {
      leader.poll(tick);
      wingman.poll(tick);
      if(complete()) return;
      std::this_thread::sleep_for(std::chrono::milliseconds{1});
    } while(std::chrono::steady_clock::now() < deadline);
  }};
  pump(0, [&]{
    return leader.state.players[1].name == "WINGMAN" && wingman.state.players[0].name == "LEADER";
  });
  require(leader.members().other_count() == 1 && wingman.members().other_count() == 1, "two independent sockets discover each other");
  require(leader.state.local == 0 && wingman.state.local == 1, "peers agree on connection numbers");
  require(leader.state.server == 0 && wingman.state.server == 0, "peers agree on the initial server");
  require(leader.state.players[1].name == "WINGMAN" && wingman.state.players[0].name == "LEADER", "lobby names delivered");
  wingman.name("GUNNER");
  pump(10, [&]{return leader.state.players[1].name == "GUNNER";});
  require(leader.state.players[1].name == "GUNNER", "name changes reach peers");
  leader.room.preferences = {1, 0, 0, 1, 0};
  leader.ready();
  pump(20, [&]{return wingman.room.preferences == leader.room.preferences;});
  require(wingman.room.preferences == leader.room.preferences && !leader.all_ready(), "preferences arrive before takeoff readiness");
  wingman.ready();
  pump(30, [&]{return leader.all_ready() && wingman.all_ready();});
  require(leader.all_ready() && wingman.all_ready(), "both peers acknowledge readiness");
  unsigned int selections{0};
  wingman.room.events.selection = [&](unsigned int selected, auto const &arguments){
    require(selected == 14 && arguments[2] == 3, "remote menu selection payload");
    ++selections;
  };
  leader.send(menu_selection(0, "LEADER", 14, {0, -1, 3, 0, 0}));
  pump(40, [&]{return selections != 0;});
  require(selections == 1, "remote menu callback delivered once");
  leader.freeze();
  wingman.freeze();
  unsigned int flight_packets{0};
  wingman.game_packet = [&](packet const &packet, int32_t tick){
    require(packet.type() == message_type::crew && packet.sender() == 0 && tick == 50, "flight callback retains sender and arrival clock");
    ++flight_packets;
  };
  leader.send(crew(0, "LEADER", std::array<uint8_t, 104>{}));
  pump(50, [&]{return flight_packets != 0;});
  require(flight_packets == 1, "same sockets deliver live crew records after lobby freeze");
  leader.state.players[0].flags |= 0x20000;
  unsigned int sync_packets{0};
  unsigned int server_packets{0};
  leader.game_packet = [&](packet const &message, int32_t){
    if(message.type() == message_type::synchronise) {
      ++sync_packets;
    } else if(message.type() == message_type::server_state) ++server_packets;
  };
  wingman.send(server_state(std::array<aircraft_state, 16>{}));
  wingman.send(identification(message_type::synchronise, 1, "GUNNER", 64));
  pump(55, [&]{return sync_packets != 0;});
  require(sync_packets == 1 && !(leader.state.players[1].flags & 0x20000), "flight initialization reaches the mission receiver without mutating the lobby copy");
  require(server_packets == 0, "a client cannot supply the server's headerless state packet");
  leader.resume();
  wingman.resume();
  pump(60, [&]{return !leader.all_ready() && !wingman.all_ready();});
  require(!leader.all_ready() && !wingman.all_ready(), "return to lobby clears readiness");
  wingman.leave();
  pump(70, [&]{return leader.members().other_count() == 0;});
  require(leader.members().other_count() == 0 && !leader.all_ready(), "departure removes the lobby peer");
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << error.what() << '\n';
  return EXIT_FAILURE;
}
