#include <charconv>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <utility>
#include <thread>
#include <bit>
#include "frontend/mission_frontend.h"
#include "frontend/mission_link.h"
#include "audio/engine.h"
#include "platform/audio_output.h"
#include "frontend/mission_session.h"
#include "platform/key_map.h"
#include "platform/window.h"
#include "resources/data_store.h"
#include "resources/installation.h"
#include "resources/animation.h"
#include "frontend/presentation.h"

namespace {
unsigned int number(std::string_view const text) {
  unsigned int result{0};
  auto const [end, error]{std::from_chars(text.data(), text.data() + text.size(), result)};
  if(error != std::errc{} || end != text.data() + text.size()) throw std::runtime_error{"Expected a non-negative integer"};
  return result;
}
struct events {
  struct key { int value, modifiers; bool down; bool character{false}, repeat{false}; };
  std::vector<key> keys;
  struct click { unsigned int button; bool down; };
  std::vector<click> clicks;
};
// Playback uses the original 16-tick frame interval (200 Hz clock). Slow
// presentation holds the current frame instead of dropping authored deltas.
bool play_clip(apache::resources::data_store const &data, apache::frontend::presentation_clip const &clip, apache::platform::window &window, events &input) {
  auto const interrupted{[&](){
    bool const skip{std::ranges::any_of(input.keys, [](auto const &k){ return k.down && !k.character; }) || std::ranges::any_of(input.clicks, [](auto const &c){ return c.down; })};
    input.keys.clear(); input.clicks.clear(); return skip;
  }};
  input.keys.clear(); input.clicks.clear();
  if(clip.movie) {
    apache::resources::animation movie{data.read(clip.path)};
    apache::render::framebuffer frame{movie.image.width, std::max(200u, movie.image.height)};
    auto previous{std::chrono::steady_clock::now()};
    while(window.poll()) {
      if(interrupted()) return true;
      auto const now{std::chrono::steady_clock::now()};
      if(std::chrono::duration<double>(now - previous).count() >= .08) {
        previous = now;
        if(!movie.advance()) return false;
        frame.blit(movie.image, 0, 0); frame.image.colours = movie.image.colours;
      }
      window.present(frame);
    }
    return true;
  }
  auto const picture{apache::resources::read_iff(data.read(clip.path))};
  apache::render::framebuffer frame{640, 480}; frame.blit(picture, 0, 0);
  auto const start{std::chrono::steady_clock::now()};
  while(window.poll()) {
    if(interrupted()) return true;
    double const seconds{std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count()};
    int const retrace{static_cast<int>(seconds * 60)};
    if((clip.title && retrace >= 50) || (!clip.title && seconds >= 4 + 50.0 / 60)) return false;
    int const fade{retrace < 50 ? 50 - retrace : seconds >= 4 ? static_cast<int>((seconds - 4) * 60) : 0};
    apache::render::palette_rgb colours;
    for(size_t i{0}; i != 256; ++i) { colours[i * 3] = picture.colours[i].r; colours[i * 3 + 1] = picture.colours[i].g; colours[i * 3 + 2] = picture.colours[i].b; }
    apache::render::palette_fader const fader{false, 0, 0, 256, static_cast<int32_t>(int64_t{fade} * INT32_MAX / 50), {0, 0, 0}};
    colours = apache::render::apply_faders(colours, std::span{&fader, 1});
    for(size_t i{0}; i != 256; ++i) frame.image.colours[i] = {colours[i * 3], colours[i * 3 + 1], colours[i * 3 + 2]};
    window.present(frame);
  }
  return true;
}
// goodbye_sequence_: three 50-retrace fades around an 800-tick hold.
// Ordinary keys and the confirming mouse click do not skip this sequence.
void play_goodbye(apache::resources::data_store const &data, apache::platform::window &window,
                  apache::render::framebuffer &menu, events &input) {
  auto const picture{apache::resources::read_iff(data.read("MENUSCRN/GOODBYE.LBM"))};
  // load_graphics_ clips the 640x512 artwork to the 640x480 menu screen.
  apache::render::framebuffer goodbye{640, 480}; goodbye.blit(picture, 0, 0);
  auto const menu_colours{menu.image.colours};
  glfwSetInputMode(window.get(), GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
  auto const start{std::chrono::steady_clock::now()};
  while(window.poll()) {
    input.keys.clear(); input.clicks.clear();
    double const elapsed{std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count()};
    int const retrace{static_cast<int>(std::min(elapsed * 60, 390.0))};
    auto &frame{retrace < 50 ? menu : goodbye};
    auto const &base{retrace < 50 ? menu_colours : picture.colours};
    int const fade{retrace < 50 ? retrace : retrace < 100 ? 99 - retrace : retrace < 340 ? 0 : retrace - 340};
    apache::render::palette_rgb colours;
    for(size_t i{0}; i != 256; ++i) { colours[i * 3] = base[i].r; colours[i * 3 + 1] = base[i].g; colours[i * 3 + 2] = base[i].b; }
    apache::render::palette_fader const fader{false, 0, 0, 256, static_cast<int32_t>(int64_t{fade} * INT32_MAX / 50), {0, 0, 0}};
    colours = apache::render::apply_faders(colours, std::span{&fader, 1});
    for(size_t i{0}; i != 256; ++i) frame.image.colours[i] = {colours[i * 3], colours[i * 3 + 1], colours[i * 3 + 2]};
    window.present(frame);
    if(retrace == 390) return;
    auto const next{start + std::chrono::duration<double>{(retrace + 1) / 60.0}};
    double const remaining{std::chrono::duration<double>(next - std::chrono::steady_clock::now()).count()};
    if(remaining > 0) glfwWaitEventsTimeout(remaining);
  }
}
} // namespace

int main(int const argc, char const *const argv[]) try {
  std::optional<std::filesystem::path> directory;
  std::filesystem::path cd_image;
  bool check_assets{false}, asset_directory{false};
  auto save_directory{apache::configuration::default_save_directory()};
  std::string selected_menu{"_main_screen"};
  std::string capture;
  std::string pressed;
  unsigned int scale{0}, frame_limit{0};
  bool no_audio{false}, skip_intro{false};
  bool quick{false}, override_model{false}, override_detail{false}, override_video{false};
  int mission_id{0};
  apache::frontend::mission_options mission_options;
  for(int i{1}; i < argc; ++i) {
    std::string_view const key{argv[i]};
    auto const value{[&]()->std::string_view{
      if(++i == argc) throw std::runtime_error{"Missing value for " + std::string{key}};
      return argv[i];
    }};
    if(key == "--help") {
      std::cout << "Apache Redux reconstruction\n"
                   "Usage: apache [--data-dir DIRECTORY] [--scale N]\n"
                   "  --data-dir DIRECTORY  Original installation or extracted CD contents\n"
                   "  --save-dir DIRECTORY  Pilot and preference storage\n"
                   "  --install-cd FILE     Extract the reference CD into --data-dir or the default location\n"
                   "  --check-assets        Verify all reference game files without starting the game\n"
                   "  --asset-directory     Print the default game-data directory\n"
                   "  --resolution WxH      Fixed 3D render size, or auto to follow window pixels (default)\n"
                   "  --fov DEGREES         Horizontal field of view, 30..150 (default: 90)\n"
                   "  --scale N             Initial window scale, or 0 to fit the desktop\n"
                   "  --skip-intro          Skip the opening movies and publisher logos\n"
                   "  --no-audio            Disable the playback device\n"
                   "Development checks:\n"
                   "  --menu NAME           Select an original frontend screen\n"
                   "  --capture FILE.ppm    Render one menu frame without opening a window\n"
                   "  --press NAME          Show a pressed menu button\n"
                   "  --quick               Show the Quickstart choices\n"
                   "  --mission ID          Start a mission directly\n"
                   "  --realistic           Use the realistic flight model\n"
                   "  --video-mode N        Original 320 (0) or 640 (1) mode\n"
                   "  --detail N            Original detail level, 0 to 9\n"
                   "  --frames N            Close after N window frames\n";
      return EXIT_SUCCESS;
    }
    if(key == "--data-dir") directory = value();
    else if(key == "--install-cd") cd_image = value();
    else if(key == "--check-assets") check_assets = true;
    else if(key == "--asset-directory") asset_directory = true;
    else if(key == "--save-dir") save_directory = value();
    else if(key == "--resolution") mission_options.graphics.fixed_resolution = apache::configuration::parse_resolution(value());
    else if(key == "--fov") mission_options.graphics.field_of_view = apache::configuration::parse_field_of_view(value());
    else if(key == "--scale") {
      scale = number(value());
      if(scale > 16) throw std::runtime_error{"Window scale must be between 0 and 16"};
    } else if(key == "--frames") frame_limit = number(value());
    else if(key == "--menu") selected_menu = "_" + std::string{value()} + "_screen";
    else if(key == "--capture") capture = value();
    else if(key == "--press") pressed = value();
    else if(key == "--skip-intro") skip_intro = true;
    else if(key == "--no-audio") no_audio = true;
    else if(key == "--quick") quick = true;
    else if(key == "--mission") mission_id = static_cast<int>(number(value()));
    else if(key == "--realistic") { mission_options.arcade = false; override_model = true; }
    else if(key == "--detail") { mission_options.detail = number(value()); override_detail = true; }
    else if(key == "--video-mode") { mission_options.video_mode = number(value()); override_video = true; }
    else throw std::runtime_error{"Unknown option: " + std::string{key}};
  }
  if(asset_directory) { std::cout << apache::resources::default_game_directory().string() << '\n'; return EXIT_SUCCESS; }
  if(!cd_image.empty()) {
    auto const destination{directory.value_or(apache::resources::default_game_directory())};
    apache::resources::install_reference_cd(cd_image, destination);
    std::cout << "Game resources installed in " << destination.string() << '\n'; return EXIT_SUCCESS;
  }
  if(check_assets) {
    apache::resources::check_reference_installation(directory.value_or(apache::resources::default_game_directory()));
    std::cout << "All game resources match the reference CD.\n"; return EXIT_SUCCESS;
  }
  apache::resources::data_store const data{apache::resources::select_game_directory(directory, save_directory)};
  std::unique_ptr<apache::frontend::mission_session> mission;
  if(mission_id && !capture.empty()) mission = std::make_unique<apache::frontend::mission_session>(data, mission_id, mission_options);
  apache::frontend::mission_frontend frontend{data, mission_options};
  auto &menu{frontend.interface};
  bool launched_from_menu{false};
  menu.show(selected_menu);
  menu.toggle_quick(quick);
  if(!pressed.empty()) menu.press_named(pressed);
  apache::render::framebuffer frame{640, 480};
  menu.render(frame);
  if(!capture.empty()) {
    if(mission) { for(unsigned int i{0}; i != std::max(frame_limit, 1u); ++i) { mission->step(10); mission->draw(); } }
    apache::resources::write_ppm(capture, mission ? mission->frame.image : frame.image);
    return EXIT_SUCCESS;
  }
  auto const data_root{std::filesystem::weakly_canonical(data.directory())};
  auto const save_root{std::filesystem::weakly_canonical(save_directory)};
  auto const relative{save_root.lexically_relative(data_root)};
  if(relative.empty() || (!relative.empty() && *relative.begin() != "..")) throw std::runtime_error{"Save directory must be outside the original game data"};
  frontend.load_profiles(save_directory);
  if(directory) apache::resources::remember_game_directory(data.directory(), save_directory);
  if(override_model) frontend.options.arcade = mission_options.arcade;
  if(override_detail) frontend.options.detail = mission_options.detail;
  if(override_video) frontend.options.video_mode = mission_options.video_mode;
  if(mission_id) mission = std::make_unique<apache::frontend::mission_session>(data, mission_id, frontend.options);
  if(!mission_id && selected_menu == "_main_screen" && !quick && pressed.empty()) frontend.startup();
  apache::platform::window window{scale, mission_options.graphics.fixed_resolution};
  auto const resize_mission{[&](){
    if(!mission) return false;
    auto const size{mission_options.graphics.fixed_resolution.value_or(window.framebuffer_size())};
    return mission->resize(size.width, size.height);
  }};
  std::unique_ptr<apache::audio::engine> sound;
  std::unique_ptr<apache::platform::audio_output> audio_device;
  if(!no_audio) {
    sound = std::make_unique<apache::audio::engine>(data);
    sound->cinematic(true); sound->configure(frontend.settings);
    try {
      audio_device = std::make_unique<apache::platform::audio_output>([](void *context, std::span<float> buffer) noexcept {
        try { static_cast<apache::audio::engine*>(context)->render(buffer); }
        catch(...) { std::ranges::fill(buffer, 0); }
      }, sound.get());
    } catch(std::exception const &error) { std::cerr << "Audio unavailable: " << error.what() << '\n'; sound.reset(); }
  }
  std::string pending_movie;
  std::optional<std::array<int, 5>> pending_speech;
  frontend.movie = [&](int zone, int level){ pending_movie = apache::frontend::campaign_movie(data, zone, level); };
  if(sound) {
    frontend.speak = [&](int kind, int flow, int zone, int level, int id){
      if(!pending_movie.empty()) pending_speech = {kind, flow, zone, level, id};
      else sound->speech(kind, flow, zone, level, id);
    };
    frontend.stop_speech = [&](){ sound->stop_speech(); };
    frontend.speech_playing = [&](){ return sound->speaking(); };
  }
  auto const connect_audio{[&](){
    if(mission && mission->chat) {
      mission->chat->language = static_cast<unsigned int>(std::clamp(frontend.settings.values.integer("Product Language").value_or(1), 0, 8));
      mission->chat->change_language = [&](unsigned int language){ frontend.settings.set("Product Language", static_cast<int>(language)); frontend.settings.save(); };
    }
    if(!sound || !mission) return;
    sound->mission(true);
    mission->play_sound = [&](auto id, auto volume, auto pan){ sound->play(id, volume, pan); };
    mission->flight_sound = [&](auto event){ sound->flight(event); };
    mission->runtime.events.sound = [&](auto id, bool cockpit){ sound->voice(id, cockpit); };
    mission->runtime.events.launch_sound = [&](auto type){ sound->play(type + 8, 30); };
    mission->runtime.events.incoming_sound = [&](){ sound->play(14, 30); };
    mission->runtime.events.gun_sound = [&](bool active){ sound->gun(active); };
  }};
  connect_audio();
  events input;
  glfwSetWindowUserPointer(window.get(), &input);
  glfwSetKeyCallback(window.get(), [](GLFWwindow *const handle, int const key, int, int const action, int const modifiers){
    static_cast<events*>(glfwGetWindowUserPointer(handle))->keys.push_back({key, modifiers, action != GLFW_RELEASE, false, action == GLFW_REPEAT});
  });
  glfwSetCharCallback(window.get(), [](GLFWwindow *handle, unsigned int value){ static_cast<events*>(glfwGetWindowUserPointer(handle))->keys.push_back({static_cast<int>(value), 0, true, true}); });
  glfwSetMouseButtonCallback(window.get(), [](GLFWwindow *const handle, int const button, int const action, int){
    if(button == GLFW_MOUSE_BUTTON_LEFT || button == GLFW_MOUSE_BUTTON_RIGHT) static_cast<events*>(glfwGetWindowUserPointer(handle))->clicks.push_back({button == GLFW_MOUSE_BUTTON_LEFT ? 0u : 1u, action == GLFW_PRESS});
  });
  if(!mission_id && !frame_limit && selected_menu == "_main_screen" && !quick && pressed.empty()) {
    bool skipped{false};
    for(auto const &clip : apache::frontend::intro_clips(data, frontend.settings.animations, skip_intro)) {
      if(skipped && !clip.title) continue;
      skipped = play_clip(data, clip, window, input) || skipped;
      if(glfwWindowShouldClose(window.get())) break;
    }
  }
  if(sound) sound->cinematic(false);
  std::unique_ptr<apache::frontend::mission_link> network_link;
  auto const network_epoch{std::chrono::steady_clock::now()};
  auto const network_clock{[&](){ return std::bit_cast<int32_t>(static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - network_epoch).count() / 5)); }};
  bool in_flight_help{false};
  unsigned int frames{0};
  auto previous_time{std::chrono::steady_clock::now()};
  double tick_fraction{0};
  if(mission) { resize_mission(); mission->step(10); mission->draw(); window.title("Apache Redux | " + mission->briefing.title); }
  while(window.poll() && !menu.quit_requested()) {
    if(network_link && !network_link->ready()) {
      if(std::ranges::any_of(input.keys, [](auto const &k){ return k.down && !k.character && k.value == GLFW_KEY_ESCAPE; })) mission->finished = true;
      input.keys.clear(); input.clicks.clear(); network_link->poll();
      if(mission->finished) {
        network_link.reset(); if(sound) sound->mission(false); frontend.complete(std::move(mission)); window.title("Apache Redux");
      } else if(network_link->ready()) {
        resize_mission(); mission->step(10); mission->draw(); window.title("Apache Redux | " + mission->briefing.title);
      } else { window.present(frame); glfwWaitEventsTimeout(0.005); }
      previous_time = std::chrono::steady_clock::now(); tick_fraction = 0;
      if(frame_limit && ++frames == frame_limit) break;
      continue;
    }
    if(mission && !in_flight_help) {
      bool const resized{resize_mission()};
      for(auto const &key : input.keys) if(!key.character && (!key.repeat || (mission->chat && mission->chat->writing))) if(auto scan{apache::platform::scan_code(key.value, key.modifiers)}) mission->key(*scan, key.down);
      input.keys.clear(); input.clicks.clear();
      if(!window.focused()) { mission->controls.held.fill(false); mission->controls.counts.fill(0); }
      mission->controls.joystick_present = window.focused() && glfwJoystickPresent(GLFW_JOYSTICK_1);
      mission->controls.buttons = mission->controls.hat = 0;
      mission->controls.hat_present = false; mission->controls.axis_count = 0;
      if(mission->controls.configuration.input_type != 1) {
        int button_count{0}, hat_count{0};
        auto const *buttons{glfwGetJoystickButtons(GLFW_JOYSTICK_1, &button_count)};
        for(int i{0}; buttons && i < std::min(button_count, 4); ++i) if(buttons[i] == GLFW_PRESS) mission->controls.buttons |= 1u << i;
        auto const *hats{glfwGetJoystickHats(GLFW_JOYSTICK_1, &hat_count)};
        mission->controls.hat_present = hats && hat_count;
        if(mission->controls.hat_present) switch(hats[0]) {
          case GLFW_HAT_UP: mission->controls.hat = 1; break;
          case GLFW_HAT_RIGHT: mission->controls.hat = 2; break;
          case GLFW_HAT_DOWN: mission->controls.hat = 3; break;
          case GLFW_HAT_LEFT: mission->controls.hat = 4; break;
          default: break;
        }
        int count{0}; auto const *axes{glfwGetJoystickAxes(GLFW_JOYSTICK_1, &count)};
        mission->controls.axis_count = static_cast<unsigned int>(std::max(count, 0));
        for(size_t i{0}; i != mission->controls.joystick.size(); ++i) mission->controls.joystick[i].value = axes && i < static_cast<size_t>(count) ? static_cast<int32_t>(std::lround((std::clamp(axes[i], -1.0f, 1.0f) + 1.0f) * 127.5f)) : 128;
      }
      auto const now{std::chrono::steady_clock::now()};
      tick_fraction += std::chrono::duration<double>(now - previous_time).count() * 200; previous_time = now;
      auto const ticks{static_cast<unsigned int>(std::min(tick_fraction, 200.0))};
      if(ticks) { tick_fraction -= ticks; mission->step(ticks); }
      if(ticks || resized) mission->draw();
      if(sound) sound->pause(mission->paused || mission->quit_prompt || mission->help_requested);
      window.present(mission->frame);
      if(mission->help_requested) {
        mission->help_requested = false; in_flight_help = true;
        mission->controls.held.fill(false); mission->controls.counts.fill(0);
        menu.show("_help_screen");
      }
      if(mission->finished) {
        network_link.reset();
        if(sound) sound->mission(false);
        if(launched_from_menu) frontend.complete(std::move(mission));
        else { mission.reset(); menu.show("_main_screen"); }
        window.title("Apache Redux");
      }
      if(frame_limit && ++frames == frame_limit) break;
      continue;
    }
    auto const now{std::chrono::steady_clock::now()};
    if(!in_flight_help) frontend.poll_network(static_cast<uint32_t>(network_clock()));
    else if(network_link) network_link->poll();
    frontend.interface.tick(std::chrono::duration<double>(now - previous_time).count());
    if(!in_flight_help) frontend.planner.tick(std::chrono::duration<double>(now - previous_time).count());
    previous_time = now; tick_fraction = 0;
    auto const cursor{window.cursor_position()};
    menu.move(cursor ? static_cast<int>(cursor->x) : -1, cursor ? static_cast<int>(cursor->y) : -1);
    for(auto const &click : input.clicks) menu.button(click.down, click.button);
    if(menu.quit_requested()) break;
    input.clicks.clear();
    for(auto const &key : input.keys) {
      if(!key.down) continue;
      if(key.character) { frontend.text_input(static_cast<unsigned int>(key.value)); continue; }
      if((key.value == GLFW_KEY_ESCAPE || key.value == GLFW_KEY_ENTER || key.value == GLFW_KEY_BACKSPACE) && frontend.text_input(key.value == GLFW_KEY_ESCAPE ? 27 : key.value == GLFW_KEY_ENTER ? 10 : 8)) continue;
      if(key.value == GLFW_KEY_ESCAPE) frontend.back();
      else if(key.value == GLFW_KEY_F1) menu.show("_help_screen");
    }
    input.keys.clear();
    if(menu.current() == "_preferences_screen") {
      int count{0}, hats{0}; auto const *axes{glfwGetJoystickAxes(GLFW_JOYSTICK_1, &count)}; auto const *hat{glfwGetJoystickHats(GLFW_JOYSTICK_1, &hats)};
      std::array<int32_t, 4> values{128, 128, 128, 128};
      for(size_t i{0}; i != values.size(); ++i) if(axes && i < static_cast<size_t>(count)) values[i] = static_cast<int32_t>(std::lround((std::clamp(axes[i], -1.0f, 1.0f) + 1.0f) * 127.5f));
      frontend.preferences.calibration(values, hat && hats ? *hat : 0u);
    }
    if(!pending_movie.empty()) {
      if(sound) sound->cinematic(true);
      play_clip(data, {std::exchange(pending_movie, {}), true, false}, window, input);
      if(sound) {
        sound->cinematic(false);
        if(pending_speech) { auto const s{*pending_speech}; pending_speech.reset(); sound->speech(s[0], s[1], s[2], s[3], s[4]); }
      }
      previous_time = std::chrono::steady_clock::now(); tick_fraction = 0;
    }
    if(in_flight_help && menu.current() != "_help_screen") {
      in_flight_help = false; mission->views->modes.changed = true;
      if(sound) sound->pause(mission->paused);
      previous_time = std::chrono::steady_clock::now(); tick_fraction = 0;
      continue;
    }
    if(sound && !in_flight_help) sound->configure(frontend.settings);
    menu.render(frame);
    window.present(frame);
    if(auto launched{frontend.takeoff()}) {
      mission = std::move(launched); launched_from_menu = true; connect_audio();
      if(mission->network && frontend.network.peers) {
        network_link = std::make_unique<apache::frontend::mission_link>(*frontend.network.peers, *mission, network_clock);
        network_link->yield = [&](){ std::this_thread::sleep_for(std::chrono::milliseconds{1}); return window.poll(); };
        window.title("Apache Redux | Waiting for other players");
      } else { resize_mission(); mission->step(10); mission->draw(); window.title("Apache Redux | " + mission->briefing.title); }
      previous_time = std::chrono::steady_clock::now(); tick_fraction = 0;
    }
    if(frame_limit && ++frames == frame_limit) break;
  }
  if(menu.quit_requested() && !glfwWindowShouldClose(window.get())) play_goodbye(data, window, frame, input);
  frontend.logs.save(); frontend.settings.save();
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}
