#include "inspect_menu.h"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <istream>
#include <ostream>
#include <stdexcept>
#include "frontend/mission_frontend.h"
#include "simulation/quick_start.h"

void inspect_menu(apache::resources::data_store const &data, std::istream &input, std::ostream &output) {
  apache::frontend::menu menu{data};
  apache::render::framebuffer frame{640, 480};
  std::string command;
  while(input >> command) {
    std::string name; input >> std::quoted(name);
    if(command == "screen") menu.show(name);
    else if(command == "draw") {
      menu.render(frame);
      if(name != "-") apache::resources::write_ppm(name, frame.image);
      output << apache::resources::fingerprint(frame.image.pixels) << '\n';
    } else {
      auto &object{menu.named(name)};
      if(command == "text") input >> std::quoted(object.text);
      else if(command == "range") { for(auto &value : object.range) input >> value; } else if(command == "on") input >> object.on;
      else if(command == "picture") input >> std::quoted(object.picture);
      else if(command == "selection") input >> object.selected_line >> object.start_line;
      else if(command == "visible") { bool visible; input >> visible; object.flags = (object.flags & ~1u) | static_cast<unsigned int>(visible); } else throw std::runtime_error{"Unknown menu inspection command"};
    }
    if(!input) throw std::runtime_error{"Incomplete menu inspection command"};
  }
}

