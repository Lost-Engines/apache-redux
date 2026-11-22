#pragma once
#include "math/geometry.h"
#include "pilot_log.h"

namespace apache::simulation {
struct debrief_record {
  std::array<uint32_t, 6> words{};
};
class debrief_records {
  math::vector3 previous_position{};
public:
  std::vector<debrief_record> entries;
  void add(debrief_record record);
  void position(math::vector3 const &position);
  bool player_killed(uint32_t object, uint32_t player) const;
  uint32_t elapsed() const;
  unsigned int worst_landing() const;
};
// Pilot outcome is distinct from the saved log status: 0 safe, 1 missing,
// 2 captured, 3 injured, 4 killed, 5 escaped.
struct debrief_result {
  mission_result mission{mission_result::failure};
  mission_result targets{mission_result::failure};
  mission_result player{mission_result::success};
  uint32_t pilot{0};
  uint32_t time{0};
  bool aborted{false};
  bool dead{false};
  bool landed_in_enemy{false};
  uint8_t front_distance{0};
  std::vector<std::string> explanations;
  void survive(bool enemy, bool injury, unsigned int distance, uint32_t &random);
  void mobiles(std::array<int32_t, 3> const &counts, std::array<int32_t, 3> const &limits);
  void required_ground(unsigned int required, unsigned int destroyed);
  void extra_ground(bool destroyed);
  void prohibited_ground(bool destroyed);
  void final_position(int32_t side, int32_t altitude, int32_t ground, unsigned int distance);
  void death(unsigned int cause, uint32_t collision_model, int32_t altitude, unsigned int surface, uint32_t &random, std::string_view collision_name = {});
  void landing(unsigned int grade, unsigned int death, uint32_t &random);
  void pilot_explanation(std::string_view pilot_name);
  std::string title() const;
  void log(pilot_log &log, uint32_t mission_id, unsigned int flow, uint32_t now_hours) const;
};
} // namespace apache::simulation
