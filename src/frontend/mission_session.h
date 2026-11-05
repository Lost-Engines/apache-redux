#pragma once
#include "multiplayer_chat.h"
#include "configuration/graphics.h"
#include "render/avionics.h"
#include "render/landscape.h"
#include "simulation/control_input.h"
#include "simulation/communications.h"
#include "simulation/daylight.h"
#include "simulation/mission_debrief.h"
#include "simulation/mission_crew.h"
#include "simulation/mission_end.h"
#include "simulation/mission_instruments.h"
#include "simulation/player_views.h"

namespace apache::frontend {
struct mission_connection {
  multiplayer::game_settings game;
  uint32_t local{0}, server{0};
  std::array<multiplayer::player_replica, 16> players;
  bool defer_initialisation{false};
};
struct mission_options {
  configuration::graphics graphics;
  bool arcade{true};
  bool invulnerable{false}, infinite_weapons{false}, no_crashes{false};
  unsigned int video_mode{1}, detail{0};
  int difficulty{0};
  simulation::control_configuration controls;
  unsigned int deadband{10};
  std::array<simulation::joystick_axis, 4> axes;
  std::optional<multiplayer::crew_role> shared_seat;
  std::optional<mission_connection> connection;
};
// Platform-independent live mission. Elapsed time is supplied in original 200 Hz ticks.
class mission_session {
  mission_options settings;
  render::framebuffer overlay;
  simulation::mission_random_state initial_seeds;
  std::optional<resources::mission_definition> network_definition;
  uint32_t library_random{1};
  unsigned int last_start_position{0};
  unsigned int keyboard_remainder{0};
  size_t frame_number{0};
  int32_t next_radio{6000};
  std::array<unsigned int, 54> radio_effects;
  int32_t last_delta{0};
  double frame_time{0.05};
  bool quit_drawn{false};
  std::vector<uint16_t> commands;
  std::optional<size_t> drone;
  unsigned int next_random();
  simulation::view_sources sources();
  simulation::projectile_record *round(size_t index);
  std::optional<simulation::view_target> missile_target(size_t index);
  void command(uint16_t key);
  void crash(bool airborne);
  void send_frame();
  void network_crash_complete();
  void restart_player();
public:
  resources::mission_briefing briefing;
  std::string zone;
  simulation::mission_runtime runtime;
  std::unique_ptr<simulation::mission_network> network;
  std::unique_ptr<simulation::mission_crew> crew;
  simulation::mission_end ending;
  simulation::control_input controls;
  simulation::communications communications;
  std::unique_ptr<multiplayer_chat> chat;
  simulation::debrief_records records;
  std::unique_ptr<simulation::player_views> views;
  std::unique_ptr<simulation::mission_instruments> instruments;
  simulation::daylight daylight;
  render::landscape world;
  render::landscape_options visual;
  render::cockpit panels;
  render::avionics_renderer avionics;
  render::framebuffer frame;
  simulation::view_frame camera;
  bool paused{false}, finished{false}, night_vision{false};
  bool quit_prompt{false}, help_requested{false};
  unsigned int accelerated_time{1};
  std::function<void(unsigned int, int32_t, int32_t)> play_sound;
  std::function<void(simulation::flight_event)> flight_sound;
  // The transport services incoming packets here, at the original render barrier.
  std::function<void()> wait_network, poll_network;
  std::function<int32_t()> network_clock;
  int32_t network_wait_ticks{0};
  mission_session(resources::data_store const &data, int mission_id, mission_options options = {});
  mission_session(resources::data_store const &data, resources::mission_briefing briefing, mission_options options = {}, resources::mission_definition const *edited = nullptr);
  mission_options const &launch_options() const { return settings; }
  static resources::mission_briefing quick_start(resources::data_store const &data, int zone);
  mission_session(mission_session const &) = delete;
  mission_session &operator=(mission_session const &) = delete;
  void key(uint16_t scan, bool down, bool dispatch = true);
  void post(uint16_t key) { commands.push_back(key); }
  void select_view(unsigned int mode);
  void request_end(unsigned int cause);
  void detail(unsigned int level);
  void step(unsigned int ticks);
  void initialise_remote_players();
  void send_server_frame();
  void landing_award();
  bool resize(unsigned int width, unsigned int height);
  void draw();
};
} // namespace apache::frontend
