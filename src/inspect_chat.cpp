#include <algorithm>
#include <iomanip>
#include <istream>
#include <ostream>
#include <stdexcept>
#include "frontend/multiplayer_chat.h"
#include "inspect_network.h"

namespace apache {
void inspect_chat(resources::data_store const &data, std::istream &input, std::ostream &out) {
  resources::executable_data executable{data};
  frontend::multiplayer_chat chat{executable};
  render::framebuffer frame{640, 480};
  int32_t clock{0};
  std::string sent;
  chat.send = [&](std::string_view text){
    sent = text;
    chat.add("> PLAYER: " + std::string{text}, clock);
  };
  std::string command;
  while(input >> command) {
    if(command == "clock") {
      input >> clock;
    } else if(command == "begin") {
      chat.begin();
    } else if(command == "key") {
      unsigned int scan;
      input >> scan;
      chat.key(static_cast<uint16_t>(scan), clock);
    } else if(command == "add") {
      std::string text;
      input >> std::quoted(text);
      chat.add(text, clock);
    } else if(command == "command") {
      std::string text;
      input >> std::quoted(text);
      chat.command(text, clock);
    } else if(command == "size") {
      unsigned int width;
      input >> width;
      frame = render::framebuffer{width, width * 3 / 4};
    } else if(command == "draw") {
      std::ranges::fill(frame.image.pixels, 100);
      chat.draw(frame, clock);
    } else if(command == "snapshot") {
      out << chat.writing << ' ' << chat.empty << ' ' << chat.cursor << ' ' << chat.previous_cursor << ' ' << chat.lines << ' ' << chat.scroll_seconds << ' ' << chat.blank_seconds << ' ' << chat.shadow << ' ' << chat.colour << ' ' << chat.language << ' ' << chat.last_message << ' ' << chat.scroll_time << ' ' << std::quoted(chat.text()) << ' ' << std::quoted(std::string{chat.previous.data()}) << ' ' << std::quoted(sent);
      for(auto const &message : chat.messages) out << ' ' << std::quoted(message);
      out << ' ' << resources::fingerprint(frame.image.pixels) << ' ' << resources::fingerprint(chat.terrain_colours) << '\n';
    } else {
      throw std::runtime_error{"Unknown chat check command"};
    }
  }
}
} // namespace apache
