#include "inspect_session.h"
#include <fstream>
#include <iomanip>
#include <istream>
#include <ostream>
#include <stdexcept>
#include "frontend/mission_session.h"

void inspect_session(apache::resources::data_store const &data, int const mission, unsigned int const video, bool const arcade, std::istream &input, std::ostream &output, int const shared_seat, int const network_mode, unsigned int const local, unsigned int const players) {
  apache::frontend::mission_options options{
    .arcade{arcade},
    .video_mode{video},
    .detail{9}
  };
  if(shared_seat >= 0) options.shared_seat = static_cast<apache::multiplayer::crew_role>(shared_seat);
  if(network_mode > 0) {
    if(players < 2 || players > 16 || local >= players) throw std::runtime_error{"Invalid network fixture roster"};
    options.connection.emplace();
    auto &connection{*options.connection};
    connection.local = local;
    connection.game.mode = static_cast<apache::multiplayer::game_mode>(network_mode);
    connection.game.second_seat = local != 0;
    for(size_t i{0}; i < 16; ++i) {
      auto &player{connection.players[i]};
      player.flags = i < players ? static_cast<uint32_t>(i << 8) : 0xff00;
      player.name = i < players ? "PLAYER" + std::to_string(i) : "";
      player.aircraft.model = i < players ? (network_mode == 6 && i != local ? 222 : 64) : 0;
    }
  }
  auto owner{mission == 0 && network_mode > 0 ? std::make_unique<apache::frontend::mission_session>(data, apache::resources::mission_briefing{
    .id{0},
    .flow{network_mode == 5 ? 6 : 5},
    .zone{2}
  }, options) : mission < 0 ? std::make_unique<apache::frontend::mission_session>(data, apache::frontend::mission_session::quick_start(data, -mission), options) : std::make_unique<apache::frontend::mission_session>(data, mission, options)};
  auto &session{*owner};
  std::vector<apache::resources::bytes> incoming;
  bool packet_mode{false};
  auto const hex{[&](std::span<uint8_t const> bytes){
    for(auto b : bytes) output << std::hex << std::setfill('0') << std::setw(2) << static_cast<unsigned int>(b);
    output << std::dec << std::setfill(' ');
  }};
  if(session.network) {
    session.network->send = [&](apache::multiplayer::packet const &message){
      if(packet_mode) {
        output << "send ";
        hex(message.bytes());
        output << '\n';
      } else if(message.type() == apache::multiplayer::message_type::crew) {
        output << "transmit ";
        hex(message.bytes().subspan(24, 104));
        output << '\n';
      }
    };
    session.wait_network = [&]{
      for(auto const &bytes : incoming) {
        if(bytes.size() != 104) {
          auto const packet{apache::multiplayer::packet::decode(bytes)};
          if(!packet) throw std::runtime_error{"Invalid incoming APNET fixture"};
          session.network->receive(*packet, session.runtime.timing.now);
          continue;
        }
        auto const message{apache::multiplayer::decode_crew(shared_seat ? apache::multiplayer::crew_role::pilot : apache::multiplayer::crew_role::gunner, bytes)};
        if(!message) throw std::runtime_error{"Invalid incoming crew fixture"};
        session.crew->receive(*message);
      }
      incoming.clear();
    };
  }
  std::string command;
  while(input >> command) {
    if(command == "packet-mode") {
      packet_mode = true;
    } else if(command == "receive" || command == "receive-packet") {
      if(!session.network) throw std::runtime_error{"Network session required"};
      std::string bytes;
      input >> bytes;
      if(command == "receive" ? bytes.size() != 208 : bytes.size() != 816 && bytes.size() != 1032) throw std::runtime_error{"Invalid incoming packet length"};
      apache::resources::bytes decoded;
      for(size_t i{0}; i < bytes.size(); i += 2) decoded.push_back(static_cast<uint8_t>(std::stoul(bytes.substr(i, 2), nullptr, 16)));
      incoming.push_back(std::move(decoded));
    } else if(command == "network-ready") {
      if(!session.network) throw std::runtime_error{"Network session required"};
      session.network->state.server_in_game = true;
    } else if(command == "network-poll") {
      if(session.wait_network) session.wait_network();
    } else if(command == "server-frame") {
      session.send_server_frame();
    } else if(command == "network-status") {
      if(!session.network) throw std::runtime_error{"Network session required"};
      auto const &n{*session.network};
      output << "network " << static_cast<unsigned int>(n.lifecycle.mode) << ' ' << n.state.local << ' ' << n.state.server << ' ' << n.state.client_server << ' ' << session.runtime.move_actors << ' ' << *session.runtime.timing.player << ' ' << session.runtime.player_armament->control_mode << '\n';
      for(size_t i{0}; i < 16; ++i) {
        auto const &p{n.state.players[i]};
        output << "peer " << i << ' ' << (n.actor(static_cast<uint32_t>(i)) ? static_cast<int64_t>(*n.actor(static_cast<uint32_t>(i))) : -1) << ' ' << p.aircraft.model << ' ' << p.flags;
        for(auto v : p.aircraft.location.position) output << ' ' << v;
        for(auto v : p.aircraft.location.rotation) output << ' ' << v;
        output << '\n';
      }
    } else if(command == "crew-status") {
      auto const &arm{*session.runtime.player_armament};
      output << "crew-status " << session.finished << ' ' << session.ending.countdown << ' ' << arm.control_mode << ' ' << arm.inventory.automatic_chaff;
      for(auto v : arm.inventory.deadlines) output << ' ' << v;
      output << '\n';
    } else if(command == "step") {
      unsigned int ticks;
      input >> ticks;
      session.step(ticks);
    } else if(command == "key") {
      unsigned int key;
      bool down;
      input >> key >> down;
      session.key(static_cast<uint16_t>(key), down, false);
    } else if(command == "post") {
      unsigned int key;
      input >> key;
      session.post(static_cast<uint16_t>(key));
    } else if(command == "joystick") {
      auto &c{session.controls};
      input >> c.configuration.cyclic >> c.joystick_present >> c.hat_present >> c.hat >> c.buttons;
      c.configuration.resolve();
    } else if(command == "modal") {
      output << "modal " << session.quit_prompt << ' ' << session.help_requested << ' ' << session.finished << ' ' << session.runtime.timing.now << '\n';
    } else if(command == "view") {
      unsigned int mode;
      input >> mode;
      session.select_view(mode);
    } else if(command == "detail") {
      unsigned int value;
      input >> value;
      session.detail(value);
    } else if(command == "flight") {
      auto const &f{session.runtime.pilot->realistic};
      auto const &d{f.dynamics};
      output << "flight" << std::hex;
      for(auto const value : {d.x, d.y, d.u, d.v, f.dx, f.dy, d.zdot}) output << ' ' << std::bit_cast<uint64_t>(value);
      output << std::dec << ' ' << d.wind_strength << ' ' << d.wind_gust << '\n';
    } else if(command == "combat-targets") {
      if(!session.network) throw std::runtime_error{"Network session required"};
      auto const &runtime{session.runtime};
      for(size_t i{0}; i < runtime.actors.units.size(); ++i) {
        auto const &a{runtime.actors.units[i]};
        auto const &arm{runtime.combat.armaments[i]};
        output << "combat " << i << ' ' << a.serial << ' ' << (a.conditions & ~0x8000u) << ' ' << a.network << ' ' << a.weapon << ' ' << a.weapon_slot;
        for(auto v : arm.deadlines) output << ' ' << v;
        output << ' ' << runtime.combat.attention[i].range << ' ' << a.random;
        auto const target{arm.target ? session.network->identify(*arm.target) : apache::multiplayer::target_state{}};
        output << ' ' << target.kind;
        for(auto v : target.data) output << ' ' << v;
        output << '\n';
      }
    } else if(command == "scene-state") {
      for(size_t index{0}; index < session.runtime.actors.units.size(); ++index) {
        auto const &actor{session.runtime.actors.units[index]};
        output << "actor " << index << ' ' << actor.object.id << ' ' << actor.object.status << ' ' << actor.destroyed;
        for(auto v : actor.object.world.position) output << ' ' << v;
        for(auto v : actor.object.world.rotation) output << ' ' << v;
        output << '\n';
      }
      for(size_t index{0}; index < session.runtime.debris.pieces.size(); ++index) {
        auto const &piece{session.runtime.debris.pieces[index]};
        if(!piece.active) continue;
        output << "fragment " << index << " 1 " << piece.model << ' ' << piece.flags;
        for(auto v : piece.position.position) output << ' ' << static_cast<uint32_t>(v);
        for(auto v : piece.position.rotation) output << ' ' << static_cast<uint32_t>(v);
        for(auto v : piece.axis) output << ' ' << static_cast<uint32_t>(v);
        output << ' ' << static_cast<uint32_t>(piece.spin);
        for(auto v : piece.velocity) output << ' ' << static_cast<uint32_t>(v);
        output << '\n';
      }
    } else if(command == "projectiles") {
      auto const &pool{session.runtime.weapons->rounds};
      for(size_t index{0}; index < pool.entries().size(); ++index) {
        if(pool.entries()[index]) {
          auto const &r{pool.entries()[index]->state};
          output << "projectile " << index << ' ' << r.weapon << ' ' << r.launched_at << ' ' << r.random << ' ' << r.body.motion.distance << ' ' << r.body.motion.speed;
          for(auto v : r.body.pose.position) output << ' ' << v;
          output << '\n';
        }
      }
    } else if(command == "attachments") {
      auto const visit{[&](auto &&self, apache::render::model_instance const &object)->void {
        output << "attachment " << object.id;
        for(auto const &pose : {object.world, object.relative}) {
          for(auto v : pose.position) output << ' ' << v;
          for(auto v : pose.rotation) output << ' ' << v;
        }
        output << '\n';
        for(auto const &child : object.children) {
          if(child) self(self, *child);
        }
      }};
      visit(visit, session.runtime.actors.units[*session.runtime.timing.player].object);
    } else if(command == "record") {
      apache::simulation::debrief_record record;
      for(auto &v : record.words) input >> v;
      session.records.add(record);
    } else if(command == "fixture_crash_timer") {
      input >> session.views->context.crash_timer;
    } else if(command == "fixture_position") {
      apache::math::vector3 value;
      for(auto &v : value) input >> v;
      session.runtime.pilot->reposition(value);
    } else if(command == "fixture_landing_award") {
      session.landing_award();
    } else if(command == "fixture_death") {
      unsigned int death;
      input >> death;
      if(arcade) {
        session.runtime.pilot->arcade.death = death;
      } else {
        session.runtime.pilot->realistic.death = death;
      }
    } else if(command == "fixture_targets") {
      bool outstanding;
      input >> outstanding;
      auto &runtime{session.runtime};
      unsigned int marked{0};
      for(auto const &group : runtime.actors.groups) {
        for(auto index : group.units) {
          auto const &actor{runtime.actors.units[index]};
          if((runtime.actors.units[group.units.front()].conditions & (outstanding ? 0x6000 : 0x2000)) && (actor.object.status & 6) == 4) {
            runtime.combat.kill(index);
            session.records.add({{8, 0, runtime.spatial.mobile_id(*runtime.timing.player), 0, 0, runtime.spatial.mobile_id(index)}});
            ++marked;
          }
        }
      }
      auto const &actor{runtime.actors.units[*runtime.timing.player]};
      for(auto const &waypoint : runtime.mission.tasks[actor.location.task].actors[actor.location.group].waypoints) {
        if(waypoint.type == 4) {
          if(auto target{apache::simulation::nearest_ground_object(runtime.scene, waypoint.position[0], waypoint.position[1])}) {
            (target->layer ? runtime.scene.extra : runtime.scene.base).at(target->index).status |= 1;
            ++marked;
          }
        }
      }
      output << "marked " << marked << '\n';
    } else if(command == "debrief") {
      uint32_t seed;
      std::string name;
      input >> seed >> std::quoted(name);
      auto const result{apache::simulation::evaluate_debrief(session.runtime, session.records, seed, name)};
      output << "debrief";
      for(auto v : {static_cast<uint32_t>(result.mission), static_cast<uint32_t>(result.targets), static_cast<uint32_t>(result.player), result.pilot, static_cast<uint32_t>(result.aborted), static_cast<uint32_t>(result.dead), static_cast<uint32_t>(result.landed_in_enemy), static_cast<uint32_t>(result.front_distance), result.time, seed}) output << ' ' << v;
      output << ' ' << std::quoted(result.title()) << ' ' << result.explanations.size();
      for(auto const &line : result.explanations) output << ' ' << std::quoted(line);
      output << '\n';
      apache::simulation::pilot_log log;
      log.name = "Reference pilot";
      log.rank = 2;
      log.status_hour = 500000;
      log.missions.push_back({static_cast<uint32_t>(mission), apache::simulation::mission_result::untried});
      if(session.briefing.flow == 4) log.campaign = {static_cast<uint32_t>(session.briefing.zone), static_cast<uint32_t>(session.briefing.level), 4, 12345, {{static_cast<uint32_t>(mission), apache::simulation::mission_result::untried}}};
      result.log(log, static_cast<uint32_t>(mission), static_cast<unsigned int>(session.briefing.flow), 500000);
      output << "log ";
      for(auto byte : log.encode()) output << std::hex << std::setfill('0') << std::setw(2) << static_cast<unsigned int>(byte);
      output << std::dec << std::setfill(' ') << '\n';
    } else if(command == "draw") {
      std::string path;
      input >> std::quoted(path);
      session.draw();
      if(path != "-") {
        apache::resources::write_ppm(path + ".ppm", session.frame.image);
        std::ofstream pixels{path + ".pixels", std::ios::binary};
        pixels.write(reinterpret_cast<char const *>(session.frame.image.pixels.data()), static_cast<std::streamsize>(session.frame.image.pixels.size()));
        std::ofstream palette{path + ".palette", std::ios::binary};
        palette.write(reinterpret_cast<char const *>(session.world.palette.modified.data()), 768);
      }
      size_t const panel_row{session.views->modes.mode == 0 || session.views->modes.mode == 5 ? static_cast<size_t>(session.views->modes.viewport(video)[3]) : session.frame.image.height};
      std::vector<apache::render::rectangle> excluded;
      // An isolated FLIR target can contain the same missing normals as the main
      // scene. Diagnose each active 3D display without excluding other instruments.
      auto &state{session.instruments->state};
      auto const missing{session.world.missing_normals};
      auto const invalid{session.world.invalid_texture_samples};
      if((missing || invalid) && state.target_model && (state.helmet.view == 0 || state.helmet.view == 5)) {
        apache::render::mfd_renderer screens{session.panels, session.runtime.executable};
        auto probe{session.frame};
        bool const gunner{state.helmet.view == 5};
        for(unsigned int i{0}; i != 3; ++i) {
          if(i < 2 && state.displays.pages[gunner][i] != apache::simulation::mfd_page::flir) continue;
          if(i == 2 && (!gunner || (state.helmet.faults & 0x100))) continue;
          auto const area{screens.area(static_cast<apache::render::mfd_panel>(i == 2 ? 4 : (gunner ? 2 : 0) + i))};
          probe.clip = area;
          session.world.missing_normals = session.world.invalid_texture_samples = 0;
          session.world.draw_flir(probe, *state.target_model, state.target_camera_aim, state.helmet.aircraft.position, session.visual.lighting);
          if(session.world.missing_normals || session.world.invalid_texture_samples) excluded.push_back(area);
        }
        session.world.missing_normals = missing;
        session.world.invalid_texture_samples = invalid;
      }
      apache::resources::bytes panel_pixels;
      for(unsigned int y{static_cast<unsigned int>(panel_row)}; y < session.frame.image.height; ++y) {
        for(unsigned int x{0}; x < session.frame.image.width; ++x) {
          bool skip{false};
          for(auto const &area : excluded) skip = skip || area.contains(static_cast<int>(x), static_cast<int>(y));
          if(!skip) panel_pixels.push_back(session.frame.image.pixels[y * session.frame.image.width + x]);
        }
      }
      output << "render " << missing << ' ' << invalid << ' ' << apache::resources::fingerprint(session.frame.image.pixels) << ' ' << apache::resources::fingerprint(session.world.palette.modified) << ' ' << apache::resources::fingerprint(panel_pixels) << ' ' << excluded.size();
      for(auto const &area : excluded) output << ' ' << area.left << ' ' << area.top << ' ' << area.right << ' ' << area.bottom;
      output << '\n';
    } else if(command == "snapshot") {
      auto &runtime{session.runtime};
      auto const &flight{runtime.pilot->readings()};
      auto const &arm{*runtime.player_armament};
      auto const &sensors{runtime.sensors->state};
      auto const &v{session.views->modes};
      output << "state " << runtime.timing.now << ' ' << v.mode << ' ' << v.crew << ' ' << v.in_cockpit << ' ' << v.movement << ' ' << session.night_vision << ' ' << session.accelerated_time;
      for(auto x : flight.position.position) output << ' ' << x;
      for(auto x : flight.position.rotation) output << ' ' << x;
      for(auto x : flight.angles) output << ' ' << x;
      output << ' ' << flight.airspeed << ' ' << flight.sidespeed << ' ' << flight.climb << ' ' << flight.torque << ' ' << flight.fuel << ' ' << runtime.pilot->faults() << ' ' << runtime.pilot->death() << ' ' << runtime.pilot->landed();
      for(auto x : flight.engines) output << ' ' << static_cast<int32_t>(x);
      output << ' ' << runtime.random.global_seed << ' ' << arm.inventory.current << ' ' << arm.inventory.armed << ' ' << arm.inventory.helmet_mode;
      for(auto x : arm.inventory.ammunition) output << ' ' << x;
      output << ' ' << sensors.ground << ' ' << sensors.air << ' ' << sensors.tads << ' ' << sensors.locked << ' ' << sensors.automatic << ' ' << sensors.frozen << ' ' << sensors.threat;
      for(auto const &pair : session.instruments->state.displays.pages) {
        for(auto page : pair) output << ' ' << static_cast<unsigned int>(page);
      }
      output << ' ' << session.instruments->state.map_range << '\n';
      output << "camera";
      for(auto x : session.camera.camera.position) output << ' ' << x;
      for(auto x : session.camera.camera.rotation) output << ' ' << x;
      output << '\n';
    } else {
      throw std::runtime_error{"Unknown live-session inspection command: " + command};
    }
    if(!input) throw std::runtime_error{"Incomplete live-session inspection command"};
  }
}

