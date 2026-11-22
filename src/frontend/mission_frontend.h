#pragma once
#include <filesystem>
#include "briefing_map.h"
#include "help_menu.h"
#include "menu.h"
#include "mission_session.h"
#include "network_menu.h"
#include "pilot_menu.h"
#include "preferences_menu.h"
#include "score_menu.h"

namespace apache::frontend {
class mission_frontend {
  resources::data_store const &data;
  std::vector<resources::mission_briefing> definitions;
  std::vector<size_t> listed;
  std::unique_ptr<mission_session> prepared;
  std::optional<simulation::debrief_result> result;
  int flow{2};
  int zone{4};
  int selected{-1};
  int campaign_mission{0};
  unsigned int last_result{0};
  bool departing{false};
  bool first_launch{false};
  bool first_quick{false};
  bool invincible_quick{false};
  void launch_quick();
  void start_network();
  void receive_selection(unsigned int code, int value);
  void network_controls();
  void leave_network();
  void network_error(std::string const &message);
  void count_run();
  std::optional<simulation::campaign_progress> transferred_campaign;
  void show_pilots(bool transfer_campaign);
  void show_awards(unsigned int mask);
  void apply_preferences();
  void visible(std::string_view name, bool show);
  void list();
  void select(int row);
  void prepare(int id);
  void map();
  void subtitles(bool expanded);
  void campaign(bool new_campaign = false);
  void log_result(bool keep);
  bool activate(widget &object, int line);
public:
  std::function<void(int, int)> movie;
  std::function<void(int, int, int, int, int)> speak;
  std::function<void()> stop_speech;
  std::function<bool()> speech_playing;
  menu interface;
  mission_options options;
  configuration::settings settings;
  preferences_menu preferences;
  help_menu help;
  briefing_map planner;
  configuration::profiles logs;
  pilot_menu pilots;
  score_menu scores;
  network_menu network;
  simulation::pilot_log &pilot() {
    return logs.current();
  }
  void load_profiles(std::filesystem::path const &path);
  void startup();
  void poll_network(uint32_t ticks);
  bool text_input(unsigned int value);
  mission_frontend(resources::data_store const &data, mission_options options = {});
  std::unique_ptr<mission_session> takeoff();
  void complete(std::unique_ptr<mission_session> mission);
  void back();
};
} // namespace apache::frontend
