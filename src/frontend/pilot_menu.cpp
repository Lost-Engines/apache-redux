#include "pilot_menu.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include "resources/executable.h"

namespace apache::frontend {
namespace {
uint32_t hours_now() {
  return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::hours>(std::chrono::system_clock::now().time_since_epoch()).count());
}
std::string label(resources::executable_data const &data, uint32_t const table, unsigned int const index) {
  resources::reader pointer{data.at(table + index * 4, 4), "pilot label"};
  auto const address{pointer.u32()};
  std::string result;
  for(uint32_t i{0}; i < 128; ++i) {
    auto const ch{data.at(address + i, 1)[0]};
    if(!ch) return result;
    result += static_cast<char>(ch);
  }
  throw std::runtime_error{"Unterminated pilot label"};
}
std::string duration(uint32_t ticks) {
  ticks /= 200;
  char text[48];
  std::snprintf(text, sizeof(text), "%02u:%02u:%02u", ticks / 3600, ticks / 60 % 60, ticks % 60);
  return text;
}
} // anonymous namespace
pilot_menu::pilot_menu(resources::data_store const &data, menu &ui, configuration::profiles &logs, std::span<resources::mission_briefing const> const definitions)
  : interface{ui},
    profiles{logs},
    missions{definitions} {
  resources::executable_data const executable{data};
  for(unsigned int i{0}; i != ranks.size(); ++i) ranks[i] = label(executable, 0x4d9fcc, i);
  for(unsigned int i{0}; i != statuses.size(); ++i) statuses[i] = label(executable, 0x4da004, i);
}
void pilot_menu::visible(std::string_view const name, bool const show) {
  auto &object{interface.named(name)};
  object.flags = (object.flags & ~1u) | static_cast<unsigned int>(show);
}
void pilot_menu::update() {
  auto &group{profiles.users.at(profiles.user)};
  auto &pilot{profiles.current()};
  size_t user{0};
  for(auto &object : interface.objects()) {
    if(object.action == "notify_log_users_") {
      object.text = profiles.users[user].name;
      object.on = user == profiles.user;
      ++user;
    }
  }
  auto &entries{interface.named("_log_entries")};
  entries.text.clear();
  entries.selected_line = static_cast<int>(group.selected);
  for(auto const &slot : group.pilots) {
    if(!entries.text.empty()) entries.text += '\n';
    entries.text += ranks.at(slot.log.rank);
    if(!slot.log.name.empty()) entries.text += " " + slot.log.name;
  }
  visible("_log_create_button", group.pilots.size() < 10);
  std::array<std::string_view, 3> constexpr medals{"_log_medal_honor", "_log_medal_cross", "_log_medal_bstar"};
  for(size_t i{0}; i != medals.size(); ++i) visible(medals[i], pilot.medals[i] != 0);
  pilot.update(missions);
  interface.named("_log_detail_heading").text = "Status:\nTotal time:\nCombat time:\nTotal missions:\nOutstanding missions:\nSuccessful missions:\nFailed missions:\nCurrent Campaign:\nCampaigns Won:";
  auto &detail{interface.named("_log_detail")};
  detail.text = statuses.at(pilot.status) + "\n" + duration(pilot.total_time) + "\n" + duration(pilot.combat_time) + "\n" + std::to_string(pilot.completed) + "\n" + std::to_string(pilot.outstanding) + "\n" + std::to_string(pilot.successes) + "\n" + std::to_string(pilot.failures) + "\n";
  std::array<std::string, 4> const zones{"No Campaign", "Yemen", "Korea", "Cyprus"};
  detail.text += zones.at(pilot.campaign.warzone);
  if(pilot.campaign.warzone) detail.text += " level " + std::to_string(pilot.campaign.level);
  detail.text += '\n';
  bool separator{false};
  for(size_t i{3}; i-- > 0;) {
    if(pilot.victories[i]) {
      if(separator) detail.text += ", ";
      detail.text += zones[i + 1];
      separator = true;
    }
  }
}
void pilot_menu::show() {
  editing = 0;
  visible("_log_screen_rename_box", false);
  visible("_log_screen_error_box", false);
  profiles.select_user(profiles.user, hours_now());
  update();
  interface.show("_log_screen");
}
void pilot_menu::error(std::string text) {
  editing = 0;
  visible("_log_screen_rename_box", false);
  visible("_log_screen_error_box", true);
  interface.named("_log_screen_error_text").text = std::move(text);
}
void pilot_menu::rename(unsigned int const kind, std::string_view const title) {
  editing = kind;
  visible("_log_screen_error_box", false);
  visible("_log_screen_rename_box", true);
  interface.named("_log_screen_rename_title").text = title;
  interface.named("_log_screen_rename_text").text = kind == 2 ? profiles.users[profiles.user].name : profiles.current().name;
}
bool pilot_menu::activate(widget &object, int const line) {
  auto const &action{object.action};
  if(action == "notify_log_button_" || action == "notify_mission_select_log_") {
    show();
  } else if(action == "notify_log_entries_") {
    if(line < 0 || line >= 10) return true;
    auto &group{profiles.users[profiles.user]};
    if(static_cast<size_t>(line) < group.pilots.size()) {
      group.selected = static_cast<size_t>(line);
      editing = 0;
      visible("_log_screen_rename_box", false);
      visible("_log_screen_error_box", false);
      update();
      profiles.save();
    } else if(profiles.create(hours_now())) {
      rename(1, "Name Log");
      update();
      profiles.save();
    }
  } else if(action == "notify_log_create_button_") {
    if(profiles.create(hours_now())) {
      rename(1, "Name Log");
      update();
      profiles.save();
    }
  } else if(action == "notify_log_delete_button_") {
    if(!profiles.erase()) {
      error("Current Log Indestructable");
    } else {
      editing = 0;
      visible("_log_screen_rename_box", false);
      visible("_log_screen_error_box", false);
      update();
      profiles.save();
    }
  } else if(action == "notify_log_rename_button_") {
    if(profiles.current().status != 2) {
      error("Current Log Not Active");
    } else if(profiles.current().protected_profile) {
      error("Cannot Rename Pilot");
    } else {
      rename(1, "Rename Log");
    }
  } else if(action == "notify_log_user_rename_button_") {
    rename(2, "Rename User");
  } else if(action == "notify_log_users_") {
    size_t index{0};
    for(auto const &candidate : interface.objects()) {
      if(candidate.action == action) {
        if(&candidate == &object) break;
        ++index;
      }
    }
    profiles.select_user(index, hours_now());
    editing = 0;
    visible("_log_screen_rename_box", false);
    visible("_log_screen_error_box", false);
    update();
    profiles.save();
  } else {
    return false;
  }
  return true;
}
bool pilot_menu::key(unsigned int const character) {
  if(!editing || interface.current() != "_log_screen" || !(interface.named("_log_screen_rename_box").flags & 1)) return false;
  auto &text{interface.named("_log_screen_rename_text").text};
  if(character == 27) {
    editing = 0;
    visible("_log_screen_rename_box", false);
  } else if(character == 8) {
    if(!text.empty()) text.pop_back();
  } else if(character == 10 || character == 13) {
    if(editing == 2) {
      profiles.users[profiles.user].name = text.substr(0, 15);
    } else {
      profiles.current().name = text.empty() ? "de Fault" : text;
    }
    editing = 0;
    visible("_log_screen_rename_box", false);
    update();
    profiles.save();
  } else if(character >= 32 && character < 127 && text.size() < 30) text += static_cast<char>(character);
  return true;
}
bool pilot_menu::can_exit() {
  if(profiles.current().status != 2) {
    error("Log Not Active");
    return false;
  }
  editing = 0;
  visible("_log_screen_rename_box", false);
  visible("_log_screen_error_box", false);
  profiles.save();
  return true;
}
} // namespace apache::frontend
