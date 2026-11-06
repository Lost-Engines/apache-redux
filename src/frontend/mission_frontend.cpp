#include "mission_frontend.h"
#include <algorithm>
#include <chrono>
#include <stdexcept>
#include "render/menu_font.h"

namespace apache::frontend {
namespace {
std::string text(resources::mission_briefing const &mission, std::string_view const tag) {
  std::string result;
  bool active{false};
  for(auto const &section : mission.text) {
    if(section.tag != "Para") active = section.tag == tag;
    else if(active) result += "\n\n";
    if(active) for(auto const &line : section.lines) {
      if(!result.empty() && result.back() != ' ' && result.back() != '\n') result += ' ';
      result += line;
    }
  }
  return result;
}
uint32_t hours_now() {
  return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::hours>(std::chrono::system_clock::now().time_since_epoch()).count());
}
std::string zone_name(int const zone) {
  return std::array<std::string, 4>{"yemen", "korea", "cyprus", "fhood"}.at(static_cast<size_t>(zone - 1));
}
}
mission_frontend::mission_frontend(resources::data_store const &data, mission_options const settings)
  : data{data}, definitions{resources::read_mission_briefings(data.read("MISSDATA/MISSIONS.BLK"))}, interface{data}, options{settings}, settings{data}, preferences{data, interface, this->settings}, help{data, interface}, planner{data, interface, this->settings}, logs{hours_now()}, pilots{data, interface, logs, definitions}, scores{data, interface, this->settings}, network{interface, this->settings} {
  network.start = [this](){ start_network(); };
  network.mission_selection = [this](unsigned int code, int value){ receive_selection(code, value); };
  interface.link_scroll("_map_briefing_subtitle_text", "_map_briefing_subtitle_scrollbar");
  interface.link_scroll("_situation_text", "_situation_scroll_bar");
  interface.number_changed = [this](widget &object, bool final){ planner.number_changed(object, final); };
  interface.finish = [this](render::framebuffer &target){ planner.finish(target); };
  interface.pointer = [this](widget &object, pointer_action action, int x, int y){ return prepared && planner.pointer(object, action, x, y); };
  interface.activated = [this](widget &object, int line){ return activate(object, line); };
  interface.paint = [this](render::framebuffer &frame, widget const &object){
    preferences.paint(frame, object);
    if(prepared) planner.paint(frame, object);
  };
}
void mission_frontend::load_profiles(std::filesystem::path const &path) { logs.load(path, hours_now()); settings.load(path); scores.load(); apply_preferences(); }
void mission_frontend::count_run() {
  auto const runs{settings.values.integer("Quick Start Game Runs").value_or(0)};
  settings.set("Quick Start Game Runs", runs < INT32_MAX ? runs + 1 : runs); settings.save();
}
void mission_frontend::startup() {
  preferences.zone = std::clamp(settings.values.integer("Set Map").value_or(3), 1, 3);
  first_launch = settings.values.integer("Quick Start Game Runs").value_or(0) == 0;
  if(first_launch) preferences.show(); else count_run();
}
void mission_frontend::launch_quick() {
  auto choice{options}; choice.invulnerable = invincible_quick;
  prepared = std::make_unique<mission_session>(data, mission_session::quick_start(data, zone), choice);
  flow = 1; result.reset(); count_run(); interface.toggle_quick(false); departing = true;
}
void mission_frontend::apply_preferences() {
  options.arcade = settings.model == 2; options.video_mode = settings.video_mode; options.detail = settings.detail; options.difficulty = static_cast<int>(settings.forces);
  options.controls = settings.controls; options.deadband = settings.deadband; options.axes = settings.axes;
  options.graphics.field_of_view = settings.graphics.field_of_view;
  options.graphics.lod_distance_multiplier = settings.graphics.lod_distance_multiplier;
  if(prepared) prepared->configure_graphics(options.graphics);
}
void mission_frontend::visible(std::string_view const name, bool const show) {
  auto &object{interface.named(name)}; object.flags = (object.flags & ~1u) | static_cast<unsigned int>(show);
}
void mission_frontend::list() {
  listed.clear(); selected = -1;
  for(size_t i{0}; i != definitions.size(); ++i) if(definitions[i].flow == flow && definitions[i].zone == zone) listed.push_back(i);
  std::ranges::sort(listed, [this](auto a, auto b){ return definitions[a].id < definitions[b].id; });
  std::string names, numbers, ticks;
  for(auto index : listed) {
    auto const &mission{definitions[index]};
    if(!names.empty()) { names += '\n'; numbers += '\n'; ticks += '\n'; }
    names += mission.title; numbers += std::to_string(mission.id);
    auto const found{std::ranges::find(pilot().missions, static_cast<uint32_t>(mission.id), &simulation::mission_history::mission)};
    unsigned int const state{found == pilot().missions.end() ? 0u : static_cast<unsigned int>(found->result)};
    ticks += state == 0 ? ' ' : state == 1 ? '-' : static_cast<char>(5 - state);
  }
  auto &entries{interface.named("_mission_selection_list")}; entries.text = names; entries.selected_line = -1; entries.start_line = 0;
  interface.named("_mission_selection_number_list").text = numbers;
  interface.named("_mission_selection_tick_list").text = ticks;
  interface.named("_mission_select_screen").picture = "menuscrn/" + zone_name(zone) + ".lbm";
  interface.named("_mission_select_background").picture = "menuscrn/" + zone_name(zone) + "_p.lbm";
  visible("_mission_select_background", false);
  visible("_training_infinite_weapons_button", flow == 2); visible("_training_no_crashes_button", flow == 2);
  for(auto &object : interface.objects()) if(object.action == "notify_mission_selection_level_") object.flags &= ~1u;
  interface.show("_mission_select_screen"); network_controls();
}
void mission_frontend::select(int const row) {
  selected = row == selected || row < 0 || static_cast<size_t>(row) >= listed.size() ? -1 : row;
  interface.named("_mission_selection_list").selected_line = selected;
  visible("_mission_select_background", selected >= 0); visible("_mission_selection_load_button", selected >= 0);
  if(selected >= 0) { interface.named("_mission_selection_info_text").text = text(definitions[listed[static_cast<size_t>(selected)]], "Description"); interface.named("_mission_selection_info_text").start_line = 0; if(speak) speak(flow == 2 ? 10 : flow == 3 ? 6 : 3, flow, zone, static_cast<int>(pilot().campaign.level), selected + 1); }
}
void mission_frontend::prepare(int const id) {
  auto choice{options};
  choice.infinite_weapons = flow == 2 && interface.named("_training_infinite_weapons_button").on;
  choice.no_crashes = flow == 2 && interface.named("_training_no_crashes_button").on;
  if(network.selecting()) choice.connection = network.connection();
  result.reset(); prepared = std::make_unique<mission_session>(data, id, choice);
  auto const &definition{prepared->briefing}; flow = definition.flow; zone = definition.zone;
  auto const mid{static_cast<uint32_t>(id)};
  if(std::ranges::find(pilot().missions, mid, &simulation::mission_history::mission) == pilot().missions.end()) pilot().missions.push_back({mid});
  if(flow == 4 && std::ranges::find(pilot().campaign.missions, mid, &simulation::mission_history::mission) == pilot().campaign.missions.end()) pilot().campaign.missions.push_back({mid});
  int const parent{static_cast<int>(&interface.named("_map_screen") - interface.objects().data())};
  for(auto &object : interface.objects()) if(object.parent == parent) object.flags &= ~1u;
  for(auto const name : {"_map_briefing_button", "_map_takeoff_button", "_map_briefing_subtitle_box", "_map_exit_button"}) visible(name, true);
  interface.named("_map_briefing_subtitle_title").text = definition.title;
  interface.named("_map_briefing_subtitle_text").text = text(definition, "Description");
  interface.named("_map_briefing_subtitle_text").start_line = 0;
  subtitles(true); map(); interface.show("_map_screen"); network_controls();
  if(flow == 4 && settings.animations && movie) movie(zone, definition.level);
  if(flow == 4 && speak) speak(3, flow, zone, definition.level, id);
}
void mission_frontend::subtitles(bool const expanded) {
  int const height{expanded ? 210 : 50};
  interface.bounds("_map_briefing_subtitle_box", {70, 478 - height, 570, 478});
  auto &body{interface.named("_map_briefing_subtitle_text")}; body.area.bottom = body.area.top + height - 20;
  auto &bar{interface.named("_map_briefing_subtitle_scrollbar")}; bar.area.bottom = bar.area.top + height - 20;
}
void mission_frontend::map() { planner.open(*prepared); }
void mission_frontend::campaign(bool const fresh) {
  if(fresh || pilot().campaign.warzone != static_cast<uint32_t>(zone)) {
    pilot().campaign = {static_cast<uint32_t>(zone), 1, 4, static_cast<uint32_t>(std::chrono::system_clock::now().time_since_epoch().count()) & 0x7fff, {}}; last_result = 0;
  }
  campaign_mission = fresh ? 0 : pilot().next_mission(definitions);
  int level{static_cast<int>(pilot().campaign.level)}; std::string section{"Situation"};
  if(fresh) { level = 0; section = "Description"; }
  else if(campaign_mission == -2 || campaign_mission == -3) { level = 0; section = campaign_mission == -2 ? "Failure" : "Success"; }
  else if(last_result == 2) { ++level; section = "Failure"; }
  auto const raw{data.read("MISSDATA/C" + std::to_string(zone) + std::to_string(level) + ".TXT")};
  std::string_view const source{reinterpret_cast<char const*>(raw.data()), raw.size()};
  auto start{source.find(":" + section)};
  if(start == std::string_view::npos) throw std::runtime_error{"Missing campaign situation text"};
  start = source.find('\n', start);
  if(start == std::string_view::npos) throw std::runtime_error{"Truncated campaign situation text"};
  start = source.find_first_not_of("\r\n", start);
  auto const end{source.find(':', start)};
  auto &body{interface.named("_situation_text")}; body.text = source.substr(start, end - start);
  std::erase(body.text, '\r'); body.start_line = 0;
  interface.named("_situation_screen").picture = "menuscrn/c_" + zone_name(zone) + ".lbm";
  visible("_situation_ok_button", campaign_mission >= 0);
  if(campaign_mission == -3) { pilot().victories[static_cast<size_t>(zone - 1)] = true; pilot().campaign = {}; }
  else if(campaign_mission == -2) pilot().campaign = {};
  logs.save(); interface.show("_situation_screen");
  if(speak) speak(fresh ? 2 : campaign_mission == -3 ? 5 : campaign_mission == -2 || last_result == 2 ? 4 : 1, 4, zone, static_cast<int>(pilot().campaign.level), 0);
}
std::unique_ptr<mission_session> mission_frontend::takeoff() {
  if(!departing) return {};
  departing = false;
  if(prepared && planner.editor.modified) {
    auto const definition{prepared->runtime.mission}; auto const briefing{prepared->briefing}; auto const choice{prepared->launch_options()};
    planner.close(); prepared = std::make_unique<mission_session>(data, briefing, choice, &definition);
  }
  planner.close(); return std::move(prepared);
}
void mission_frontend::complete(std::unique_ptr<mission_session> mission) {
  if(mission->network && network.peers) {
    auto const mode{mission->network->lifecycle.mode}; network.completed(*mission->network);
    if(mode == multiplayer::game_mode::combat || mode == multiplayer::game_mode::multiplayer || mode == multiplayer::game_mode::capture_the_flag) { planner.close(); prepared.reset(); interface.show(mode == multiplayer::game_mode::combat ? "_modem_screen" : "_network_screen"); return; }
  }
  if(mission->briefing.flow == 1) {
    if(first_quick) { first_quick = false; interface.show("_main_screen"); }
    else if(!invincible_quick) { interface.back(); scores.record(mission->runtime.quick_score, pilot().name); scores.show(); }
    return;
  }
  prepared = std::move(mission);
  result = simulation::evaluate_debrief(prepared->runtime, prepared->records, prepared->runtime.random.global_seed, pilot().name);
  interface.named("_map_briefing_subtitle_title").text = result->title();
  auto &body{interface.named("_map_briefing_subtitle_text")}; body.text.clear(); body.start_line = 0;
  for(auto const &line : result->explanations) { if(!body.text.empty()) body.text += "\n\n"; body.text += line; }
  visible("_map_takeoff_button", false); visible("_map_exit_button", false); visible("_map_debrief_exit_button", true);
  visible("_map_debrief_log_do_not_log_box", false); map(); interface.show("_map_screen");
}
void mission_frontend::show_pilots(bool const transfer_campaign) {
  if(transfer_campaign) transferred_campaign = pilot().campaign;
  else transferred_campaign.reset();
  pilots.show();
}
void mission_frontend::show_awards(unsigned int const mask) {
  // Screens are stacked in the same order as the original debrief callback.
  if(mask & 8) interface.show("_medal_3_screen");
  if(mask & 4) interface.show("_medal_2_screen");
  if(mask & 2) interface.show("_medal_1_screen");
  if(mask & 1) {
    std::array<std::string_view, 4> const pictures{"prm_lt", "prm_cpt", "prm_mjr", "prm_lcol"};
    if(pilot().rank >= 3 && pilot().rank <= 6) interface.named("_promotion_screen").picture = "menuscrn/" + std::string{pictures[pilot().rank - 3]} + ".lbm";
    interface.show("_promotion_screen");
  }
}
void mission_frontend::log_result(bool const keep) {
  if(!prepared || !result) return;
  unsigned int awards{0};
  if(keep) {
    result->log(pilot(), static_cast<uint32_t>(prepared->briefing.id), static_cast<unsigned int>(flow), hours_now());
    pilot().update(definitions); awards = pilot().awards; last_result = static_cast<unsigned int>(result->mission); logs.save();
  }
  planner.close(); prepared.reset(); result.reset(); interface.back();
  if(network.peers) { interface.show("_modem_screen"); return; }
  if(flow != 4) list();
  if(pilot().status != 2) show_pilots(flow == 4);
  else if(flow == 4 && !awards) campaign();
  show_awards(awards);
}
void mission_frontend::back() {
  if(interface.dismiss_popup()) return;
  auto const current{interface.current()};
  if(stop_speech && (current == "_map_screen" || current == "_mission_select_screen" || current == "_situation_screen")) stop_speech();
  if(network.peers && (current == "_network_screen" || current == "_modem_screen" || (network.selecting() && (current == "_world_screen" || current == "_mission_select_screen" || current == "_map_screen")))) { leave_network(); return; }
  if(current == "_preferences_screen") {
    settings.save(); apply_preferences();
    if(first_launch) { first_launch = false; first_quick = true; invincible_quick = true; zone = preferences.zone; launch_quick(); return; }
  }
  if(current == "_top_ten_screen") { scores.close(); interface.back(); return; }
  if(current == "_promotion_screen" || current.starts_with("_medal_")) {
    interface.back();
    auto const next{interface.current()};
    if(next != "_promotion_screen" && !next.starts_with("_medal_") && flow == 4 && pilot().status == 2 && !transferred_campaign) campaign();
    return;
  }
  if(current == "_log_screen") {
    if(!pilots.can_exit()) return;
    interface.back();
    if(transferred_campaign) {
      pilot().campaign = std::move(*transferred_campaign); transferred_campaign.reset();
      zone = static_cast<int>(pilot().campaign.warzone); logs.save(); campaign();
    }
    return;
  }
  if(current == "_map_screen" && result) {
    visible("_map_debrief_log_do_not_log_box", true); visible("_map_debrief_retry_mission_button", true); visible("_map_debrief_do_not_log_mission_button", flow != 4);
  } else { if(current == "_map_screen") { planner.close(); prepared.reset(); } interface.back(); }
}
bool mission_frontend::activate(widget &object, int const line) {
  auto const &action{object.action};
  if(action == "notify_log_button_" || action == "notify_mission_select_log_") {
    show_pilots(flow == 4 && interface.current() != "_main_screen" && interface.current() != "_flight_screen"); return true;
  }
  if(action == "notify_flight_network_button_" || action == "notify_flight_2_player_button_") {
    try { network.open(action == "notify_flight_2_player_button_", pilot().name); }
    catch(std::exception const &error) { network_error(error.what()); }
    return true;
  }
  if(action == "notify_network_exit_button_" || action == "notify_modem_exit_button_") { leave_network(); return true; }
  try { if(network.activate(object)) return true; }
  catch(std::exception const &error) { network_error(error.what()); return true; }
  if(network.selecting() && network.remote_controlled() && (action.starts_with("notify_map_") || action.starts_with("notify_mission_selection_") || action == "notify_takeoff_button_")) return true;
  if(help.activate(object)) return true;
  if(planner.activate(object)) return true;
  if(pilots.activate(object, line)) return true;
  if(preferences.activate(object)) { apply_preferences(); return true; }
  if(action == "notify_quick_start_topten_button_") scores.show();
  else if(action == "notify_top_ten_exit_button_") back();
  else if(action == "notify_quick_start_arcade_button_" || action == "notify_quick_start_invin_button_") {
    flow = 1; invincible_quick = action == "notify_quick_start_invin_button_"; interface.show("_world_screen");
  }
  else if(action == "notify_training_no_crashes_" || action == "notify_training_infinite_weapons_") object.on = !object.on;
  else if(action == "notify_training_button_") { flow = 2; zone = 4; preferences.zone = zone; list(); }
  else if(action == "notify_single_button_" || action == "notify_campaign_button_") { flow = action == "notify_single_button_" ? 3 : 4; interface.show("_world_screen"); }
  else if(action == "notify_map_yemen_button_" || action == "notify_map_korea_button_" || action == "notify_map_cyprus_button_") {
    zone = action == "notify_map_yemen_button_" ? 1 : action == "notify_map_korea_button_" ? 2 : 3; preferences.zone = zone;
    if(network.selecting()) network.send_selection(static_cast<unsigned int>(12 + zone));
    if(flow == 1) launch_quick(); else if(flow == 4) campaign(pilot().campaign.warzone != static_cast<uint32_t>(zone)); else list();
  } else if(action == "notify_mission_selection_list_") { select(line); if(network.selecting()) network.send_selection(17, line); }
  else if(action == "notify_mission_selection_load_") { if(selected >= 0) { if(network.selecting()) network.send_selection(16); prepare(definitions[listed[static_cast<size_t>(selected)]].id); } }
  else if(action == "notify_situation_ok_button_") { if(campaign_mission == 0) campaign(); else if(campaign_mission > 0) prepare(campaign_mission); }
  else if(action == "notify_takeoff_button_") { departing = prepared != nullptr; if(departing && network.selecting()) network.send_selection(18); }
  else if(action == "notify_debrief_log_button_") log_result(true);
  else if(action == "notify_debrief_do_not_log_button_") log_result(false);
  else if(action == "notify_debrief_retry_mission_button_") { if(prepared) prepare(prepared->briefing.id); }
  else if(action == "notify_preferences_exit_" || action == "notify_promotion_exit_button_" || action == "notify_exit_log_screen_" || action == "notify_mission_select_exit_button_" || action == "notify_map_exit_button_" || action == "notify_debrief_exit_button_" || action == "notify_situation_exit_button_") back();
  else if(action == "notify_briefing_subtitle_min_max_button_") { auto const &area{interface.named("_map_briefing_subtitle_box").area}; subtitles(area.bottom - area.top < 51); }
  else if(action == "notify_briefing_button_") {
    bool const show{!(interface.named("_map_briefing_subtitle_box").flags & 1)};
    visible("_map_briefing_subtitle_box", show);
    if(show && !result && prepared && speak && (!speech_playing || !speech_playing())) speak(flow == 2 ? 10 : flow == 3 ? 6 : 3, flow, zone, prepared->briefing.level, flow == 4 ? prepared->briefing.id : selected + 1);
  }
  else return false;
  return true;
}
} // namespace apache::frontend
