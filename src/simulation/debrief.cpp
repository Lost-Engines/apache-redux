#include "debrief.h"
#include <algorithm>
#include <cstdlib>

namespace apache::simulation {
void debrief_records::add(debrief_record record) {
  // The original allocates a fresh 1,000-record block before coalescing time.
  if(!entries.empty() && entries.size() % 1000 && record.words[0] == 2 && entries.back().words[0] == 2) {
    entries.back().words[1] += record.words[1];
  } else entries.push_back(record);
}
void debrief_records::position(math::vector3 const &p) {
  if(std::abs(static_cast<int64_t>(p[0]) - previous_position[0]) < 128000 && std::abs(static_cast<int64_t>(p[1]) - previous_position[1]) < 128000) return;
  add({{3, static_cast<uint32_t>(p[0]), static_cast<uint32_t>(p[1]), 0, 0, 0}}); previous_position = p;
}
bool debrief_records::player_killed(uint32_t const object, uint32_t const player) const {
  for(auto const &record : entries) if(record.words[0] == 8 && record.words[5] == object) return record.words[2] == player;
  return false;
}
uint32_t debrief_records::elapsed() const {
  uint32_t time{0}; for(auto const &record : entries) if(record.words[0] == 2) time += record.words[1]; return time;
}
unsigned int debrief_records::worst_landing() const {
  unsigned int grade{0}; for(auto const &record : entries) if(record.words[0] == 12) grade = std::max(grade, record.words[5]); return grade;
}
void debrief_result::survive(bool const enemy, bool const injury, unsigned int const distance, uint32_t &random) {
  if(pilot) return;
  auto const coin{[&]{ random = random * 0x41c64e6du + 0x3039u; return (random >> 16) & 1; }};
  if(injury) {
    if(coin()) { pilot = enemy ? 1 : 4; return; }
    pilot = 3;
  }
  if(enemy) {
    if(distance == 1) { if(coin()) pilot = 2; else if(!pilot) pilot = 5; } else if(distance > 2) pilot = 2;
  }
}
void debrief_result::mobiles(std::array<int32_t, 3> const &counts, std::array<int32_t, 3> const &limits) {
  if(counts[0] > limits[0]) { mission = mission_result::failure; explanations.emplace_back("Allied or civilian casualties too high."); } else {
    if(counts[1] >= limits[1]) {
      mission = static_cast<int64_t>(counts[1]) + counts[2] >= limits[2] ? mission_result::outstanding : mission_result::success;
      explanations.emplace_back(mission == mission_result::outstanding ? "Outstanding number of targets." : "Enough targets for success.");
    } else explanations.emplace_back("Insufficient targets destroyed.");
  }
  targets = mission;
}
void debrief_result::required_ground(unsigned int const required, unsigned int const destroyed) {
  if(!required) return;
  if(destroyed < required) {
    if(mission > mission_result::failure) { mission = mission_result::failure; explanations.emplace_back("Not all the required targets were destroyed."); }
    targets = mission_result::failure;
  } else if(destroyed == required) explanations.emplace_back("All required targets were destroyed.");
}
void debrief_result::extra_ground(bool const destroyed) {
  if(destroyed && mission == mission_result::success) mission = targets = mission_result::outstanding;
}
void debrief_result::prohibited_ground(bool const destroyed) {
  if(destroyed && mission > mission_result::failure) {
    mission = player = mission_result::failure; explanations.emplace_back("You destroyed a prohibited target.");
  }
}
void debrief_result::final_position(int32_t const side, int32_t const altitude, int32_t const ground, unsigned int const distance) {
  if(side != 2) return;
  if(mission > mission_result::failure) mission = player = mission_result::failure;
  aborted = true; if(altitude <= ground) landed_in_enemy = true; front_distance = static_cast<uint8_t>(distance);
}
void debrief_result::death(unsigned int const cause, uint32_t const collision_model, int32_t const altitude, unsigned int const surface, uint32_t &random, std::string_view const collision_name) {
  if(cause != 2 && cause != 3 && cause != 4 && cause != 11 && cause != 12) return;
  if(mission > mission_result::failure) {
    mission = player = mission_result::failure;
    explanations.emplace_back(cause == 2 ? "Mission Failed - Apache destroyed.." : "Mission Failed - Apache destroyed.");
  }
  aborted = false;
  if(cause == 2) {
    if(collision_model == 11) { explanations.emplace_back("Apache crash landed."); survive(landed_in_enemy, true, front_distance, random); return; }
    explanations.emplace_back(collision_model && !collision_name.empty() ? "Apache collided with " + std::string{collision_name} + "." : "Apache crashed");
    if(altitude <= 5120) survive(landed_in_enemy, true, front_distance, random); else pilot = 4;
  } else if(cause == 3) { survive(landed_in_enemy, true, front_distance, random); explanations.emplace_back("Apache crash landed."); } else if(cause == 4) {
    pilot = 4; explanations.emplace_back(collision_model && !collision_name.empty() ? "Apache was shot down by " + std::string{collision_name} : "Apache was shot down");
  } else if(cause == 11) { pilot = 4; explanations.emplace_back("Apache disintegrated due to excessive speed."); } else {
    survive(landed_in_enemy, true, front_distance, random);
    if(surface >= 1 && surface <= 3) explanations.emplace_back(surface == 1 ? "Apache ditched in a river." : surface == 2 ? "Apache ditched in a lake." : "Apache ditched in the sea.");
  }
  dead = true;
}
void debrief_result::landing(unsigned int const grade, unsigned int const death, uint32_t &random) {
  if(!grade) {
    if(mission > mission_result::failure) { mission = player = mission_result::failure; explanations.emplace_back("Mission failed because you did not land."); }
    if(death == 0 || death == 1) aborted = true;
  } else if(grade >= 5) {
    if(mission > mission_result::failure) { mission = player = mission_result::failure; explanations.emplace_back("Mission failed because you crash landed."); }
    dead = true; survive(landed_in_enemy, true, front_distance, random);
  } else if(grade >= 3) {
    if(mission > mission_result::success) { mission = player = mission_result::success; explanations.emplace_back("Mission not outstanding due to heavy landing."); }
  } else if(landed_in_enemy && death != 3) {
    mission = player = mission_result::failure; explanations.emplace_back("Misson failed due to landing in enemy territory.");
    survive(landed_in_enemy, false, front_distance, random);
  }
}
void debrief_result::pilot_explanation(std::string_view const pilot_name) {
  static std::array<std::string_view, 5> const outcomes{
    " is missing in action.", " was captured by enemy forces and is now a prisoner of war.",
    " was injured.", " was killed.", " landed in enemy territory, but fortunately escaped."
  };
  if(pilot >= 1 && pilot <= outcomes.size()) explanations.push_back(std::string{pilot_name} + std::string{outcomes[pilot - 1]});
}
std::string debrief_result::title() const {
  return mission == mission_result::outstanding ? " - OUTSTANDING" : mission == mission_result::success ? " - SUCCESS" : aborted ? " - ABORTED" : " - FAILURE";
}
void debrief_result::log(pilot_log &log, uint32_t const mission_id, unsigned int const flow, uint32_t const now_hours) const {
  if(flow >= 2 && flow <= 4) { log.log_result(mission_id, mission, flow == 4); log.total_time += time; }
  if(flow == 3 || flow == 4) log.combat_time += time;
  if(dead && pilot >= 1 && pilot <= 4) log.set_status(pilot == 4 ? (flow == 2 ? 9 : 8) : pilot + 4, now_hours);
}
} // namespace apache::simulation
