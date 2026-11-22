#pragma once
#include "instrument_state.h"
#include "mission_runtime.h"
#include "render/avionics.h"

namespace apache::simulation {
// Cockpit values are sampled from the same mission objects as collision and targeting.
class mission_instruments {
  mission_runtime &mission;
  render::helmet_display helmet;
  std::optional<instrument_state> sampled;
  instrument_state flight_display() const;
  std::optional<render::model_instance> ground_target;
  void radar_contacts(std::span<radar_return const> contacts, bool tactical, std::vector<render::radar_contact> &output) const;
public:
  render::avionics_instruments state;
  mission_instruments(mission_runtime &runtime, unsigned int video_mode, unsigned int game_mode = 0);
  void sample() {
    sampled = flight_display();
  }
  void update(unsigned int view, math::pose const &camera, render::projection const &projection);
};
} // namespace apache::simulation
