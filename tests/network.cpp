#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include "multiplayer/casualties.h"
#include "multiplayer/crew.h"
#include "multiplayer/lifecycle.h"
#include "multiplayer/packet.h"

namespace {
void require(bool condition, char const *message) {
  if(!condition) throw std::runtime_error{message};
}
}
int main() try {
  using apache::multiplayer::actor_link;
  using apache::multiplayer::aircraft_state;
  using apache::multiplayer::casualties;
  using apache::multiplayer::crew_role;
  using apache::multiplayer::crew_state;
  using apache::multiplayer::decode_crew;
  using apache::multiplayer::encode_crew;
  using apache::multiplayer::event;
  using apache::multiplayer::kills;
  using apache::multiplayer::lifecycle;
  using apache::multiplayer::message_type;
  using apache::multiplayer::packet;
  using apache::multiplayer::replication;
  using apache::multiplayer::server_state;
  using apache::multiplayer::two_player_settings;
  std::array<aircraft_state, 16> players;
  auto &first{players[0]}; first.location.position = {0x1234567, -1, 0xfedcba9};
  first.location.rotation = {INT32_MAX, 0, 0, 0, -INT32_MAX, 0, 0, 0, INT32_MAX}; first.speed = -12345; first.model = -1;
  auto const state{server_state(players)};
  // Captured from send_server_data_ and apnet_receive_ in the reference engine.
  std::array<uint8_t, 36> const original{0x1e,0,0,0,0x56,0x34,0x12,0xff,0xff,0xff,0xba,0xdc,0xfe,0xc7,0xcf,0xff,0xff,0xff,0xff,0x7f,0,0,0,0,0,0,0,0x80,0,0,0,0,0,0,0xff,0x7f};
  require(std::ranges::equal(state.bytes().first(36), original), "original compressed aircraft record");
  auto const decoded{server_state(state)[0]};
  require(decoded.location.position == apache::math::vector3{19088736, 268435440, 267242400}, "original 24-bit coordinate decoding");
  require(decoded.location.rotation == apache::math::matrix3{2147418112,0,0,0,-2147418113,0,0,0,2147418112}, "original orientation normalization");
  require(decoded.speed == -12345 && decoded.model == -1, "signed speed and model decoding");
  for(size_t size{0}; size < state.bytes().size(); ++size) require(!packet::decode(state.bytes().first(size)), "truncated server packet rejected");
  packet message{message_type::identity, 3};
  for(size_t size{0}; size < message.bytes().size(); ++size) require(!packet::decode(message.bytes().first(size)), "truncated message rejected");
  message.word(16, 16); require(!packet::decode(message.bytes()), "out-of-range player rejected");
  message.word(16, 3); message.word(0, 0xffffffff); require(!packet::decode(message.bytes()), "unknown message rejected");
  auto options{two_player_settings({3, 1, 2, 1, 0})};
  require(options.bytes().size() == 24 && packet::decode(options.bytes()).has_value(), "short two-player options packet");
  auto batch{kills(0, "Pilot", false, 63, std::array<uint8_t, 252>{})};
  require(packet::decode(batch.bytes()).has_value(), "full mobile casualty batch");
  batch.word(24, 64); require(!packet::decode(batch.bytes()), "oversized mobile casualty count rejected");
  batch = kills(0, "Pilot", true, 28, std::array<uint8_t, 336>{});
  batch.word(24, 29); require(!packet::decode(batch.bytes()), "oversized scenery casualty count rejected");
  casualties queue;
  for(unsigned int i{0}; i < 63; ++i) require(queue.mobile(5, static_cast<int32_t>(i), 2), "mobile casualty accepted");
  require(!queue.mobile(5, 63, 2), "full casualty list refuses overflow");
  auto sent{queue.take_mobile(0, "Pilot")}; require(sent && !queue.take_mobile(0, "Pilot"), "casualties sent once");
  replication replica; replica.local = 0; replica.active = true;
  lifecycle life{replica}; size_t removed{99};
  life.events.find = [](int32_t serial, int32_t task)->std::optional<actor_link> { if(serial == 2 && task == 3) return actor_link{7, 0, false}; return {}; };
  life.events.remove = [&](size_t id){ removed = id; };
  auto collided{event(message_type::collision, 1, "Pilot", 2, 3)};
  require(life.receive(collided) && removed == 7, "collision resolves the transmitted task and drone identity");
  collided.word(400, INT32_MAX); removed = 99;
  require(life.receive(collided) && removed == 99, "unknown collision task ignored");
  crew_state shared;
  for(unsigned int i{0}; i < 10; ++i) require(!shared.notify_key(static_cast<uint16_t>(i + 1)), "ten distinct crew actions accepted");
  require(shared.notify_key(1) && shared.notify_key(11) && shared.keys.size() == 10, "duplicate/full crew queue suppressed");
  shared.message = 10; shared.pilot.landing = 3; shared.pilot.fuel = 2442; shared.pilot.weight = 16000;
  auto payload{encode_crew(crew_role::pilot, shared)};
  require(shared.message == 0 && shared.pilot.landing == 0 && shared.keys.empty(), "one-shot crew events consumed");
  auto crew_value{decode_crew(crew_role::pilot, payload)};
  require(crew_value && crew_value->keys.size() == 10 && crew_value->message == 10 && crew_value->pilot.landing == 3, "crew events preserved on receive");
  for(size_t size{0}; size < payload.size(); ++size) require(!decode_crew(crew_role::pilot, std::span{payload}.first(size)), "truncated crew record rejected");
  payload[44] = 11; require(!decode_crew(crew_role::pilot, payload), "crew key count cannot overrun record");
  payload[44] = 10; payload[47] = 11; require(!decode_crew(crew_role::pilot, payload), "unknown radio message rejected");
  payload[47] = 10; payload[76] = 3; require(!decode_crew(crew_role::gunner, payload), "unknown gunner target type rejected");
  require(!decode_crew(static_cast<crew_role>(2), payload), "single-player role rejected for shared cockpit");
  return EXIT_SUCCESS;
} catch(std::exception const &error) { std::cerr << error.what() << '\n'; return EXIT_FAILURE; }
