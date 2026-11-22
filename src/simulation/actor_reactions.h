#pragma once
#include <span>
#include "actor_firing.h"

namespace apache::simulation {
struct combat_warning_times {
  int32_t ships_attacked{0}, radar{0}, bandit{0};
};
struct actor_reaction_context {
  int32_t now{0}, task{0}, attacker_side{0};
  uint32_t attacker_model{0}, view_heading{0};
  bool player{false};
  math::vector3 view_position{};
  movement_target attacker;
  std::shared_ptr<target_reference> reference;
};
struct actor_reaction_callbacks {
  std::function<bool()> visible;
  std::function<void()> target_released;
  std::function<void(unsigned int, bool)> sound;
};
bool acquire_attacker(actor_state &actor, actor_armament &armament, actor_type const &type, std::array<weapon_info, 55> const &weapons, uint32_t &war_random, combat_random &weapon_random, actor_reaction_context const &context, actor_reaction_callbacks const &callbacks);
void notify_actor_fired_at(actor_state &actor, actor_armament &armament, actor_type const &type, std::array<weapon_info, 55> const &weapons, combat_warning_times &warnings, uint32_t &war_random, combat_random &weapon_random, actor_reaction_context const &context, actor_reaction_callbacks const &callbacks);
void notify_actor_targeted(actor_state &actor, actor_armament &armament, actor_type const &type, std::array<weapon_info, 55> const &weapons, combat_warning_times &warnings, uint32_t &war_random, combat_random &weapon_random, math::trigonometry const &trig, actor_reaction_context const &context, actor_reaction_callbacks const &callbacks);
bool kill_actor(actor_state &actor, actor_armament &armament, int32_t &task_alive, std::span<actor_state *> followers, std::function<void()> const &unplace, std::function<void()> const &released);
} // namespace apache::simulation
