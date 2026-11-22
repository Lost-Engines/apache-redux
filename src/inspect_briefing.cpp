#include "inspect_briefing.h"
#include <fstream>
#include <istream>
#include <ostream>
#include <stdexcept>
#include "frontend/map_camera.h"
#include "frontend/map_editor.h"
#include "frontend/map_layers.h"
#include "frontend/map_routes.h"
#include "frontend/mission_session.h"

bool inspect_briefing(apache::resources::data_store const &data, std::string_view const command, std::istream &input, std::ostream &output, std::string_view const dump) {
  if(command != "map-view" && command != "map-routes" && command != "map-overlay" && command != "map-camera" && command != "map-editor") return false;
  apache::math::trigonometry trig{data}; apache::frontend::map_view view;
  if(command == "map-editor") {
    apache::resources::mission_task task; apache::frontend::map_editor editor; std::string operation;
    while(input >> operation) {
      if(operation == "init") {
        size_t count; input >> count; if(count > 100) throw std::runtime_error{"Too many test groups"}; task.actors.resize(count);
        for(auto &actor : task.actors) { input >> actor.start_time >> count; if(count > 40) throw std::runtime_error{"Too many test waypoints"}; actor.waypoints.resize(count); for(auto &p : actor.waypoints) { input >> p.type; for(auto &v : p.position) input >> v; input >> p.speed; } }
        editor.attach(task, 0);
      } else if(operation == "view") { input >> view.position.x >> view.position.y >> view.position.zoom >> view.position.rotation; } else if(operation == "click" || operation == "drag") { int32_t x, y; input >> x >> y; if(operation == "click") editor.click(x, y, view, trig); else editor.drag(x, y, view.position.zoom); } else if(operation == "select") { size_t number; input >> number; editor.select(number); } else if(operation == "insert") editor.insert();
      else if(operation == "erase") editor.erase();
      else if(operation == "type") editor.cycle_type();
      else if(operation == "position") { int32_t x, y, z; input >> x >> y >> z; editor.position(x, y, z); } else if(operation == "speed" || operation == "time") { int32_t value; input >> value; if(operation == "speed") editor.speed(value); else editor.time(value); } else throw std::runtime_error{"Unknown map editor operation"};
      if(!input) throw std::runtime_error{"Incomplete map editor operation"};
      output << editor.group << ' ' << editor.selected << ' ' << editor.display_type();
      for(auto const &actor : task.actors) {
        auto const letters{apache::frontend::waypoint_letters(actor.waypoints)}; output << ' ' << actor.start_time << ' ' << actor.waypoints.size();
        for(size_t i{0}; i < actor.waypoints.size(); ++i) { auto const &p{actor.waypoints[i]}; output << ' ' << p.type; for(auto v : p.position) output << ' ' << v; output << ' ' << p.time << ' ' << p.speed << ' ' << p.interval << ' ' << p.distance << ' ' << static_cast<unsigned int>(letters[i]); }
      }
      output << '\n';
    }
    return true;
  }
  if(command == "map-camera") {
    apache::frontend::map_camera camera; std::string operation;
    while(input >> operation) {
      if(operation == "select") { unsigned int mode; input >> mode; camera.select(mode); } else if(operation == "target" || operation == "position") { int32_t x, y; input >> x >> y; if(operation == "target") camera.target(x, y); else camera.position(x, y); } else if(operation == "step") { double seconds; input >> seconds; camera.step(seconds, trig, [](int32_t x, int32_t y){ return ((x >> 10) ^ (y >> 11)) & 0x3ffff; }); } else throw std::runtime_error{"Unknown map camera operation"};
      if(!input) throw std::runtime_error{"Incomplete map camera operation"};
      output << camera.mode << ' ' << camera.heading << ' ' << camera.pitch << ' ' << camera.roll << ' ' << camera.angle << ' ' << static_cast<uint32_t>(camera.speed) << ' ' << static_cast<uint32_t>(camera.target_x) << ' ' << static_cast<uint32_t>(camera.target_y);
      for(auto v : camera.camera.position) output << ' ' << static_cast<uint32_t>(v);
      for(auto v : camera.camera.rotation) output << ' ' << static_cast<uint32_t>(v);
      output << '\n';
    }
    return true;
  }
  if(command == "map-view") {
    std::string operation;
    while(input >> operation) {
      int x, y, z; uint32_t angle;
      if(operation == "set") { input >> x >> y >> z >> angle; view.set(x, y, z, angle); } else if(operation == "centre") { input >> x >> y; view.centre(x, y); } else if(operation == "zoom") { input >> z; view.zoom(z); } else if(operation == "rotate") { input >> angle; view.rotate(angle); } else if(operation == "push") view.push();
      else if(operation == "pop") view.pop();
      else if(operation == "clear") view.clear();
      else if(operation == "fit") { input >> z; if(z < 0 || z > 3072) throw std::runtime_error{"Invalid fit point count"}; std::vector<apache::render::vertex2> points(static_cast<size_t>(z)); for(auto &p : points) input >> p.x >> p.y; view.fit(points); } else if(operation == "unproject") { input >> x >> y; auto const p{view.projection(trig).unproject(x, y)}; output << p.x << ' ' << p.y << '\n'; continue; } else throw std::runtime_error{"Unknown map operation"};
      if(!input) throw std::runtime_error{"Incomplete map operation"};
      auto const &p{view.position}; output << p.x << ' ' << p.y << ' ' << p.zoom << ' ' << p.rotation << ' ' << view.history_size() << '\n';
    }
    return true;
  }
  apache::render::framebuffer frame{640, 480};
  auto const result{[&]{
    output << apache::resources::fingerprint(frame.image.pixels) << '\n';
    if(!dump.empty()) { std::ofstream file{std::string{dump}, std::ios::binary}; file.write(reinterpret_cast<char const*>(frame.image.pixels.data()), static_cast<std::streamsize>(frame.image.pixels.size())); }
  }};
  if(command == "map-routes") {
    apache::resources::executable_data exe{data}; apache::render::instrument_font font{exe, true};
    bool primary, others; size_t count;
    while(input >> view.position.x >> view.position.y >> view.position.zoom >> view.position.rotation >> primary >> others >> count) {
      if(count > 3072) throw std::runtime_error{"Invalid route command count"};
      std::vector<apache::frontend::map_route_command> commands(count); for(auto &c : commands) input >> c.kind >> c.x >> c.y;
      frame.clear(); apache::frontend::draw_map_routes(frame, view, commands, trig, font, primary, others); result();
    }
  } else {
    std::unique_ptr<apache::frontend::mission_session> mission; int id; unsigned int flags;
    while(input >> id >> view.position.x >> view.position.y >> view.position.zoom >> view.position.rotation >> flags) {
      if(!mission || mission->briefing.id != id) mission = std::make_unique<apache::frontend::mission_session>(data, id);
      apache::frontend::map_layers const options{bool(flags & 1), bool(flags & 2), bool(flags & 4), bool(flags & 8), bool(flags & 16), bool(flags & 32), bool(flags & 64), bool(flags & 128), bool(flags & 256), bool(flags & 512)};
      frame.clear(); apache::frontend::draw_map_overlay(frame, view, trig, mission->runtime.scene, mission->runtime.mission.destroy_markers, options); result();
    }
  }
  return true;
}
