#include "multiplayer_chat.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include "math/fixed.h"

namespace apache::frontend {
namespace {
std::string string_at(resources::executable_data const &exe, uint32_t address, size_t limit = 100) {
  auto const data{exe.at(address, limit)};
  auto const end{std::ranges::find(data, uint8_t{0})};
  return {data.begin(), end};
}
int integer(std::string const &value) {
  // Command values have the original atoi syntax, with bounded conversion.
  char *end; auto const number{std::strtoll(value.c_str(), &end, 10)};
  return static_cast<int>(std::clamp(number, static_cast<long long>(INT32_MIN), static_cast<long long>(INT32_MAX)));
}
} // anonymous namespace
multiplayer_chat::multiplayer_chat(resources::executable_data const &exe) : font{exe, true} {
  auto const keys{exe.at(0x4ecc24, ascii.size())}; std::ranges::copy(keys, ascii.begin());
  for(size_t i{0}; i < commands.size(); ++i) {
    auto const address{0x4e062cu + static_cast<uint32_t>(i * 0x144)}; auto &c{commands[i]};
    c.name = string_at(exe, address, 12); resources::reader fields{exe.at(address + 12, 12)};
    c.arguments = fields.u32(); fields.skip(4); c.listed = fields.u32() != 0;
    for(size_t j{0}; j < 3; ++j) c.help[j] = string_at(exe, address + 24 + static_cast<uint32_t>(j * 100));
  }
  resources::reader names{exe.at(0x4379c0, 9 * 4)};
  for(auto &name : languages) name = string_at(exe, names.u32());
  std::ranges::copy(exe.at(0x4ef37c, 96), terrain_colours.begin());
  for(size_t i{0}; i < 2; ++i) std::ranges::copy(exe.at(i ? 0x4ef3dc : 0x4ef31c, 96), terrain_presets[i].begin());
  resources::reader v{exe.at(0x4d9910, 4)}; version = string_at(exe, v.u32());
}
std::string_view multiplayer_chat::text() const {
  return {entry.data(), static_cast<size_t>(std::ranges::find(entry, '\0') - entry.begin())};
}
void multiplayer_chat::begin() {
  std::fill_n(entry.begin(), 78, ' ');
  entry[78] = 0;
  writing = true;
}
void multiplayer_chat::scroll() {
  for(unsigned int i{1}; i < lines; ++i) messages[i - 1] = messages[i];
}
void multiplayer_chat::add(std::string_view const value, int32_t const now) {
  scroll(); messages[lines - 1] = value.substr(0, 99); empty = false; last_message = scroll_time = now;
}
void multiplayer_chat::key(uint16_t const scan, int32_t const now) {
  if(!writing) return;
  auto const code{scan & 255};
  if(code == 1) { writing = false; cursor = 0; return; }
  if(code == 0x1c) {
    previous = entry; previous_cursor = cursor; cursor = 0; writing = false;
    if(entry[0] == '/') command(text(), now); else if(send) send(text());
    return;
  }
  if(code == 0x48) { std::copy_n(previous.begin(), previous_cursor, entry.begin()); cursor = previous_cursor; return; }
  if(code == 0xe || code == 0x53) { if(cursor) --cursor; entry[cursor] = ' '; return; }
  unsigned int value{0};
  if(!(scan & 0x80) && !(scan & 0x1000)) { value = ascii[(scan & 127) + ((scan & 0x4000) ? 128 : 0)]; if(scan & 0x2000) value &= 31; }
  if(cursor < 80 && value >= 32 && value < 128) entry[cursor++] = static_cast<char>(value);
}
void multiplayer_chat::command(std::string_view const text, int32_t const now) {
  std::istringstream stream{std::string{text}}; std::string name; stream >> name;
  std::array<std::string, 4> arguments; for(auto &arg : arguments) stream >> arg;
  auto const found{std::ranges::find(commands, name.substr(1), &command_info::name)};
  if(found == commands.end()) { add("Unknown command", now); return; }
  if(found->arguments && arguments[std::min(found->arguments, 4u) - 1].empty()) { add("Not enough arguments", now); return; }
  auto const value{integer(arguments[0])};
  if(name == "/lines") {
    if(value < 1 || value > 24) add("Number of message lines has to be between 1 and 24", now);
    else { lines = static_cast<unsigned int>(value); add("Number of message lines set to: " + std::to_string(value), now); }
  } else if(name == "/scroll") {
    if(value < 1 || value >= 60) add("Scroll delay has to be between 0 and 60 seconds", now);
    else { scroll_seconds = static_cast<unsigned int>(value); add("Scroll delay set to: " + std::to_string(value), now); }
  } else if(name == "/blank") {
    if(value < 1 || value >= 360) add("Blank time has to be between 0 and 360 seconds", now);
    else { blank_seconds = static_cast<unsigned int>(value); add("Blank time set to: " + std::to_string(value), now); }
  } else if(name == "/text") {
    auto const setting{integer(arguments[1])};
    if(arguments[0] == "colour") { if(setting >= 1 && setting <= 255) { colour = static_cast<unsigned int>(setting); add("Message text colour set to palette number: " + std::to_string(setting), now); } } else if(arguments[0] == "shadow") { if(setting >= 0 && setting <= 8) { shadow = static_cast<unsigned int>(setting); add("Message shadow level set to : " + std::to_string(setting), now); } } else add(arguments[0] + " is not a correct argument for the text command.", now);
  } else if(name == "/help") {
    if(arguments[0].empty()) {
      add("Commands available are:", now);
      std::string text;
      for(size_t i{0}; i < 8; ++i) { if(commands[i].listed) text += commands[i].name + (i == 7 ? "." : ", "); if(i == 4 || i == 7) { add(text, now); text.clear(); } }
      add("To get help about a command type: /help command_name", now);
    } else if(auto const command{std::ranges::find(commands, arguments[0], &command_info::name)}; command != commands.end()) for(auto const &line : command->help) add(line, now);
  } else if(name == "/version") {
    add(languages.at(language) + " version " + version + " Dated Jan 14 1998 at 17:03:57", now);
  } else if(name == "/language") {
    static std::array<std::pair<std::string_view, unsigned int>, 5> constexpr choices{{{"english",1},{"american",2},{"french",3},{"german",4},{"italian",6}}};
    auto const found{std::ranges::find_if(choices, [&](auto const &choice){ return choice.first == arguments[0]; })};
    if(found == choices.end()) add("Set Language to : english, american, french, german or italian", now);
    else { language = found->second; auto label{arguments[0]}; label[0] = static_cast<char>(label[0] - 'a' + 'A'); add("Language set to " + label + ".", now); if(change_language) change_language(language); }
  } else if(name == "/terrain") terrain_command(arguments, now);
}
void multiplayer_chat::terrain_command(std::array<std::string, 4> const &arguments, int32_t const now) {
  auto const &name{arguments[0]};
  if(name == "print") {
    lines = 20; scroll_seconds = 60; blank_seconds = 1000;
    for(size_t i{0}; i < 48; i += 3) {
      std::array<char, 100> text;
      std::snprintf(text.data(), text.size(), "r1 = %d, g1 = %d, b1 = %d         r2 = %d, g2 = %d, b2= %d", terrain_colours[i], terrain_colours[i + 1], terrain_colours[i + 2], terrain_colours[i + 48], terrain_colours[i + 49], terrain_colours[i + 50]);
      add(text.data(), now);
    }
    return;
  }
  if(name == "andys" || name == "nicks") terrain_colours = terrain_presets[name == "nicks"];
  else {
    int bank{0}; bool top{false};
    if(name == "top" || name == "top1" || name == "top2") top = true;
    else if(name != "bottom" && name != "bottom1" && name != "bottom2") { add(name + " is not a correct argument for the text command.", now); return; }
    if(name.ends_with('1')) bank = -1; else if(name.ends_with('2')) bank = 1;
    std::array<int, 3> first{}, last{};
    for(size_t c{0}; c < 3; ++c) {
      auto const value{integer(arguments[c + 1]) % 256};
      first[c] = top ? terrain_colours[(bank == 1 ? 48 : 0) + c] : value;
      // The second bank's bottom command takes its upper colour from entry 16.
      last[c] = top ? value : terrain_colours[(bank == 1 ? 48 : 45) + c];
    }
    std::array<float, 3> rgb{}, step{};
    for(size_t c{0}; c < 3; ++c) { rgb[c] = static_cast<float>(first[c]); step[c] = static_cast<float>(last[c] - first[c]) * .0625f; }
    for(size_t i{0}; i < 48; i += 3) for(size_t c{0}; c < 3; ++c) {
      auto const value{static_cast<uint8_t>(static_cast<int>(rgb[c]))};
      if(bank <= 0) terrain_colours[i + c] = value;
      if(bank >= 0) terrain_colours[i + c + 48] = value;
      rgb[c] += step[c];
    }
  }
  if(change_terrain) change_terrain(terrain_colours);
}
void multiplayer_chat::draw(render::framebuffer &frame, int32_t const now) {
  frame.reset_clip();
  if(writing) {
    int const x{font.width(text().substr(0, cursor)) + 20}, y{static_cast<int>(lines * 10)};
    font.draw(frame, "_", x, y + 8, 1); font.draw(frame, "_", x + 1, y + 9, 0);
    auto const value{":- " + std::string{text()}};
    for(auto const &[dx, dy] : std::array<std::array<int, 2>, 8>{{{0,0},{2,0},{1,1},{1,-1},{0,-1},{0,1},{2,1},{2,-1}}}) font.draw(frame, value, dx, y + 5 + dy, 0);
    font.draw(frame, value, 1, y + 5, 1);
  }
  if(empty || math::subtract(now, last_message) > static_cast<int32_t>(blank_seconds * 200)) {
    // clear_all_messages_ deliberately leaves the final history row alone.
    for(unsigned int i{0}; i + 1 < lines; ++i) messages[i] = std::string(78, ' ');
    return;
  }
  if(math::subtract(now, scroll_time) > static_cast<int32_t>(scroll_seconds * 200)) { scroll(); messages[lines - 1] = "> "; scroll_time = now; }
  static std::array<std::array<int, 2>, 8> constexpr offsets{{{2,1},{0,-1},{2,-1},{0,1},{1,1},{1,-1},{0,0},{2,0}}};
  for(unsigned int i{0}; i < lines; ++i) {
    auto const y{static_cast<int>(i * 10)};
    for(unsigned int j{0}; j < shadow; ++j) font.draw(frame, messages[i], offsets[j][0], y + offsets[j][1], 0);
    font.draw(frame, messages[i], 2, y + 1, 0); font.draw(frame, messages[i], 1, y, static_cast<uint8_t>(colour));
  }
}
} // namespace apache::frontend