void inspect_communications(std::istream &input, std::ostream &output) {
  apache::simulation::communications panel;
  std::string command;
  while(input >> command) {
    if(command == "add") {
      int32_t priority;
      int32_t lifetime;
      std::string text;
      input >> priority >> lifetime >> std::quoted(text);
      panel.add(text, priority, lifetime);
    } else if(command == "changing") {
      uint32_t id;
      int32_t priority;
      std::string text;
      input >> id >> priority >> std::quoted(text);
      panel.add_changing(id, text, priority);
    } else if(command == "update") {
      uint32_t id;
      std::string text;
      input >> id >> std::quoted(text);
      panel.update_changing(id, text);
    } else if(command == "remove") {
      uint32_t id;
      input >> id;
      panel.remove(id);
    } else if(command == "step") {
      double seconds;
      input >> seconds;
      panel.advance(seconds);
    } else if(command == "compose") {
      apache::simulation::communication_status status;
      std::string weapon;
      input >> status.fuel >> status.endurance >> status.flow >> status.score >> status.infinite_fuel >> status.faults >> std::quoted(weapon);
      status.weapon = weapon;
      panel.compose(status);
    } else if(command != "snapshot") throw std::runtime_error{"Invalid communications command"};
    if(!input) throw std::runtime_error{"Incomplete communications command"};
    if(command != "snapshot") continue;
    output << panel.damaged_seed;
    for(auto const &line : panel.lines) output << ' ' << std::quoted(line);
    for(auto const &m : panel.messages) output << ' ' << m.lifetime << ' ' << m.priority << ' ' << m.displayed << ' ' << m.changing << ' ' << m.identity << ' ' << std::quoted(m.lifetime > 0 ? m.text : std::string{});
    output << '\n';
  }
}
