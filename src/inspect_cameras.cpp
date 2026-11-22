#include "inspect_cameras.h"
#include <iomanip>
#include <istream>
#include <memory>
#include <ostream>
#include <stdexcept>
#include "simulation/camera_geometry.h"
#include "simulation/daylight.h"
#include "simulation/player_views.h"
#include "simulation/sight_geometry.h"
#include "simulation/view_modes.h"

void inspect_cameras(apache::resources::data_store const &data, std::istream &input, std::ostream &output) {
    using apache::simulation::camera_geometry;
  using apache::simulation::camera_input;
  using apache::simulation::chain_gun_impact;
  using apache::simulation::look_camera;
  using apache::simulation::optical_devices;
  using apache::simulation::orbit_camera;
  using apache::simulation::player_targeting;
  using apache::simulation::player_views;
  using apache::simulation::view_context;
  using apache::simulation::view_events;
  using apache::simulation::view_modes;
  using apache::simulation::view_sources;
  using apache::simulation::view_subject;
  using apache::simulation::view_target;
  apache::math::trigonometry trig{data}; apache::resources::executable_data executable{data}; std::string kind;
  std::unique_ptr<player_views> views; player_targeting targeting; uint32_t seed{1}; int32_t floor{0};
  while(input >> kind) {
    if(kind == "daylight") {
      int32_t time{0}, now{0}, update{0}; input >> time >> now >> update;
      apache::simulation::daylight state{executable, time, now}; state.update(update);
      output << state.light.heading << ' ' << state.light.pitch << ' ' << state.light.night << ' ' << state.night_vision() << '\n'; continue;
    }
    if(kind == "gun-impact") {
      apache::math::pose barrel; int32_t speed{0}, ground{0}; bool selected{false}; apache::math::vector3 intercept;
      input >> speed >> ground >> selected; for(auto &v : barrel.position) input >> v; for(auto &v : barrel.rotation) input >> v; for(auto &v : intercept) input >> v;
      auto const result{chain_gun_impact(barrel, speed, selected ? std::optional{intercept} : std::nullopt, [&](int32_t, int32_t){ return ground; })};
      output << result[0] << ' ' << result[1] << ' ' << result[2] << '\n'; continue;
    }
    if(kind == "camera-reset") {
      unsigned int control{2}; input >> control >> seed >> floor; targeting = player_targeting{};
      views = std::make_unique<player_views>(executable, trig, [&](int32_t, int32_t){ return floor; }, targeting, control); continue;
    }
    if(kind == "camera-step") {
      if(!views) throw std::runtime_error{"Camera sequence not initialised"};
      auto &v{*views}; view_sources sources; camera_input controls; unsigned int action{0}, value{0}, target_kind{0}, valid{0}, targets{0}, locks{0}; bool drone{false};
      input >> action >> value >> controls.delta >> controls.turn >> controls.pitch >> controls.zoom >> sources.faults >> targeting.locked >> targeting.tads >> targeting.automatic >> target_kind >> drone >> valid >> targets >> locks;
      auto const read_pose{[&](apache::math::pose &pose){ for(auto &i : pose.position) input >> i; for(auto &i : pose.rotation) input >> i; }};
      sources.player.model = 208; sources.player.radius = 5000; read_pose(sources.player.position);
      view_target aim; aim.height = 12000; for(auto &i : aim.position) input >> i;
      targeting.current.identity.kind = static_cast<apache::simulation::target_kind>(target_kind); if(target_kind || targeting.locked) sources.target = aim;
      view_subject other; other.model = 34; other.radius = 7000; read_pose(other.position); if(drone) sources.drone = other;
      std::array<view_subject, 2> rounds; rounds[0].model = 103; rounds[1].model = 607; for(auto &round : rounds) read_pose(round.position);
      sources.weapon = [&](size_t i)->std::optional<view_subject> { return (valid & (1u << i)) ? std::optional{rounds.at(i)} : std::nullopt; };
      sources.weapon_target = [&](size_t)->std::optional<view_target> { return aim; };
      v.context.has_target = [&](size_t i){ return (targets & (1u << i)) != 0; }; v.context.has_lock = [&](size_t i){ return (locks & (1u << i)) != 0; };
      for(size_t i{0}; i != 2; ++i) if(!(valid & (1u << i))) v.removed(i);
      int crash_event{0}; v.crashed = [&](bool air){ crash_event = air ? 2 : 1; };
      int accepted{-1};
      if(action == 0) accepted = v.select(value, sources, seed); else if(action == 1) accepted = v.select_optics(sources, seed); else if(action == 2) v.optics.zoom(v.modes.mode, value != 0); else if(action == 4) v.context.latest_weapon = value;
      auto const frame{v.update(sources, controls, seed)};
      output << accepted << ' ' << v.modes.mode << ' ' << v.modes.crew << ' ' << v.modes.in_cockpit << ' ' << v.modes.movement << ' ' << v.modes.changed << ' ' << targeting.tads << ' ' << targeting.automatic << ' ' << (v.context.current_weapon ? static_cast<int>(*v.context.current_weapon) : -1) << ' ' << (v.context.latest_weapon ? static_cast<int>(*v.context.latest_weapon) : -1) << ' ' << seed << ' ' << v.context.crash_timer << ' ' << v.air_crash << ' ' << v.optics.device;
      for(auto i : v.optics.selected) output << ' ' << i;
      output << ' ' << v.tracking.heading << ' ' << v.tracking.pitch << ' ' << v.tracking.zoom << ' ' << v.spectator.heading << ' ' << v.spectator.pitch << ' ' << v.crash.heading << ' ' << v.crash.pitch << ' ' << v.helmet.heading << ' ' << v.helmet.pitch << ' ' << v.goggles.heading << ' ' << v.goggles.pitch << ' ' << v.helmet_previous << ' ' << v.goggles_previous;
      for(auto const &pair : {v.helmet_targeting, v.goggles_targeting}) for(auto i : pair) output << ' ' << i;
      output << ' ' << frame.finished << ' ' << frame.fade << ' ' << frame.sight << ' ' << frame.infrared << ' ' << crash_event;
      for(auto const &pose : {frame.camera, v.spectator_pose}) { for(auto i : pose.position) output << ' ' << i; for(auto i : pose.rotation) output << ' ' << i; }
      for(auto const &position : {v.remote_position, v.crash_position}) for(auto i : position) output << ' ' << i;
      output << '\n'; continue;
    }
    if(kind == "view-mode") {
      view_modes state{executable}; view_context context; unsigned int requested{0}; int current{-1}, latest{-1}; std::array<bool, 2> targets{}, locks{};
      input >> state.mode >> state.crew >> state.in_cockpit >> state.movement >> state.changed >> requested >> context.control_mode >> context.following >> context.network >> context.serial_connection >> context.drone >> context.crash_timer >> context.target_kind >> current >> latest;
      for(auto &v : targets) input >> v; for(auto &v : locks) input >> v;
      if(current >= 0) context.current_weapon = static_cast<size_t>(current); if(latest >= 0) context.latest_weapon = static_cast<size_t>(latest);
      std::vector<unsigned int> calls;
      context.has_target = [&](size_t i){ calls.insert(calls.end(), {1, static_cast<unsigned int>(i)}); return targets.at(i); };
      context.has_lock = [&](size_t i){ calls.insert(calls.end(), {2, static_cast<unsigned int>(i)}); return locks.at(i); };
      view_events events{.enter{[&](unsigned int id){ calls.insert(calls.end(), {4, id}); }}, .leave{[&](unsigned int id){ calls.insert(calls.end(), {3, id}); }}};
      bool const selected{state.select(requested, context, events)};
      output << selected << ' ' << state.mode << ' ' << state.crew << ' ' << state.in_cockpit << ' ' << state.movement << ' ' << state.changed << ' ' << state.left_view() << ' ' << state.right_view();
      for(unsigned int video{0}; video != 3; ++video) for(auto v : state.viewport(video)) output << ' ' << v;
      output << ' ' << calls.size() / 2; for(auto v : calls) output << ' ' << v; output << '\n'; continue;
    }
    if(kind == "optical") {
      optical_devices state{executable}; unsigned int action{0}, value{0}, view{0}; uint32_t faults{0}; input >> action >> value >> view >> faults >> state.device; for(auto &v : state.selected) input >> v;
      if(action == 0) state.select(value, faults); else if(action == 1) state.cycle(view, faults); else if(action == 2) state.check(faults); else state.zoom(view, value != 0);
      output << state.device; for(auto v : state.selected) output << ' ' << v; auto const &level{state.magnification()}; output << ' ' << level.distance << ' ' << level.rate << ' ' << std::quoted(state.name()) << ' ' << std::quoted(level.label) << '\n'; continue;
    }
    orbit_camera state; camera_input controls; apache::math::pose body, result; apache::math::vector3 target; uint32_t model{0}; int32_t radius{0}, target_height{0}, floor{0}, rate{0}, distance{0}; bool locked{false};
    input >> state.heading >> state.pitch >> state.zoom >> controls.delta >> controls.turn >> controls.pitch >> controls.zoom >> model >> radius >> target_height >> floor;
    for(auto &v : body.position) input >> v; for(auto &v : body.rotation) input >> v; for(auto &v : target) input >> v; input >> locked >> rate >> distance;
    camera_geometry camera{trig, [&](int32_t, int32_t){ return floor; }};
    if(kind == "head") result = camera.head(body, state.heading, static_cast<uint32_t>(state.pitch));
    else if(kind == "tracking") result = camera.tracking(state, body, model, radius, controls);
    else if(kind == "spectator") result = camera.spectator(state, body.position, controls);
    else if(kind == "remote") result = camera.remote(body.position, target);
    else if(kind == "missile") result = camera.missile_target(body.position, target, target_height);
    else if(kind == "target") result = camera.target_lock(body.position, target);
    else if(kind == "crash") result = camera.crash(state, body.position, locked, controls.delta);
    else if(kind == "magnify") { result = body; camera.magnify(result, distance); } else if(kind == "helmet" || kind == "goggles") {
      look_camera look{std::bit_cast<int32_t>(state.heading), state.pitch};
      std::optional<apache::math::vector3> aim; if(locked) aim = target;
      result = kind == "helmet" ? camera.helmet(look, body, controls, aim) : camera.goggles(look, body, controls, aim, rate, distance);
      state.heading = static_cast<uint32_t>(look.heading); state.pitch = look.pitch;
    } else throw std::runtime_error{"Unknown camera geometry command"};
    if(!input) throw std::runtime_error{"Incomplete camera geometry command"};
    output << state.heading << ' ' << state.pitch << ' ' << state.zoom;
    for(auto v : result.position) output << ' ' << v; for(auto v : result.rotation) output << ' ' << v; output << '\n';
  }
}
