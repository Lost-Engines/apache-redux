#include "missions.h"
#include <algorithm>
#include <charconv>
#include <stdexcept>

namespace apache::resources {
namespace {
std::string_view trim(std::string_view value) {
  while(!value.empty() && (value.front() == ' ' || value.front() == '\t' || value.front() == '\r')) value.remove_prefix(1);
  while(!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r')) value.remove_suffix(1);
  return value;
}
std::string canonical(std::string_view const text) {
  std::string result{text}; for(char &letter : result) if(letter >= 'a' && letter <= 'z') letter = static_cast<char>(letter - 'a' + 'A'); return result;
}
std::vector<int32_t> numbers(std::string_view text) {
  std::vector<int32_t> result;
  while(!text.empty()) {
    while(!text.empty() && (text.front() == ',' || text.front() == ':' || text.front() == ' ' || text.front() == '\t' || text.front() == '\r')) text.remove_prefix(1);
    if(text.empty()) break;
    if(text.front() == '+') text.remove_prefix(1);
    int32_t number{0}; auto const [last, error]{std::from_chars(text.data(), text.data() + text.size(), number)};
    if(error != std::errc{}) throw std::runtime_error{"Invalid mission integer"};
    result.push_back(number); text.remove_prefix(static_cast<size_t>(last - text.data()));
    if(!text.empty() && text.front() != ',' && text.front() != ':' && text.front() != ' ' && text.front() != '\t' && text.front() != '\r') throw std::runtime_error{"Invalid mission number delimiter"};
  }
  return result;
}

int32_t single(tagged_section const &section) {
  if(section.lines.size() != 1) throw std::runtime_error{"Expected one value for mission tag " + section.tag};
  auto const values{numbers(section.lines.front())};
  if(values.size() != 1) throw std::runtime_error{"Expected one integer for mission tag " + section.tag};
  return values.front();
}
} // anonymous namespace
std::vector<tagged_section> tagged_text(std::string_view text) {
  std::vector<tagged_section> result;
  while(!text.empty()) {
    auto const end{text.find('\n')}; auto line{text.substr(0, end)};
    if(!line.empty() && line.back() == '\r') line.remove_suffix(1);
    if(!trim(line).empty() && line.front() != '#') {
      if(line.front() == ':') {
        if(canonical(trim(line.substr(1))) == "EOF") break;
        result.push_back({std::string{trim(line.substr(1))}, {}});
      } else {
        if(result.empty()) throw std::runtime_error{"Mission text precedes its first tag"};
        result.back().lines.emplace_back(line);
      }
    }
    if(end == std::string_view::npos) break;
    text.remove_prefix(end + 1);
  }
  return result;
}
std::vector<mission_briefing> read_mission_briefings(std::span<uint8_t const> bytes) {
  reader in{bytes, "mission briefings"}; std::vector<mission_briefing> result;
  for(;;) {
    auto const id{in.i32()};
    if(id == -1) break;
    mission_briefing mission; mission.id = id; mission.text = tagged_text(in.text(in.u32()));
    for(auto const &section : mission.text) if(canonical(section.tag) == "TITLE") for(auto const &line : section.lines) { if(!mission.title.empty()) mission.title += ' '; mission.title += line; }
    for(auto const &section : tagged_text(in.text(in.u32()))) {
      if(canonical(section.tag) == "GAME FLOW") mission.flow = single(section);
      else if(canonical(section.tag) == "WAR ZONE") mission.zone = single(section);
      else if(canonical(section.tag) == "CAMPAIGN LEVEL") mission.level = single(section);
      else throw std::runtime_error{"Unknown mission briefing metadata: " + section.tag};
    }
    result.push_back(std::move(mission));
  }
  if(in.remaining()) in.fail("Trailing mission bundle data");
  return result;
}
mission_definition read_mission_definition(std::string_view const text) {
  mission_definition result; mission_task *task{nullptr}; mission_actor *actor{nullptr};
  auto const need_task{[&]()->mission_task&{ if(!task) throw std::runtime_error{"Mission tag requires a task"}; return *task; }};
  auto const need_actor{[&]()->mission_actor&{ if(!actor) throw std::runtime_error{"Mission tag requires an actor"}; return *actor; }};
  for(auto const &section : tagged_text(text)) {
    auto const tag{canonical(section.tag)};
    if(tag == "TASK") { task = &result.tasks.emplace_back(); actor = nullptr; } else if(tag == "USER TASK") need_task().user = true;
    else if(tag == "SIDE") need_task().side = single(section);
    else if(tag == "TASK TYPE") need_task().type = single(section);
    else if(tag == "BATTLE CELL") need_task().battle_cell = single(section);
    else if(tag == "DRONE") actor = &need_task().actors.emplace_back();
    else if(tag == "DRONE TYPE") need_actor().model = static_cast<uint32_t>(single(section));
    else if(tag == "FORMATION TYPE") need_actor().formation = static_cast<uint32_t>(single(section));
    else if(tag == "DRONE START TIME") need_actor().start_time = single(section);
    else if(tag == "START WAYPOINT") need_actor().start_waypoint = static_cast<uint32_t>(single(section));
    else if(tag == "KILL FAILURE") need_actor().conditions |= 0x1000;
    else if(tag == "KILL SUCCESS") need_actor().conditions |= 0x2000;
    else if(tag == "KILL OUTSTANDING") need_actor().conditions |= 0x4000;
    else if(tag == "LASER DESIGNATED") need_actor().conditions |= 1;
    else if(tag == "DRONE START") {
      if(section.lines.size() != 1) throw std::runtime_error{"Invalid actor position"};
      auto const values{numbers(section.lines.front())}; if(values.size() != 7) throw std::runtime_error{"Invalid actor position fields"};
      std::array<int32_t, 7> state; std::copy(values.begin(), values.end(), state.begin()); need_actor().initial_state = state;
    } else if(tag == "WAYPOINT") {
      for(auto const &line : section.lines) {
        auto const v{numbers(line)}; if(v.size() < 9) throw std::runtime_error{"Incomplete waypoint"};
        need_actor().waypoints.push_back({v[0], {v[1], v[2], v[3]}, v[4], v[5], v[6], v[7]});
      }
    } else {
      auto &rows{result.settings[tag]};
      for(auto const &line : section.lines) rows.push_back(numbers(line));
      if(tag == "WIND SPEED") result.wind_strength = single(section);
      else if(tag == "WIND DIRECTION") result.wind_direction = static_cast<uint32_t>(single(section)) * 0xb60b61u;
      else if(tag == "WEATHER CONDITION") result.weather = single(section);
      else if(tag == "LASER TIMEOUT") result.laser_timeout = single(section);
      else if(tag == "INFINITE WEAPONS") result.infinite_weapons = single(section) != 0;
      else if(tag == "GUN") result.gun = single(section);
      else if(tag == "FAILURE COUNT") result.failure_count = single(section);
      else if(tag == "SUCCESS COUNT") result.success_count = single(section);
      else if(tag == "OUTSTANDING COUNT") result.outstanding_count = single(section);
      else if(tag == "TIME") {
        if(rows.size() != 1 || rows[0].size() != 2) throw std::runtime_error{"Invalid mission time"};
        result.time_of_day = std::bit_cast<int32_t>(static_cast<uint32_t>(rows[0][0]) * 720000u + static_cast<uint32_t>(rows[0][1]) * 12000u);
      } else if(tag == "WEAPON") {
        for(auto const &row : rows) { if(row.size() != 2 || row[0] < 0 || row[0] >= 9) throw std::runtime_error{"Invalid weapon station"}; result.weapons[static_cast<size_t>(row[0])] = row[1]; }
      } else if(tag == "DESTROY") {
        for(auto const &row : rows) { if(row.size() != 2) throw std::runtime_error{"Invalid destruction marker"}; result.destroy_markers.push_back({row[0], row[1]}); }
      }
    }
  }
  return result;
}
std::vector<std::vector<formation_member>> read_formations(executable_data const &executable) {
  reader pointers{executable.at(0x4e017c, 89 * 4), "formation pointers"}; std::vector<std::vector<formation_member>> result(89);
  for(auto &members : result) {
    auto const pointer{pointers.u32()}; if(!pointer) continue;
    reader header{executable.at(pointer, 4), "formation count"}; auto const count{header.u32()};
    if(count > 100) throw std::runtime_error{"Invalid formation size"};
    reader entries{executable.at(pointer + 4, count * 16), "formation members"};
    for(uint32_t i{0}; i != count; ++i) members.push_back({{entries.i32(), entries.i32(), entries.i32()}, entries.u32()});
  }
  return result;
}
} // namespace apache::resources