void inspect_frontend(apache::resources::data_store const &data, std::istream &input, std::ostream &output) {
  auto owner{std::make_unique<apache::frontend::mission_frontend>(data)};
  auto &app{*owner}; auto &menu{app.interface};
  apache::render::framebuffer frame{640, 480};
  std::unique_ptr<apache::frontend::mission_session> mission;
  menu.render(frame);
  std::string command;
  while(input >> command) {
    if(command == "click") {
      int x, y; input >> x >> y; menu.move(x, y); menu.button(true); menu.button(false); menu.move(-1, -1);
    } else if(command == "rightclick") { int x, y; input >> x >> y; menu.move(x, y); menu.button(true, 1); menu.button(false, 1); menu.move(-1, -1); } else if(command == "menutick") { double seconds; input >> seconds; menu.tick(seconds); app.planner.tick(seconds); } else if(command == "mappose") { auto &c{app.planner.preview_camera}; c.mode = 1; for(auto &v : c.camera.position) input >> v; input >> c.heading >> c.pitch >> c.roll; c.target(c.camera.position[0], c.camera.position[1]); app.planner.tick(0); } else if(command == "widgetbounds") { std::string name; input >> std::quoted(name); auto const &r{menu.named(name).area}; output << r.left << ' ' << r.top << ' ' << r.right << ' ' << r.bottom << '\n'; } else if(command == "mapcamera") { auto const &c{app.planner.preview_camera}; output << c.mode; for(auto v : c.camera.position) output << ' ' << v; output << ' ' << c.heading << ' ' << c.pitch << ' ' << c.roll << ' ' << c.angle << ' ' << c.speed << ' ' << c.target_x << ' ' << c.target_y << '\n'; } else if(command == "mapissues") { auto const [n, t]{app.planner.preview_issues()}; output << n << ' ' << t << '\n'; } else if(command == "plannerstate" || command == "flightroute") {
      apache::resources::mission_actor const *actor{app.planner.editor.actor()};
      if(command == "flightroute") {
        if(!mission) throw std::runtime_error{"No mission running"};
        auto const &r{mission->runtime}; auto const &p{r.actors.units.at(*r.timing.player)};
        auto const g{std::ranges::find_if(r.actors.groups, [&](auto const &g){ return g.task == p.location.task && g.index == p.location.group; })};
        actor = &g->definition;
      } else output << app.planner.editor.group << ' ' << app.planner.editor.selected << ' ';
      if(!actor) throw std::runtime_error{"No route selected"};
      output << actor->start_time << ' ' << actor->waypoints.size();
      for(auto const &p : actor->waypoints) { output << ' ' << p.type; for(auto v : p.position) output << ' ' << v; output << ' ' << p.speed; }
      output << '\n';
    } else if(command == "widgettext") { std::string name; input >> std::quoted(name); output << std::quoted(menu.named(name).text) << '\n'; } else if(command == "press" || command == "release") { unsigned int button; input >> button; menu.button(command == "press", button); } else if(command == "mapstate") { auto const &p{app.planner.view.position}; output << p.x << ' ' << p.y << ' ' << p.zoom << ' ' << p.rotation << ' ' << app.planner.view.history_size() << '\n'; } else if(command == "loadout") { if(!mission) throw std::runtime_error{"No mission running"}; for(auto const &p : mission->runtime.player_armament->inventory.pylons) output << p.weapon << ' ' << p.ammunition << ' '; output << '\n'; } else if(command == "move") { int x, y; input >> x >> y; menu.move(x, y); } else if(command == "drag") { int x, y, xx, yy; input >> x >> y >> xx >> yy; menu.move(x, y); menu.button(true); menu.move(xx, yy); menu.button(false); menu.move(-1, -1); } else if(command == "preference") { std::string name; int value; input >> std::quoted(name) >> value; app.settings.set(name, value); app.preferences.update(); } else if(command == "previewzone") input >> app.preferences.zone;
    else if(command == "previewissues") { auto const [normals, textures]{app.preferences.preview_issues()}; output << normals << ' ' << textures << '\n'; } else if(command == "indicators") {
      output << "indicators";
      for(auto name : {"_preferences_cyclic_sprite", "_preferences_throttle_vt_sprite", "_preferences_throttle_hz_sprite", "_preferences_coolie_c_sprite", "_preferences_coolie_t_sprite", "_preferences_coolie_r_sprite", "_preferences_coolie_b_sprite", "_preferences_coolie_l_sprite"}) { auto const &object{menu.named(name)}; output << ' ' << (object.flags & 1) << ' ' << object.area.left << ' ' << object.area.top << ' ' << object.area.right << ' ' << object.area.bottom; }
      output << '\n';
    } else if(command == "calibration") { std::array<int32_t, 4> axes; unsigned int hat; for(auto &v : axes) input >> v; input >> hat; app.preferences.calibration(axes, hat); } else if(command == "settings") {
      auto const &s{app.settings}; output << "settings " << s.video_mode << ' ' << s.detail << ' ' << s.model << ' ' << s.forces << ' ' << s.volume << ' ' << s.planner << ' ' << s.animations << ' ' << s.speech << ' ' << s.music << ' ' << s.effects << ' ' << s.minimal_effects << ' ' << s.controls.cyclic << ' ' << s.controls.collective << ' ' << s.controls.tail_rotor << '\n';
      if(mission) output << "flight " << mission->frame.image.width << ' ' << mission->frame.image.height << ' ' << app.options.arcade << ' ' << mission->controls.configuration.cyclic << ' ' << mission->controls.configuration.collective << ' ' << mission->controls.configuration.tail_rotor << '\n';
    } else if(command == "profiles") { std::string path; input >> std::quoted(path); app.load_profiles(path); } else if(command == "startup") app.startup();
    else if(command == "topscore") { int32_t score; std::string name; input >> score >> std::quoted(name); app.scores.record(score, name); app.scores.show(); } else if(command == "scoreselection") { input >> app.scores.last; app.scores.show(); } else if(command == "scores") { output << "scores " << app.scores.last; for(auto const &entry : app.scores.entries) output << ' ' << entry.score << ' ' << std::quoted(entry.name); output << '\n'; } else if(command == "scorepoints") { apache::resources::model object; int difficulty; input >> object.metadata[7] >> object.metadata[0] >> difficulty; output << apache::simulation::quick_start_points(object, difficulty) << '\n'; } else if(command == "quickstate") {
      if(!mission) output << "none\n";
      else { auto const &r{mission->runtime}; auto const &w{r.player_armament->inventory}; output << mission->zone << ' ' << mission->briefing.flow << ' ' << w.armed << ' ' << w.infinite << ' ' << r.pilot->arcade.infinite_fuel << ' ' << r.pilot->invulnerable() << ' ' << r.pilot->arcade.no_crashes << ' ' << r.quick_score << ' ' << w.helmet_mode << '\n'; }
    } else if(command == "fixture_score") { if(!mission) throw std::runtime_error{"No mission running"}; input >> mission->runtime.quick_score; } else if(command == "text") { std::string text; input >> std::quoted(text); for(unsigned char ch : text) app.text_input(ch); } else if(command == "editkey") { unsigned int value; input >> value; app.text_input(value); } else if(command == "profile") { output << "profile " << app.logs.user << ' ' << app.logs.users[app.logs.user].selected << ' ' << app.logs.users[app.logs.user].pilots.size() << ' ' << std::quoted(app.logs.users[app.logs.user].name) << ' ' << std::quoted(app.pilot().name) << ' ' << app.pilot().rank << ' ' << app.pilot().status << '\n'; } else if(command == "back") app.back();
    else if(command == "tick") {
      unsigned int ticks; input >> ticks;
      if(!mission) throw std::runtime_error{"No mission running"};
      mission->step(ticks); mission->draw();
    } else if(command == "fixture_death") {
      if(!mission) throw std::runtime_error{"No mission running"};
      unsigned int death; input >> death;
      mission->runtime.pilot->arcade.death = death; mission->runtime.pilot->realistic.death = death;
    } else if(command == "complete") {
      if(!mission) throw std::runtime_error{"No mission running"};
      app.complete(std::move(mission));
    } else if(command == "campaign") {
      auto const &p{app.pilot().campaign}; output << "campaign " << p.warzone << ' ' << p.level << ' ' << p.lives << ' ' << p.seed << ' ' << p.missions.size();
      for(bool won : app.pilot().victories) output << ' ' << won;
      output << '\n';
    } else if(command == "help") { menu.show("_help_screen");
    } else if(command == "helppage") { int page; input >> page; app.help.select(page);
    } else if(command == "helpgeneric") { int page; input >> page; app.help.generic(page);
    } else if(command == "helpstate") { output << app.help.page() << '\n';
    } else if(command == "end") {
      if(!mission) throw std::runtime_error{"No mission running"};
      mission->post(0x2001); mission->step(1); mission->draw(); mission->post(0x15); mission->step(1); app.complete(std::move(mission));
    } else if(command == "draw") {
      std::string file; input >> std::quoted(file);
      auto const &image{mission ? mission->frame.image : frame.image};
      if(file != "-") {
        apache::resources::write_ppm(file, image);
        std::ofstream raw{file + ".pixels", std::ios::binary}; raw.write(reinterpret_cast<char const*>(image.pixels.data()), static_cast<std::streamsize>(image.pixels.size()));
      }
      output << "frame " << apache::resources::fingerprint(image.pixels) << '\n';
    } else if(command == "state") {
      output << std::quoted(menu.current()) << ' ' << (mission ? mission->briefing.id : 0) << ' ' << app.pilot().total_time << ' ' << app.pilot().missions.size();
      for(auto const &entry : app.pilot().missions) output << ' ' << entry.mission << ' ' << static_cast<unsigned int>(entry.result);
      output << '\n';
    } else throw std::runtime_error{"Unknown frontend inspection command"};
    if(!input) throw std::runtime_error{"Incomplete frontend inspection command"};
    menu.render(frame);
    if(auto launched{app.takeoff()}) { mission = std::move(launched); mission->step(10); mission->draw(); }
  }
}
