#include "capture_options.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include "frontend/mission_frontend.h"

namespace {
void require(bool value, char const *message) { if(!value) throw std::runtime_error{message}; }
struct temporary_directory {
  std::filesystem::path path{std::filesystem::temp_directory_path() / ("apache-controls-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))};
  temporary_directory() { std::filesystem::create_directory(path); }
  ~temporary_directory() { std::error_code error; std::filesystem::remove_all(path, error); }
};
}
int main(int argc, char **argv) try {
  auto const options{apache::testing::read_capture_options(argc, argv, "Controller menu checks")};
  if(options.help) return 0;
  apache::resources::data_store data{options.directory};
  temporary_directory temporary;
  apache::frontend::mission_frontend frontend{data};
  frontend.load_profiles(temporary.path);
  auto &menu{frontend.interface};
  auto &controls{frontend.controls};
  auto &settings{frontend.settings};
  require(!settings.controllers.current().enabled, "fresh install must default to keyboard flight");
  apache::render::framebuffer frame{640, 480};
  std::vector<apache::input::device_state> devices{{{"hotas", "Test HOTAS", 0}, std::vector<float>(12), std::vector<unsigned char>(40), {0, 0}, {}, {}}};
  auto const draw{[&]{menu.render(frame);}};
  auto const click_at{[&](int x, int y){draw(); menu.move(x, y); menu.button(true); menu.button(false);}};
  auto const click{[&](std::string_view name){auto const r{menu.named(name).area}; click_at((r.left + r.right) / 2, (r.top + r.bottom) / 2);}};
  auto const save{[&](char const *name){
    if(options.capture) { std::filesystem::create_directories(*options.capture); draw(); frame.write_ppm((*options.capture / name).string()); }
  }};
  frontend.preferences.show();
  controls.poll(devices, true);
  save("preferences.ppm");
  click("_redux_controls_open");
  require(menu.current() == "_controls_screen", "Preferences must open Controls through the real click handler");
  save("keyboard.ppm");
  auto const select{[&](size_t row){
    auto &list{menu.named("_controls_list")};
    list.start_line = row > 12 ? static_cast<unsigned int>(row - 8) : 0;
    click_at(list.area.left + 25, list.area.top + static_cast<int>(row - list.start_line) * 16 + 7);
  }};
  select(2);
  click("_controls_bind");
  controls.poll(devices, true);
  require(!settings.controllers.current().axes[2].positive.assigned(), "starting capture must not immediately bind an unmoved axis");
  devices[0].axes[7] = .8f;
  controls.poll(devices, true);
  require(settings.controllers.current().axes[2].positive.index == 7 && settings.controllers.current().enabled, "learned axis activates controller profile");
  click("_controls_calibrate");
  devices[0].axes[7] = -.7f; controls.poll(devices, true);
  devices[0].axes[7] = .8f; controls.poll(devices, true);
  devices[0].axes[7] = .1f; controls.poll(devices, true);
  click("_controls_calibrate");
  require(std::abs(settings.controllers.current().axes[2].centre - .1) < .001 && settings.controllers.current().axes[2].minimum < -.69, "calibration saves observed endpoints and released centre");
  click("_controls_bind");
  require(frontend.text_input(27) && menu.current() == "_controls_screen", "Escape must cancel capture before leaving Controls");
  click("_controls_invert");
  require(settings.controllers.current().axes[2].inverted, "invert button changes selected axis");
  devices[0].axes[7] = .65f; controls.poll(devices, true);
  save("flight-axes.ppm");
  select(1);
  click("_controls_negative");
  devices[0].buttons[25] = 1; controls.poll(devices, true);
  devices[0].buttons[25] = 0; controls.poll(devices, true);
  click("_controls_positive");
  devices[0].buttons[26] = 1; controls.poll(devices, true);
  require(settings.controllers.current().axes[1].paired && settings.controllers.current().axes[1].negative.index == 25 && settings.controllers.current().axes[1].positive.index == 26, "separate button inputs can operate either pedal");
  click("_controls_new");
  require(settings.controllers.profiles.size() == 2 && settings.controllers.selected == 1, "Copy creates a selectable independent profile");
  click("_controls_rename");
  for(int i{0}; i < 20; ++i) frontend.text_input(8);
  for(char c : std::string{"My HOTAS"}) frontend.text_input(static_cast<unsigned int>(c));
  frontend.text_input(10);
  require(settings.controllers.current().name == "My HOTAS", "profile name can be edited in the menu");
  click("_controls_buttons");
  select(0);
  click("_controls_bind");
  devices[0].buttons[31] = 1; controls.poll(devices, true);
  require(settings.controllers.current().buttons[0].index == 31, "button binding uses full button range");
  save("buttons.ppm");
  select(23); // Look right, below the first page.
  click("_controls_bind");
  devices[0].hats[1] = 2; controls.poll(devices, true);
  require(settings.controllers.current().buttons[23].kind == apache::input::source_kind::hat && settings.controllers.current().buttons[23].index == 1, "scrolling selects actions below the first page and binds another hat");
  save("hat.ppm");
  click("_controls_mode");
  require(!settings.controllers.current().enabled, "input mode switches profile to keyboard only");
  apache::configuration::settings loaded{data};
  loaded.load(temporary.path);
  require(loaded.controllers == settings.controllers, "all UI changes survive application restart");
  click("_controls_back");
  require(menu.current() == "_preferences_screen", "Done returns to Preferences");
  // Saved legacy selection seeds the new profile once, with its calibration.
  temporary_directory imported;
  {
    std::ofstream prefs{imported.path / "prefs.cfg"};
    prefs << "Cyclic control=1\nCollective control=6\nTailrotor control=5\nControl Deadband Size=10\n";
  }
  apache::configuration::settings legacy{data};
  legacy.load(imported.path);
  auto const &old{legacy.controllers.current()};
  require(old.enabled && old.axes[0].positive.index == 3 && old.axes[1].positive.index == 2 && old.axes[2].positive.index == 1 && old.axes[3].positive.index == 0, "legacy joystick preferences seed all four flight axes");
  require(old.axes[0].inverted && old.axes[1].inverted, "legacy throttle and rudder retain their directions");
  legacy.save();
  apache::configuration::settings again{data}; again.load(imported.path);
  require(again.controllers == legacy.controllers, "migrated controller profile survives restart");
  {
    std::ofstream corrupt{imported.path / "controls.cfg"}; corrupt << "bad controls";
  }
  again.load(imported.path);
  require(!again.controller_error.empty(), "damaged controller settings have a visible error");
  again.save();
  std::ifstream preserved{imported.path / "controls.cfg"}; std::string text; std::getline(preserved, text);
  require(text == "bad controls", "unrelated preference saves must preserve damaged controller profiles");
  // Run mapped inputs through the actual mission command queue, not just the
  // mapper: toggles must not repeat and camera routing must work while paused.
  apache::frontend::mission_options flight_options;
  flight_options.detail = 0;
  flight_options.invulnerable = true;
  apache::frontend::mission_session mission{data, apache::frontend::mission_session::quick_start(data, 3), flight_options};
  apache::input::controller_profile flight_profile;
  flight_profile.enabled = true;
  auto const action_index{[](std::string_view id){return static_cast<size_t>(std::ranges::find(apache::input::actions, id, &apache::input::action::id) - apache::input::actions.begin());}};
  auto const bind_button{[&](std::string_view action, unsigned int index){flight_profile.buttons[action_index(action)] = {devices[0].id, apache::input::source_kind::button, index};}};
  bind_button("pause", 30);
  bind_button("auto_countermeasures", 27);
  bind_button("external", 28);
  bind_button("orbit", 29);
  flight_profile.axes[3].positive = {devices[0].id, apache::input::source_kind::axis, 9};
  apache::input::controller_mapper mapper;
  devices[0].buttons.assign(40, 0);
  auto const step{[&]{
    auto const sample{mapper.sample(flight_profile, devices, true)};
    mission.controls.controller(sample);
    for(size_t i{0}; i < apache::input::actions.size(); ++i) if(sample.pressed[i]) mission.post(apache::input::actions[i].scan);
    mission.step(10);
  }};
  step();
  bool const automatic{mission.runtime.player_armament->inventory.automatic_chaff};
  devices[0].buttons[27] = 1;
  step();
  require(mission.runtime.player_armament->inventory.automatic_chaff != automatic, "mapped action reaches the real armament command handler");
  for(int i{0}; i < 5; ++i) step();
  require(mission.runtime.player_armament->inventory.automatic_chaff != automatic, "holding a toggle button must not repeat its command in flight");
  devices[0].buttons[30] = 1;
  step();
  require(mission.paused, "controller Pause action reaches the mission");
  auto const position{mission.runtime.pilot->readings().position.position};
  devices[0].buttons[28] = 1;
  devices[0].buttons[29] = 1;
  devices[0].axes[9] = .6f;
  step();
  require(mission.views->modes.mode == 10 && mission.controls.external[3] != 0, "view switching and joystick camera orbit work while paused");
  require(mission.runtime.pilot->readings().position.position == position && mission.paused, "paused camera control must not advance flight or toggle pause repeatedly");
  devices[0].buttons[30] = 0; step();
  devices[0].buttons[30] = 1; step();
  require(!mission.paused, "Pause toggles again only after release and a new press");
  std::cout << "Controls menu capture, calibration, navigation, migration, restart and live mission checks passed\n";
  return 0;
} catch(std::exception const &error) { std::cerr << error.what() << '\n'; return 1; }
