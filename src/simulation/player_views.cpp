#include "player_views.h"
#include <algorithm>
#include <stdexcept>

namespace apache::simulation {
player_views::player_views(resources::executable_data const &executable, math::trigonometry const &angles, std::function<int32_t(int32_t, int32_t)> ground, player_targeting &sensors, unsigned int const control_mode)
  : trig{angles}, height{std::move(ground)}, targeting{sensors}, geometry{trig, height}, modes{executable, control_mode}, optics{executable} {
  context.control_mode = control_mode;
  spectator_pose.position = {0x8000000, 0x8000000, height(0x8000000, 0x8000000)};
  spectator.pitch = crash.pitch = 0;
}
void player_views::remember(math::pose const &camera) {
  spectator_pose = camera;
  auto const angles{trig.angles(camera.rotation)};
  spectator.heading = angles[0]; spectator.pitch = std::bit_cast<int32_t>(angles[1]);
}
void player_views::leave(unsigned int const mode) {
  // Both original exit paths restore the helmet's saved pair.
  if((mode == 12 || mode == 13) && !targeting.locked) { targeting.tads = helmet_targeting[0]; targeting.automatic = helmet_targeting[1]; }
}
void player_views::enter(unsigned int const mode, view_sources const &sources, uint32_t &random_seed) {
  switch(mode) {
    case 12: case 13: {
      auto &look{mode == 12 ? helmet : goggles}; look = {};
      (mode == 12 ? helmet_previous : goggles_previous) = modes.mode;
      (mode == 12 ? helmet_targeting : goggles_targeting) = {targeting.tads, targeting.automatic};
      if(!(sources.faults & 0x2000)) { targeting.tads = true; targeting.automatic = false; }
      break;
    }
    case 14: if(modes.mode != 15) context.current_weapon = context.latest_weapon; break;
    case 15: if(modes.mode != 14) context.current_weapon = context.latest_weapon; break;
    case 16: if(modes.mode == 16) remember(sources.player.position); break;
    case 17:
      remote_position = sources.player.position.position;
      remote_position[2] = std::max(remote_position[2], math::add(height(remote_position[0], remote_position[1]), 1024)); break;
    case 19:
      crash_position = sources.player.position.position;
      random_seed = random_seed * 0x41c64e6du + 0x3039u;
      crash.heading = (random_seed & 0x7fff0000u) * 2; crash.pitch = 0;
      air_crash = math::subtract(crash_position[2], height(crash_position[0], crash_position[1])) >= 0xa01;
      context.crash_timer = 0;
      if(crashed) crashed(air_crash);
      break;
    default: break;
  }
}
bool player_views::select(unsigned int const mode, view_sources const &sources, uint32_t &random_seed) {
  context.drone = sources.drone.has_value(); context.target_kind = static_cast<unsigned int>(targeting.current.identity.kind);
  return modes.select(mode, context, {.enter{[&](unsigned int id){ enter(id, sources, random_seed); }}, .leave{[&](unsigned int id){ leave(id); }}});
}
bool player_views::select_optics(view_sources const &sources, uint32_t &random_seed) {
  return modes.mode == 13 ? optics.cycle(13, sources.faults) : select(13, sources, random_seed);
}
void player_views::removed(size_t const round) {
  if(context.current_weapon == round) context.current_weapon.reset();
  if(context.latest_weapon == round) context.latest_weapon.reset();
}
view_frame player_views::update(view_sources const &sources, camera_input const &input, uint32_t &random_seed) {
  view_frame result;
  auto const weapon{[&]()->std::optional<view_subject> { return context.current_weapon && sources.weapon ? sources.weapon(*context.current_weapon) : std::nullopt; }};
  if(modes.mode == 13) optics.check(sources.faults);
  else if(modes.mode == 14 || modes.mode == 15) {
    if(!weapon()) { context.current_weapon.reset(); select(16, sources, random_seed); } else if(modes.mode == 15 && (!context.has_target || !context.has_target(*context.current_weapon) || !context.has_lock || !context.has_lock(*context.current_weapon))) select(14, sources, random_seed);
  } else if(modes.mode == 18 && targeting.current.identity.kind != target_kind::scenery && targeting.current.identity.kind != target_kind::mobile) select(modes.home, sources, random_seed);
  else if(modes.mode == 19) {
    auto const timer{int64_t{context.crash_timer} + input.delta / 4};
    context.crash_timer = static_cast<int32_t>(std::min<int64_t>(timer, INT32_MAX));
    result.finished = timer > INT32_MAX && ((!context.network && !context.serial_connection) || (context.network && context.game_mode == 3));
    if(context.game_mode != 2 && context.game_mode != 6 && context.crash_timer > 0x40000000) result.fade = math::add(context.crash_timer - 0x40000000, context.crash_timer - 0x40000000);
  }
  auto const track{[&](view_subject const &subject){ return geometry.tracking(tracking, subject.position, subject.model, subject.radius, input); }};
  if(modes.mode <= 9) {
    std::array<uint32_t, 5> constexpr headings{0, 0xe0000000, 0xc0000000, 0x20000000, 0x40000000};
    result.camera = geometry.head(sources.player.position, headings[modes.mode % 5]); result.sight = true;
  } else switch(modes.mode) {
    case 10: result.camera = track(sources.player); break;
    case 11: result.camera = track(sources.drone.value_or(sources.player)); break;
    case 12: case 13: {
      std::optional<math::vector3> aim;
      if(targeting.locked && sources.target) aim = sources.target->position;
      if(modes.mode == 12) result.camera = geometry.helmet(helmet, sources.player.position, input, aim);
      else { auto const &zoom{optics.magnification()}; result.camera = geometry.goggles(goggles, sources.player.position, input, aim, zoom.rate, zoom.distance); result.infrared = optics.device == 1; }
      result.sight = true; break;
    }
    case 14: result.camera = track(weapon().value_or(sources.player)); break;
    case 15: {
      auto const target{sources.weapon_target ? sources.weapon_target(*context.current_weapon) : std::nullopt};
      result.camera = target ? geometry.missile_target(weapon()->position.position, target->position, target->height) : track(*weapon()); break;
    }
    case 16: result.camera = geometry.spectator(spectator, spectator_pose.position, input); break;
    case 17: result.camera = geometry.remote(remote_position, sources.player.position.position); break;
    case 18: result.camera = geometry.target_lock(sources.player.position.position, sources.target ? sources.target->position : sources.player.position.position); result.sight = true; break;
    case 19: result.camera = geometry.crash(crash, crash_position, air_crash, input.delta); break;
    default: throw std::runtime_error{"No active mission view"};
  }
  if(modes.mode != 16) remember(result.camera);
  return result;
}
} // namespace apache::simulation
