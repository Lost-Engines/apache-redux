#pragma once
#include <string_view>
#include "actor_firing.h"
#include "target_search.h"

namespace apache::simulation {
struct actor_attention {
  int32_t range{0};
  uint8_t damage{0}, processed_damage{0};
};
struct flight_path {
  std::optional<math::vector3> previous, current, next;
  int32_t type{0};
};
int32_t attention_range(int32_t task, int32_t side, forces_info const &skill);
bool target_behind_actor(actor_state const &actor, math::vector3 const &position);
int32_t distance_from_flight_path(flight_path const &path, math::vector3 const &position, math::trigonometry const &trig);
struct actor_targeting_context {
  int32_t now{0}, task{0}, start_time{0};
  bool has_formation{false}, radar_model{false};
  flight_path path;
  shared_target player_target;
};
struct actor_targeting_callbacks {
  std::function<movement_target(target_reference const &)> describe;
  std::function<int32_t(target_reference const &)> actor_class;
  std::function<shared_target(math::vector3 const &, int32_t, int32_t, int32_t, search_kind)> search;
  std::function<shared_target(int32_t, int32_t, int32_t)> ground_sector;
  std::function<bool(target_reference const &)> visible;
  std::function<int32_t(int32_t, int32_t)> height;
  std::function<shared_target(math::vector3 const &)> fixed;
  std::function<void()> released, cap;
  std::function<void(target_reference const &)> targeted;
  std::function<void(unsigned int)> sound;
  std::function<void(std::string_view)> message;
};
class actor_targeting {
  actor_state &actor;
  actor_armament &armament;
  actor_attention &attention;
  actor_type const &type;
  std::array<weapon_info, 55> const &weapons;
  math::trigonometry const &trig;
  forces_info const &skill, &friendly_skill;
  combat_random &random;
  int32_t &bombing_message_time;
  actor_targeting_context const &context;
  actor_targeting_callbacks const &callbacks;
  movement_target describe(shared_target const &target) const;
  int32_t enemy_side() const;
  int32_t choose();
  int32_t next_fire_time();
  void set_fire_time(int32_t value);
  void release();
  void delay_fire();
  bool behind(shared_target const &target) const;
  shared_target search_air() const;
  shared_target search_ground() const;
  void bomber_transit();
public:
  actor_targeting(actor_state &unit, actor_armament &arms, actor_attention &state, actor_type const &header, std::array<weapon_info, 55> const &definitions, math::trigonometry const &angles, forces_info const &difficulty, forces_info const &friendly, combat_random &rng, int32_t &message_time, actor_targeting_context const &inputs, actor_targeting_callbacks const &events);
  void basic_aircraft();
  void strategic_bomber();
  void tactical_bomber();
  void laser_bomber();
  void ground_strike();
  void aircraft();
  void air_defence();
  void artillery();
  void check_air_lock();
};
} // namespace apache::simulation
