#include "communications.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace apache::simulation {
bool communications::add(std::string_view const text, int32_t const priority, int32_t const lifetime) {
  if(lifetime <= 0) return false;
  auto const free{std::ranges::find_if(messages, [](auto const &m){return m.lifetime <= 0;})};
  if(free == messages.end()) return false;
  *free = {
    .text{std::string{text}},
    .lifetime{lifetime},
    .priority{priority}
  };
  return true;
}
bool communications::add_changing(uint32_t const identity, std::string_view const text, int32_t const priority) {
  auto const free{std::ranges::find_if(messages, [](auto const &m){return m.lifetime <= 0;})};
  if(free == messages.end()) return false;
  *free = {
    .text{std::string{text}},
    .lifetime{1},
    .priority{priority},
    .displayed{false},
    .changing{true},
    .identity{identity}
  };
  return true;
}
void communications::update_changing(uint32_t const identity, std::string_view const text) {
  for(auto &message : messages) {
    if(message.changing && message.identity == identity) message.text = text;
  }
}
void communications::remove(uint32_t const identity) {
  for(auto &message : messages) {
    if(message.changing && message.identity == identity) {
      message.lifetime = 0;
      message.displayed = message.changing = false;
      message.identity = 0;
      return;
    }
  }
}
void communications::advance(double const seconds) {
  if(!std::isfinite(seconds) || seconds < 0 || seconds > 3600) throw std::runtime_error{"Invalid communications interval"};
  auto const ticks{std::max(static_cast<int32_t>(seconds * 200.0), 1)};
  for(auto &message : messages) {
    if(message.displayed && !message.changing) {
      message.lifetime = std::max(message.lifetime - ticks, 0);
      if(message.lifetime == 0) message.displayed = false;
    }
  }
}
std::array<std::string, 3> const &communications::compose(communication_status const &status) {
  if(status.faults & 0x20000) {
    for(auto &line : lines) {
      line.clear();
      for(unsigned int i{0}; i != 17; ++i) {
        damaged_seed = damaged_seed * 0x41c64e6du + 0x3039u;
        line.push_back(static_cast<char>(' ' + ((damaged_seed >> 16) & 0x7fff) % 59));
      }
    }
    return lines;
  }
  std::vector<size_t> order;
  for(size_t i{0}; i != messages.size(); ++i) {
    if(messages[i].lifetime > 0) {
      // The original linked-list insertion puts equal-priority arrivals after
      // the first message at that priority, ahead of its remaining peers.
      size_t at{0};
      if(!order.empty() && messages[i].priority <= messages[order.front()].priority) {
        while(at + 1 < order.size() && messages[i].priority < messages[order[at]].priority) ++at;
        if(messages[order[at]].priority >= messages[i].priority) ++at;
      }
      order.insert(order.begin() + static_cast<ptrdiff_t>(at), i);
    }
  }
  if(order.empty()) {
    char text[96];
    if(status.flow == 1) {
      lines[0] = "    QUICKSTART";
      std::snprintf(text, sizeof(text), "SCORE %d", status.score);
      std::string const score{text};
      lines[1] = std::string(score.size() < 17 ? (17 - score.size()) / 2 : 0, ' ') + score;
      return lines;
    }
    if(status.infinite_fuel) {
      lines[0] = "FUEL: INFINITE";
      lines[1] = "FLY TIME:  ---:--";
    } else {
      std::snprintf(text, sizeof(text), "FUEL:       %05d", status.fuel);
      lines[0] = text;
      std::snprintf(text, sizeof(text), "FLY TIME:  %3d:%02d", status.endurance / 60, status.endurance % 60);
      lines[1] = text;
    }
    lines[2] = "WPN:" + std::string(status.weapon.size() < 13 ? 13 - status.weapon.size() : 0, ' ') + std::string{status.weapon};
    return lines;
  }
  lines = {};
  for(size_t i{0}; i != order.size(); ++i) {
    auto &message{messages[order[i]]};
    message.displayed = i < lines.size();
    if(message.displayed) lines[i] = message.text;
  }
  return lines;
}
} // namespace apache::simulation
