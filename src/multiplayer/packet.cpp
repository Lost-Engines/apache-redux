#include "packet.h"
#include <algorithm>
#include <bit>
#include <stdexcept>

namespace apache::multiplayer {
namespace {
bool known_type(uint32_t const type) {
  return (type >= 2 && type <= 10) || type == 12 || type == 13 || (type >= 16 && type <= 36) || (type >= 38 && type <= 42);
}
bool headerless(message_type const type) {
  return type == message_type::server_state || type == message_type::two_player_settings;
}
size_t packet_size(message_type const type) {
  return type == message_type::server_state ? 516 : type == message_type::two_player_settings ? 24 : 408;
}
int32_t signed_word(uint32_t value) {
  return std::bit_cast<int32_t>(value);
}
}
packet::packet(message_type const type, uint32_t const sender, std::string_view const name)
  : storage(packet_size(type), 0) {
  if(!known_type(static_cast<uint32_t>(type))) throw std::runtime_error{"Unknown multiplayer message"};
  if(!headerless(type) && sender >= 16 && !(type == message_type::discover && sender == UINT32_MAX)) throw std::runtime_error{"Invalid multiplayer sender"};
  word(0, static_cast<uint32_t>(type));
  if(headerless(type)) return;
  word(16, sender);
  for(size_t i{0}; i < std::min(size_t{8}, name.size()) && name[i]; ++i) storage[4 + i] = static_cast<uint8_t>(name[i]);
}
std::optional<packet> packet::decode(std::span<uint8_t const> const source) {
  if(source.size() < 4) return {};
  resources::reader input{source};
  auto const type{input.u32()};
  if(!known_type(type) || source.size() != packet_size(static_cast<message_type>(type))) return {};
  uint32_t sender{0};
  if(!headerless(static_cast<message_type>(type))) {
    input.seek(16);
    sender = input.u32();
  }
  if(!headerless(static_cast<message_type>(type)) && sender >= 16 && !(type == 2 && sender == UINT32_MAX)) return {};
  if((type == 26 && source[24] > 63) || (type == 27 && source[24] > 28)) return {};
  packet result{static_cast<message_type>(type), sender};
  result.storage.assign(source.begin(), source.end());
  return result;
}
message_type packet::type() const {
  return static_cast<message_type>(word(0));
}
uint32_t packet::sender() const {
  return headerless(type()) ? 0 : word(16);
}
std::string packet::name() const {
  if(headerless(type())) return {};
  std::string result;
  for(size_t i{4}; i < 12 && storage[i]; ++i) result.push_back(static_cast<char>(storage[i]));
  return result;
}
std::span<uint8_t const> packet::bytes() const {
  return storage;
}
uint32_t packet::word(size_t const offset) const {
  resources::reader input{storage};
  input.seek(offset);
  return input.u32();
}
void packet::word(size_t const offset, uint32_t const value) {
  if(offset > storage.size() || storage.size() - offset < 4) throw std::runtime_error{"Multiplayer field outside packet"};
  for(size_t i{0}; i < 4; ++i) storage[offset + i] = static_cast<uint8_t>(value >> (8 * i));
}
void packet::copy(size_t const offset, std::span<uint8_t const> const source) {
  if(offset > storage.size() || source.size() > storage.size() - offset) throw std::runtime_error{"Multiplayer payload outside packet"};
  std::ranges::copy(source, storage.begin() + static_cast<ptrdiff_t>(offset));
}
void packet::pose(math::pose const &value) {
  for(size_t i{0}; i < 3; ++i) word(24 + i * 4, static_cast<uint32_t>(value.position[i]));
  for(size_t i{0}; i < 9; ++i) word(36 + i * 4, static_cast<uint32_t>(value.rotation[i]));
}
math::pose packet::pose() const {
  math::pose value;
  for(size_t i{0}; i < 3; ++i) value.position[i] = signed_word(word(24 + i * 4));
  for(size_t i{0}; i < 9; ++i) value.rotation[i] = signed_word(word(36 + i * 4));
  return value;
}
packet identification(message_type const type, uint32_t const sender, std::string_view const name, int32_t const model) {
  packet result{type, sender, name};
  result.word(76, static_cast<uint32_t>(model));
  return result;
}
packet nomination(uint32_t const sender, std::string_view const name, double const clock) {
  packet result{message_type::nominate_server, sender, name};
  auto const bits{std::bit_cast<uint64_t>(clock)};
  result.word(80, static_cast<uint32_t>(bits));
  result.word(84, static_cast<uint32_t>(bits >> 32));
  return result;
}
packet two_player_settings(std::array<int32_t, 5> const &values) {
  packet result{message_type::two_player_settings, 0};
  for(size_t i{0}; i < values.size(); ++i) result.word(4 + i * 4, static_cast<uint32_t>(values[i]));
  return result;
}
packet chat(uint32_t const sender, std::string_view const name, std::string_view const text) {
  packet result{message_type::chat, sender, name};
  std::array<uint8_t, 80> message{};
  for(size_t i{0}; i < std::min(size_t{79}, text.size()) && text[i]; ++i) message[i] = static_cast<uint8_t>(text[i]);
  result.copy(24, message);
  return result;
}
packet crew(uint32_t const sender, std::string_view const name, std::span<uint8_t const, 104> const payload) {
  packet result{message_type::crew, sender, name};
  result.copy(24, payload);
  return result;
}
packet kills(uint32_t const sender, std::string_view const name, bool const scenery, uint8_t const count, std::span<uint8_t const> const records) {
  if(count > (scenery ? 28 : 63) || records.size() != (scenery ? 336u : 252u)) throw std::runtime_error{"Invalid multiplayer casualty batch"};
  packet result{scenery ? message_type::scenery_kills : message_type::mobile_kills, sender, name};
  result.word(24, count);
  result.copy(scenery ? 28 : 26, records);
  return result;
}
packet flag_won(uint32_t const sender, std::string_view const name) {
  packet result{message_type::flag_won, sender, name};
  result.word(400, 1);
  return result;
}
packet preferences(uint32_t const sender, std::array<int32_t, 5> const &values) {
  packet result{message_type::preferences, sender};
  for(size_t i{0}; i < values.size(); ++i) result.word(24 + i * 4, static_cast<uint32_t>(values[i]));
  return result;
}
packet weather(uint32_t const sender, std::string_view const name, int32_t const strength, int32_t const direction) {
  packet result{message_type::weather, sender, name};
  result.word(68, static_cast<uint32_t>(strength));
  result.word(72, static_cast<uint32_t>(direction));
  return result;
}
packet position(uint32_t const sender, math::pose location, int32_t const speed, target_state const &target) {
  packet result{message_type::position, sender};
  location.position[2] = math::subtract(location.position[2], 0x900);
  result.pose(location);
  result.word(72, static_cast<uint32_t>(speed));
  result.word(392, target.kind);
  size_t const count{target.kind == 1 ? 3u : target.kind == 2 ? 2u : 0u};
  for(size_t i{0}; i < count; ++i) result.word(396 + i * 4, static_cast<uint32_t>(target.data[i]));
  return result;
}
packet restarted(uint32_t const sender, std::string_view const name, math::pose const &location) {
  packet result{message_type::restarted, sender, name};
  result.pose(location);
  return result;
}
packet client_state(uint32_t const sender, math::pose const &location, int32_t const speed, double const frame_delta) {
  packet result{message_type::client_state, sender};
  result.pose(location);
  result.word(72, static_cast<uint32_t>(speed));
  auto const bits{std::bit_cast<uint64_t>(frame_delta)};
  result.word(80, static_cast<uint32_t>(bits));
  result.word(84, static_cast<uint32_t>(bits >> 32));
  return result;
}
packet server_state(std::array<aircraft_state, 16> const &players) {
  packet result{message_type::server_state, 0};
  for(size_t i{0}; i < players.size(); ++i) {
    auto const &player{players[i]};
    std::array<uint8_t, 32> record{};
    auto const put{[&](size_t offset, uint32_t value, size_t count){
      for(size_t j{0}; j < count; ++j) record[offset + j] = static_cast<uint8_t>(value >> (j * 8));
    }};
    for(size_t j{0}; j < 3; ++j) put(j * 3, static_cast<uint32_t>(player.location.position[j] >> 4), 3);
    put(9, static_cast<uint32_t>(player.speed), 3);
    put(12, static_cast<uint32_t>(player.model), 2);
    for(size_t j{0}; j < 9; ++j) put(14 + j * 2, static_cast<uint32_t>(player.location.rotation[j]) >> 16, 2);
    result.copy(4 + i * 32, record);
  }
  return result;
}
std::array<aircraft_state, 16> server_state(packet const &message) {
  if(message.type() != message_type::server_state) throw std::runtime_error{"Expected a multiplayer server-state packet"};
  std::array<aircraft_state, 16> result;
  for(size_t i{0}; i < result.size(); ++i) {
    auto const record{message.bytes().subspan(4 + i * 32, 32)};
    auto const get{[&](size_t offset, size_t count){
      uint32_t value{0};
      for(size_t j{0}; j < count; ++j) value |= uint32_t{record[offset + j]} << (j * 8);
      return value;
    }};
    auto &player{result[i]};
    for(size_t j{0}; j < 3; ++j) player.location.position[j] = static_cast<int32_t>(get(j * 3, 3) << 4);
    player.speed = signed_word(get(9, 3) << 8) >> 8;
    player.model = signed_word(get(12, 2) << 16) >> 16;
    for(size_t row{0}; row < 3; ++row) {
      math::vector3 vector;
      for(size_t j{0}; j < 3; ++j) vector[j] = signed_word(get(14 + (row * 3 + j) * 2, 2) << 16) >> 16;
      vector = math::normalise_q31(vector);
      for(size_t j{0}; j < 3; ++j) player.location.rotation[row * 3 + j] = vector[j];
    }
  }
  return result;
}
packet scores(uint32_t const sender, std::string_view const name, std::array<int32_t, 16> const &values) {
  packet result{message_type::scores, sender, name};
  for(size_t i{0}; i < values.size(); ++i) result.word(24 + i * 4, static_cast<uint32_t>(values[i]));
  return result;
}
packet fired(uint32_t const sender, std::string_view const name, uint32_t const weapon, uint32_t const shot_seed, target_state const &target, math::vector3 const &gun_direction) {
  packet result{message_type::fired, sender, name};
  result.word(96, weapon);
  result.word(112, shot_seed);
  result.word(392, target.kind);
  size_t const count{weapon == 1 || target.kind == 1 ? 3u : target.kind == 2 ? 2u : 0u};
  for(size_t i{0}; i < count; ++i) result.word(396 + i * 4, static_cast<uint32_t>(weapon == 1 ? gun_direction[i] : target.data[i]));
  return result;
}
packet event(message_type const type, uint32_t const sender, std::string_view const name, int32_t const first, int32_t const second) {
  packet result{type, sender, name};
  result.word(396, static_cast<uint32_t>(first));
  result.word(400, static_cast<uint32_t>(second));
  return result;
}
packet menu_selection(uint32_t const sender, std::string_view const name, uint32_t const selection, std::array<int32_t, 5> const &arguments) {
  packet result{message_type::menu_selection, sender, name};
  result.word(44, selection);
  for(size_t i{0}; i < arguments.size(); ++i) result.word(48 + i * 4, static_cast<uint32_t>(arguments[i]));
  return result;
}
packet initial_state(uint32_t const sender, std::string_view const name, math::vector3 const &position, uint32_t const war_seed, uint32_t const weapon_seed) {
  packet result{message_type::initial_state, sender, name};
  for(size_t i{0}; i < 3; ++i) result.word(24 + i * 4, static_cast<uint32_t>(position[i]));
  result.word(88, war_seed);
  result.word(92, weapon_seed);
  return result;
}
} // namespace apache::multiplayer
