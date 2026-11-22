#include "pilot_log.h"
#include <algorithm>
#include <ranges>
#include <stdexcept>

namespace apache::simulation {
namespace {
void record(std::vector<mission_history> &history, uint32_t const id, mission_result const result, size_t const capacity) {
  auto const entry{std::ranges::find(history, id, &mission_history::mission)};
  if(entry != history.end()) {
    entry->result = result;
  } else {
    if(history.size() == capacity) throw std::runtime_error{"Pilot mission history is full"};
    history.push_back({id, result});
  }
}
}
pilot_log pilot_log::decode(std::span<uint8_t const> bytes) {
  resources::reader input{bytes, "pilot log"};
  pilot_log result;
  for(size_t i{0}; ; ++i) {
    if(i == 64) input.fail("Unterminated pilot name");
    auto const ch{input.u8()};
    if(!ch) break;
    result.name.push_back(static_cast<char>(ch));
  }
  for(auto *field : {&result.rank, &result.status, &result.total_time, &result.combat_time, &result.status_hour, &result.protected_profile}) *field = input.u32();
  for(auto &medal : result.medals) medal = input.u32();
  auto const history{[&](std::vector<mission_history> &entries, size_t const capacity){
    for(;;) {
      auto const id{input.u32()};
      if(id == UINT32_MAX) break;
      if(entries.size() == capacity) input.fail("Too many pilot mission records");
      auto const state{input.u32()};
      if(state > 4) input.fail("Invalid mission result");
      entries.push_back({id, static_cast<mission_result>(state)});
    }
  }};
  history(result.missions, 500);
  for(auto *field : {&result.campaign.warzone, &result.campaign.level, &result.campaign.lives, &result.campaign.seed}) *field = input.u32();
  history(result.campaign.missions, 64);
  if(input.remaining()) {
    for(;;) {
      auto const zone{input.u32()};
      if(zone == UINT32_MAX) break;
      if(zone < 1 || zone > 3 || result.victories[zone - 1]) input.fail("Invalid campaign victory");
      result.victories[zone - 1] = true;
    }
  }
  if(input.remaining()) input.fail("Trailing pilot data");
  return result;
}
resources::bytes pilot_log::encode() const {
  if(name.size() > 63 || name.find('\0') != std::string::npos) throw std::runtime_error{"Pilot name must fit 63 bytes"};
  resources::bytes output{name.begin(), name.end()};
  output.push_back(0);
  auto const word{[&](uint32_t const value){
    for(unsigned int i{0}; i != 32; i += 8) output.push_back(static_cast<uint8_t>(value >> i));
  }};
  for(auto field : {rank, status, total_time, combat_time, status_hour, protected_profile}) word(field);
  for(auto medal : medals) word(medal);
  auto const history{[&](auto const &entries, size_t const capacity){
    if(entries.size() > capacity) throw std::runtime_error{"Too many pilot mission records"};
    for(auto const &entry : entries) {
      if(entry.mission == UINT32_MAX || static_cast<uint32_t>(entry.result) > 4) throw std::runtime_error{"Invalid mission history"};
      word(entry.mission);
      word(static_cast<uint32_t>(entry.result));
    }
    word(UINT32_MAX);
  }};
  history(missions, 500);
  for(auto field : {campaign.warzone, campaign.level, campaign.lives, campaign.seed}) word(field);
  history(campaign.missions, 64);
  for(size_t i{0}; i != victories.size(); ++i) {
    if(victories[i]) word(static_cast<uint32_t>(i + 1));
  }
  word(UINT32_MAX);
  return output;
}
void pilot_log::update(std::span<resources::mission_briefing const> const definitions) {
  completed = successes = failures = outstanding = campaigns_won = awards = 0;
  for(auto const &entry : missions) {
    auto const definition{std::ranges::find(definitions, static_cast<int32_t>(entry.mission), &resources::mission_briefing::id)};
    if(definition != definitions.end() && definition->flow == 2) continue;
    switch(entry.result) {
    case mission_result::failure:
      ++failures;
      break;
    case mission_result::success:
      ++successes;
      break;
    case mission_result::outstanding:
      ++outstanding;
      break;
    default:
      break;
    }
  }
  completed = successes + failures + outstanding;
  campaigns_won = static_cast<uint32_t>(std::ranges::count(victories, true));
  auto const score{successes + outstanding + campaigns_won * 8};
  static std::array<uint32_t, 4> constexpr thresholds{5, 15, 30, 50};
  for(size_t i{0}; i != thresholds.size(); ++i) {
    if(score >= thresholds[i] && rank <= i + 2) {
      if(!protected_profile) rank = static_cast<uint32_t>(i + 3);
      awards |= 1;
      break;
    }
  }
  for(size_t i{0}; i != medals.size(); ++i) {
    if(!medals[i] && (campaigns_won > i + 1 || (campaigns_won >= i + 1 && score > i * 10 + 9))) {
      medals[i] = 1;
      awards |= 2u << i;
    }
  }
}
void pilot_log::set_status(uint32_t const value, uint32_t const now_hours) {
  if(status != value) status_hour = now_hours;
  if(!protected_profile) status = value;
}
void pilot_log::recover(uint32_t const now_hours) {
  uint32_t const delay{status == 4 || status == 7 ? 24u : status == 5 ? 48u : status == 6 ? 96u : UINT32_MAX};
  if(delay != UINT32_MAX && now_hours >= status_hour && now_hours - status_hour >= delay) set_status(2, now_hours);
}
void pilot_log::log_result(uint32_t const id, mission_result const result, bool const in_campaign) {
  record(missions, id, result, 500);
  if(in_campaign) {
    record(campaign.missions, id, result, 64);
    campaign.seed = campaign.seed * 0x41c64e6du + 0x3039u;
    if(result == mission_result::success || result == mission_result::outstanding) {
      ++campaign.level;
    } else if(campaign.level) --campaign.level;
  }
}
int32_t pilot_log::next_mission(std::span<resources::mission_briefing const> const definitions) const {
  if(!campaign.warzone) return -1;
  if(!campaign.level) return -2;
  if(campaign.level == 9) return -3;
  if(!campaign.lives) return -2;
  std::array<std::vector<int32_t>, 3> choices;
  for(auto const &mission : definitions | std::views::reverse) {
    if(mission.flow == 4 && mission.zone == static_cast<int32_t>(campaign.warzone) && mission.level == static_cast<int32_t>(campaign.level)) {
      choices[2].push_back(mission.id);
      auto const previous{std::ranges::find(campaign.missions, static_cast<uint32_t>(mission.id), &mission_history::mission)};
      auto const state{previous == campaign.missions.end() ? mission_result::untried : previous->result};
      if(state == mission_result::untried) {
        choices[0].push_back(mission.id);
      } else if(state == mission_result::aborted || state == mission_result::failure) choices[1].push_back(mission.id);
    }
  }
  for(auto const &pool : choices) {
    if(!pool.empty()) {
      uint32_t const next{campaign.seed * 0x41c64e6du + 0x3039u};
      return pool[((next >> 16) & 0x7fff) % pool.size()];
    }
  }
  throw std::runtime_error{"Campaign stage has no missions"};
}
} // namespace apache::simulation
