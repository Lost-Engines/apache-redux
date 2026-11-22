#pragma once
#include "fragments.h"
#include "mission_damage.h"
#include "mission_flight.h"
#include "mission_player_damage.h"
#include "mission_player_weapons.h"
#include "player_route.h"
#include "player_sensors.h"
#include "resources/features.h"

namespace apache::simulation {
struct mission_random_state { uint32_t global{1}, weapons{1}, damage{1}; };
struct mission_runtime_events {
  std::function<void(size_t)> fire;
  std::function<void(size_t)> waypoint;
  std::function<void(target_reference const &, target_reference const &, unsigned int)> ground_fire;
  std::function<void(size_t, projectile_record const &)> launched;
  std::function<void(size_t, projectile_record &, projectile_event_info const &)> projectile;
  std::function<void(target_identity const &, target_identity const &)> killed;
  std::function<void(target_identity const &)> collision, player_killed;
  std::function<void(unsigned int, bool)> sound;
  std::function<void(std::string_view)> message;
  std::function<void(std::string_view, int32_t, int32_t)> notice;
  std::function<void(explosion_audio const &)> explosion_sound;
  std::function<void(bool)> impact_sound;
  std::function<void(unsigned int)> radar_toggle;
  std::function<void(unsigned int)> launch_sound;
  std::function<void()> cycle_target, incoming_sound, player_weapon_update, instrument_update;
  std::function<void(bool)> gun_sound;
  std::function<void(size_t, bool, std::string_view)> warning_message;
  std::function<void(size_t)> missile_camera;
  std::function<void(flight_step_result const &)> flight;
  std::function<void(landing_result const &)> landing;
};
// One mission owns the actor/target identities shared by physics, combat and drawing.
class mission_runtime {
  mission_actors make_actors(unsigned int map);
  mission_combat_callbacks combat_callbacks();
  movement_callbacks movement_events();
  int32_t previous_time{0};
  void synchronize_player_target();
public:
  mission_runtime_events events;
  int32_t quick_score{0};
  resources::mission_definition mission;
  resources::executable_data executable;
  resources::terrain terrain;
  resources::model_archive models;
  mission_scenery scene;
  resources::feature_layer features, super_features;
  math::trigonometry trig;
  std::vector<std::vector<resources::formation_member>> formations;
  actor_environment environment;
  combat_random random;
  mission_actors actors;
  render::mobile_scene mobiles;
  mission_spatial spatial;
  scenery_collisions structures;
  navigation_environment navigation;
  mission_targets targets;
  combat_random weapon_random;
  combat_tables tables;
  mission_combat combat;
  navigation_tables performance;
  actor_movement movement;
  std::unique_ptr<mission_weapons> weapons;
  std::unique_ptr<mission_damage> damage;
  std::unique_ptr<mission_flight> pilot;
  std::unique_ptr<mission_player_damage> player_damage;
  std::unique_ptr<mission_player_weapons> player_armament;
  std::unique_ptr<player_sensors> sensors;
  player_route route;
  movement_time timing;
  size_t frame{0};
  bool check_collisions{true}, subdivide_flight{true}, flight_active{true}, move_actors{true};
  int32_t smoke_radius{2048}, smoke_lifetime{400};
  unsigned int smoke_density{2};
  fragments debris;
  craters collision_marks;
  mission_runtime(resources::data_store const &data, std::string const &zone, std::string const &path, bool armed = true, bool hurt = true, int difficulty = 0, mission_random_state seeds = {}, resources::mission_definition const *edited = nullptr);
  mission_runtime(mission_runtime const &) = delete;
  mission_runtime &operator=(mission_runtime const &) = delete;
  void start_player(size_t index, mission_flight_settings const &settings);
  void equip_player(std::array<int32_t, 9> const &loadout);
  void enable_player_sensors(unsigned int game_mode = 0);
  void look_player(math::pose const &view, render::projection const &projection);
  bool fire_player(int32_t now, double dt, uint16_t key);
  void advance(int32_t now, int32_t delta, flight_input const &input, std::function<unsigned int()> const &flight_random);
};
} // namespace apache::simulation
