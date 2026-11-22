#include "remote_weapons.h"

namespace apache::simulation {
std::vector<remote_shot> remote_weapon_shots(remote_fire_input const &input, std::function<int32_t(int32_t, int32_t)> const &height) {
  if(input.weapon == 0 || input.weapon > 19) return {};
  remote_shot shot{remote_fire_kind::unguided, {}, input.seed};
  auto &request{shot.request};
  request.weapon = input.weapon;
  request.source = input.source;
  request.launch.pose = input.aircraft;
  request.launch.source = input.movement;
  switch(input.weapon) {
  case 1:
  case 19:
    {
    static auto const definitions{weapon_definitions()};
    auto const &weapon{definitions[input.weapon]};
    math::vector3 displacement;
    auto const velocity{weapon.launch_speed / weapon.fire_interval_ticks};
    for(size_t i{0}; i < 3; ++i) {
      request.launch.pose.rotation[3 + i] = input.gun_direction[i];
      displacement[i] = math::multiply_q31(input.gun_direction[i], velocity);
    }
    std::vector<remote_shot> shots;
    for(size_t i{0}; i < 4; ++i) {
      auto next{request.launch.pose.position};
      for(size_t j{0}; j < 3; ++j) next[j] = math::add(next[j], displacement[j]);
      if(!height || height(next[0], next[1]) <= next[2]) request.launch.pose.position = next;
      shots.push_back(shot);
    }
    return shots;
  }
  case 2:
  case 18:
    request.designator = input.source;
    [[fallthrough]];
  case 3:
  case 6:
  case 9:
  case 10:
    shot.kind = remote_fire_kind::guided;
    request.guided_constructor = true;
    request.selected = input.target;
    break;
  case 4:
  case 5:
  case 16:
  case 17:
    break;
  case 7:
    shot.kind = remote_fire_kind::chaff;
    break;
  case 8:
    shot.kind = remote_fire_kind::flare;
    break;
  default:
    shot.kind = remote_fire_kind::simple;
    break;
  }
  return std::vector<remote_shot>(input.weapon == 11 ? 35u : 1u, shot);
}
} // namespace apache::simulation
