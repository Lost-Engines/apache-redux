#include "preferences_menu.h"
#include <algorithm>
#include <format>
#include "resources/executable.h"

namespace apache::frontend {
void preferences_menu::update() {
  interface.bounds("_preferences_visual_button", {19, 83, 310, 240});
  interface.bounds("_preferences_visual", {20, 84, 309, 239});
  interface.named("_preferences_fov_button").text = std::format("Field of view: {} deg", settings.graphics.field_of_view);
  interface.named("_preferences_lod_button").text = std::format("LOD distance: {}x", settings.graphics.lod_distance_multiplier);
  auto const on{[this](std::string_view name, bool value){ interface.named(name).on = value; }};
  interface.named("_preferences_visual_detail_bar").range = {0, 99, static_cast<int>(settings.detail * 10), 10};
  interface.named("_preferences_forces_slider_bar").range = {0, 99, static_cast<int>(settings.forces * 10), 5};
  interface.named("_preferences_volume_bar").range = {0, 63, static_cast<int>(settings.volume), 5};
  on("_preferences_visual_320_button", !settings.video_mode); on("_preferences_visual_640_button", settings.video_mode);
  on("_preferences_arcade_button", settings.model == 2); on("_preferences_realistic_button", settings.model == 0);
  on("_preferences_mission_planner_enabled_button", settings.planner); on("_preferences_mission_planner_disabled_button", !settings.planner);
  on("_preferences_animations_enabled_button", settings.animations); on("_preferences_animations_disabled_button", !settings.animations);
  on("_preferences_sound_speech_button", settings.speech); on("_preferences_sound_music_button", settings.music);
  on("_preferences_sound_effects_off_button", !settings.effects); on("_preferences_sound_effects_min_button", settings.effects && settings.minimal_effects); on("_preferences_sound_effects_full_button", settings.effects && !settings.minimal_effects);
  interface.named("_preferences_collective_button").text = settings.controls.collective == 6 ? "Throttle Stick" : "Keyboard";
  interface.named("_preferences_tailrotor_button").text = settings.controls.tail_rotor == 5 ? "Pedals" : "Keyboard";
  interface.named("_preferences_cyclic_button").text = std::array<std::string_view, 5>{"Keyboard", "Joystick", "Thrustmaster FCS", "Flightstick Pro", "Virtual Pilot Pro"}[settings.controls.cyclic];
}
void preferences_menu::show() {
  update();
  interface.named("_preferences_visual_button").flags &= ~1u; interface.named("_preferences_visual").flags &= ~1u;
  preview_visible = false; interface.show("_preferences_screen");
}
void preferences_menu::preview() {
  if(!world || preview_zone != zone) {
    world = std::make_unique<render::landscape>(data, std::array<std::string_view, 4>{"YEMEN", "KOREA", "CYPRUS", "FORT"}.at(static_cast<size_t>(zone - 1)));
    world->initialise_model_metadata(resources::executable_data{data}); preview_zone = zone;
  }
  std::array<math::vector3, 4> const positions{{{0x83a3f77, 0x6f157fe, 0x1f400}, {0x7ccf476, 0x8e82876, 0x3e800}, {0x7312403, 0x99cbec7, 0x1f400}, {0x7b3c8e5, 0x89c0846, 0xc800}}};
  math::trigonometry const trig{data};
  math::pose camera{positions.at(static_cast<size_t>(zone - 1)), trig.transformation(zone == 2 ? 0x1c71c71cu : 0xe0000000u, 0, zone == 2 ? 0xff49f49fu : 0x00b60b61u)};
  render::landscape_options options; auto const level{settings.detail};
  options.distance = level < 7 ? 1 : (level - 6) * 2 + 1;
  options.far_object_depth = level == 0 ? 0x200000 : 0x400000;
  options.terrain_detail = level < 2 ? 0 : level < 5 ? 1 : level == 5 ? 2 : 3;
  options.models.textures = level >= 3; options.models.gouraud = level >= 6;
  options.models.lod_distance_multiplier = settings.graphics.lod_distance_multiplier;
  preview_frame.horizontal_fov = settings.graphics.field_of_view;
  options.sky.cloud_flags = level == 0 ? 0 : level < 4 ? 2 : 10;
  options.lighting.heading = 0; options.lighting.pitch = 0xc0000000;
  world->draw(preview_frame, camera, options);
  preview_visible = true;
  interface.named("_preferences_visual_button").flags |= 1; interface.named("_preferences_visual").flags |= 1;
}
void preferences_menu::paint(render::framebuffer &frame, widget const &object) {
  if(preview_visible && &object == &interface.named("_preferences_visual")) {
    frame.blit(preview_frame.image, object.area.left, object.area.top);
    std::copy_n(preview_frame.image.colours.begin(), 168, frame.image.colours.begin());
  }
}
void preferences_menu::calibration(std::array<int32_t, 4> values, unsigned int const hat) {
  auto const visible{[this](std::string_view name, bool value){ auto &object{interface.named(name)}; object.flags = (object.flags & ~1u) | static_cast<unsigned int>(value); }};
  auto const offset{[&](size_t axis, int extent){
    auto sample{settings.axes[axis]}; sample.value = values[axis];
    auto const original{simulation::joystick_curve{settings.deadband}.sample(1, sample)};
    int32_t const value{original < 0 ? -(original + INT32_MAX) : INT32_MAX - original};
    return -math::multiply_q31(value, extent);
  }};
  auto const &controls{settings.controls};
  visible("_preferences_cyclic_sprite", controls.input_type != 1);
  if(controls.input_type != 1) { int const x{471 + offset(0, 29)}, y{335 + offset(1, 25)}; interface.bounds("_preferences_cyclic_sprite", {x, y, x + 8, y + 8}); }
  visible("_preferences_throttle_vt_sprite", controls.absolute_throttle);
  if(controls.absolute_throttle) { int const y{334 + offset(3, 29)}; interface.bounds("_preferences_throttle_vt_sprite", {343, y, 351, y + 11}); }
  visible("_preferences_throttle_hz_sprite", controls.rudder);
  if(controls.rudder) { int const x{379 + offset(2, 33)}; interface.bounds("_preferences_throttle_hz_sprite", {x, 361, x + 16, 367}); }
  std::array<std::string_view, 5> const hats{"_preferences_coolie_c_sprite", "_preferences_coolie_t_sprite", "_preferences_coolie_r_sprite", "_preferences_coolie_b_sprite", "_preferences_coolie_l_sprite"};
  for(auto name : hats) visible(name, false);
  if(controls.cyclic >= 2) visible(hats[hat & 1 ? 1u : hat & 2 ? 2u : hat & 4 ? 3u : hat & 8 ? 4u : 0u], true);
}
void preferences_menu::dropdown(widget const &button, std::string const &key, std::vector<std::pair<std::string, int>> choices) {
  interface.popup(std::move(choices), [this, key](int value){ settings.set(key, value); update(); settings.save(); });
  (void)button;
}
bool preferences_menu::activate(widget &object) {
  auto const &action{object.action};
  if(action == "notify_preferences_button_" || action == "notify_mission_select_preferences_") { show(); return true; }
  if(action == "redux_field_of_view") {
    std::vector<std::pair<std::string, int>> choices;
    for(int value : {30, 45, 60, 75, 90, 100, 110, 120, 130, 140, 150}) choices.emplace_back(std::format("{} degrees", value), value);
    interface.popup(std::move(choices), [this](int value){ settings.set_field_of_view(value); update(); preview(); settings.save(); });
  } else if(action == "redux_lod_distance") {
    std::vector<std::pair<std::string, int>> choices;
    for(int value : {25, 50, 100, 200, 300, 400, 600, 800, 1200, 1600, 2400, 3200}) choices.emplace_back(std::format("{}x{}", value / 100.0, value == 100 ? " (original)" : value == 600 ? " (default)" : ""), value);
    interface.popup(std::move(choices), [this](int value){ settings.set_lod_distance_multiplier(value / 100.0); update(); preview(); settings.save(); });
  } else if(action == "notify_preferences_collective_button_") dropdown(object, "Collective control", {{"Keyboard", 0}, {"Throttle Stick", 6}});
  else if(action == "notify_preferences_tailrotor_button_") dropdown(object, "Tailrotor control", {{"Keyboard", 0}, {"Pedals", 5}});
  else if(action == "notify_preferences_cyclic_button_") dropdown(object, "Cyclic control", {{"Keyboard", 0}, {"Joystick", 1}, {"Thrustmaster FCS", 2}, {"Flightstick Pro", 3}, {"Virtual Pilot Pro", 4}});
  else if(action == "notify_preferences_visual_detail_bar_") { settings.set("Visual Detail", object.range[2] / 10); preview(); }
  else if(action == "notify_preferences_visual_button_") preview();
  else if(action == "notify_preferences_forces_bar_") settings.set("Forces Type", object.range[2] / 10);
  else if(action == "notify_preferences_volume_bar_") settings.set("Sound Volume", object.range[2]);
  else if(action == "notify_preferences_visual_resolution_button_") settings.set("Visual Resolution", &object == &interface.named("_preferences_visual_640_button"));
  else if(action == "notify_preferences_arcade_button_" || action == "notify_preferences_realistic_button_") settings.set("Model ID", action == "notify_preferences_arcade_button_" ? 2 : 0);
  else if(action == "notify_preferences_mission_planner_enabled_button_" || action == "notify_preferences_mission_planner_disabled_button_") settings.set("Mission Planner", action == "notify_preferences_mission_planner_enabled_button_");
  else if(action == "notify_preferences_animations_enabled_button_" || action == "notify_preferences_animations_disabled_button_") settings.set("Animations", action == "notify_preferences_animations_enabled_button_");
  else if(action == "notify_preferences_sound_speech_button_") settings.set("Sound Speech", !settings.speech);
  else if(action == "notify_preferences_sound_music_button_") settings.set("Sound Music", !settings.music);
  else if(action == "notify_preferences_sound_effects_off_button_") settings.set("Sound effects", 0);
  else if(action == "notify_preferences_sound_effects_min_button_" || action == "notify_preferences_sound_effects_full_button_") { settings.set("Sound effects", 1); settings.set("Sound effects level", action == "notify_preferences_sound_effects_min_button_"); }
  else return false;
  update(); settings.save(); return true;
}
} // namespace apache::frontend
