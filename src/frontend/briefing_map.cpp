#include "briefing_map.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdio>

namespace apache::frontend {
namespace {
std::array<std::string_view, 17> constexpr key_names{"", "Key Grid Lines", "Key Waypoints", "Key Other Way", "Key Hill Contours", "Key Compass Rose", "Key Allied AAA", "Key Allied SAM", "Key Allied EWR", "Key Allied Tanks", "Key Enemy AAA", "Key Enemy SAM", "Key Enemy EWR", "Key Enemy Tanks", "Key Sides", "Key Roads", "Key Rails"};
std::string weapon_name(int const kind) {
  switch(kind) { case 2: return "Hellfire Laser"; case 3: return "Hellfire Radar"; case 4: return "Hydra 255"; case 5: return "Hydra 261"; case 6: return "Stingers"; default: return "None"; }
}
}
briefing_map::briefing_map(resources::data_store const &data, menu &ui, configuration::settings &preferences)
  : data{data}, interface{ui}, settings{preferences}, font{resources::executable_data{data}, true} {
  for(auto &object : interface.objects()) if(object.type == 15) {
    int field{object.name == "X" ? 0 : object.name == "Y" ? 1 : object.name == "Z" ? 2 : object.name == "Speed" ? 3 : object.name == "time" ? 4 : -1};
    if(field >= 0) object.number = &number_values[static_cast<size_t>(field)];
  }
  auto const sheet{resources::read_iff(data.read("UIDATA/MOUSE.LBM"))};
  marker.width = 18; marker.height = 12; marker.pixels.resize(18 * 12);
  std::vector<uint8_t> colours;
  for(size_t y{0}; y != 12; ++y) for(size_t x{0}; x != 18; ++x) {
    auto const c{sheet.pixels.at(y * sheet.width + 256 + x)};
    if(c <= 1) continue;
    auto at{std::ranges::find(colours, c)};
    if(at == colours.end()) { if(colours.size() >= 2) throw std::runtime_error{"Invalid map camera marker"}; colours.push_back(c); at = colours.end() - 1; }
    auto const index{253 + static_cast<size_t>(at - colours.begin())}; marker.pixels[y * 18 + x] = static_cast<uint8_t>(index); marker.colours[index] = sheet.colours[c];
  }
}
void briefing_map::visible(std::string_view const name, bool const value) { auto &object{interface.named(name)}; object.flags = (object.flags & ~1u) | static_cast<unsigned int>(value); }
void briefing_map::show(std::string_view const name) { visible(name, true); interface.raise(name); }
void briefing_map::open(mission_session &session) {
  mission = &session; active_tool = dragging = 0; preview_world.reset(); preview_dirty = true;
  visible("_map_3d_view_box", false); visible("_map_3d_position_sprite", false);
  for(auto &object : interface.objects()) {
    if(object.action == "notify_map_tool_" && object.type == 10) object.on = object.item == 0;
    if(object.action == "notify_map_key_button_" && object.item > 0 && object.item < 17) object.on = settings.values.integer(key_names[static_cast<size_t>(object.item)]).value_or(1) != 0;
  }
  auto &r{mission->runtime}; auto const &actor{r.actors.units.at(*r.timing.player)};
  task_index = actor.location.task; editor.attach(r.mission.tasks.at(task_index), actor.location.group);
  routes(); route_controls(); route_information();
  fit(); visible("_map_tool_box", true);
}
void briefing_map::routes() {
  commands = map_route_commands(mission->runtime.mission.tasks.at(task_index), editor.group, settings.values.integer("Key Other Way").value_or(0) != 0);
}
void briefing_map::route_controls() {
  auto const *actor{editor.actor()}; auto const letters{actor ? waypoint_letters(actor->waypoints) : std::vector<uint8_t>{}};
  for(auto &button : interface.objects()) if(button.action == "notify_waypoint_tool_button_") {
    bool const available{button.item >= 0 && static_cast<size_t>(button.item) < letters.size()};
    button.flags = (button.flags & ~1u) | static_cast<unsigned int>(available);
    button.on = button.item == editor.selected;
    if(available) button.text = std::string(1, static_cast<char>(letters[static_cast<size_t>(button.item)]));
  }
  visible("_map_waypoint_insert", !editor.fixed() && actor->waypoints.size() < 40);
  visible("_map_waypoint_delete", !editor.fixed());
}
void briefing_map::route_information() {
  auto const *point{editor.point()};
  for(auto &object : interface.objects()) if(object.type == 15 && (object.name == "X" || object.name == "Y")) object.number_max = point ? map_editor::grid(0xfffffff) : 0;
  if(!point) { interface.named("_title_text").text = "No Waypoint"; number_values.fill(0); interface.named("_type_text").text = "-"; return; }
  auto const &actor{*editor.actor()}; auto const letters{waypoint_letters(actor.waypoints)};
  std::string title{"Waypoint "}; title += static_cast<char>(letters[static_cast<size_t>(editor.selected)]);
  if(point->type == 4) {
    auto const &r{mission->runtime};
    if(auto target{simulation::nearest_ground_object(r.scene, point->position[0], point->position[1])}) {
      auto const &object{(target->layer ? r.scene.extra : r.scene.base).at(target->index)};
      if(object.model < r.models.size() && r.models[object.model]) title += " (" + r.models[object.model]->description + ")";
    }
  } else if(editor.fixed()) title += " (Fixed)";
  interface.named("_title_text").text = std::move(title);
  number_values = {map_editor::grid(point->position[0]), map_editor::grid(point->position[1]), point->position[2] / 256, point->speed / 432, actor.start_time / 200};
  std::array<char, 160> text;
  if(point->time == -1) interface.named("_travel_time").text = "Time to this waypoint: not reached";
  else {
    int32_t const ticks{math::add(point->time, number_values[4] * 200)};
    std::snprintf(text.data(), text.size(), "Time to this waypoint: %d:%02d:%02d", ticks / 720000, (ticks % 720000) / 12000, (ticks % 12000) / 200);
    interface.named("_travel_time").text = text.data();
  }
  std::snprintf(text.data(), text.size(), "Length this leg: %dft (%d miles)", point->distance / 256, point->distance / 0x14a000);
  interface.named("_distance").text = text.data();
  std::array<std::string_view, 11> constexpr types{"Not Used", "Departure", "Turning", "Cap Start", "Target", "Not Used", "Recon", "Landing", "Approach", "Initial Point", "Cap End"};
  auto const type{editor.display_type()}; interface.named("_type_text").text = type >= 0 && type < 11 ? types[static_cast<size_t>(type)] : "Not Used";
}
void briefing_map::number_changed(widget &object, bool const final) {
  if(!mission || !editor.point()) return;
  if(!final) {
    if(object.action == "notify_waypoint_position_") { editor.position(number_values[0], number_values[1], number_values[2]); if(editor.fixed()) route_information(); }
    else if(object.action == "notify_waypoint_speed_") editor.speed(number_values[3]);
    else if(object.action == "notify_waypoint_time_") editor.time(number_values[4]);
  }
  dirty = true;
}
void briefing_map::fit() {
  routes();
  std::vector<render::vertex2> points;
  for(auto const &command : commands) if(command.kind == 1 || command.kind == 2) points.push_back({command.x, command.y});
  view.fit(points); dirty = true;
}
void briefing_map::refresh() {
  routes();
  auto const &r{mission->runtime}; auto const &palette{mission->world.palette.colours};
  image.reset_clip();
  for(size_t i{0}; i != 256; ++i) image.image.colours[i] = {palette[i * 3], palette[i * 3 + 1], palette[i * 3 + 2]};
  image.image.colours[245] = {255, 0, 0};
  mission->world.draw_map(image, image.clip, view.window(), view.position.rotation, {250, 31});
  auto const enabled{[&](unsigned int item, bool fallback = true){ return settings.values.integer(key_names[item]).value_or(fallback ? 1 : 0) != 0; }};
  overlay.clear();
  draw_map_overlay(overlay, view, r.trig, r.scene, r.mission.destroy_markers, {enabled(1), enabled(5), enabled(14), true, enabled(6), enabled(7), enabled(9, false), enabled(10), enabled(11), enabled(13, false)});
  image.blit(overlay.image, 0, 0, true); overlay.clear();
  draw_map_routes(overlay, view, commands, r.trig, font, enabled(2), enabled(3, false));
  image.blit(overlay.image, 0, 0, true); dirty = false;
}
void briefing_map::paint(render::framebuffer &target, widget const &object) {
  if(!mission) return;
  if(&object == &interface.named("_map_3d_view_graphic")) {
    if(preview_dirty) preview();
    target.blit(preview_frame.image, object.area.left, object.area.top);
    std::copy_n(preview_frame.image.colours.begin(), 224, target.image.colours.begin());
    return;
  }
  if(&object == &interface.named("_map_3d_position_sprite")) {
    target.blit(marker, object.area.left, object.area.top, true);
    target.image.colours[253] = marker.colours[253]; target.image.colours[254] = marker.colours[254];
    return;
  }
  if(&object != &interface.named("_map_screen")) return;
  preview_layout();
  if(dirty) refresh();
  target.blit(image.image, 0, 0); target.image.colours = image.image.colours;
  if(dragging == 1) {
    render::rasterizer raster{target};
    int const l{start_x - half_width}, r{start_x + half_width}, t{start_y - half_height}, b{start_y + half_height};
    raster.line({l, t}, {r, t}, 245); raster.line({r, t}, {r, b}, 245); raster.line({r, b}, {l, b}, 245); raster.line({l, b}, {l, t}, 245);
  }
}
void briefing_map::preview_layout() {
  bool const displayed{(interface.named("_map_3d_view_box").flags & 1) != 0};
  interface.named("_map_waypoint_info").on = (interface.named("_map_waypoint_information_box").flags & 1) != 0;
  visible("_map_3d_position_sprite", displayed);
  if(!displayed) return;
  auto const &box{interface.named("_map_3d_view_box").area};
  render::rectangle const area{box.left + 1, box.top + 1, box.right - 1, box.bottom - 19};
  interface.bounds("_map_3d_view_graphic", area);
  unsigned int const width{static_cast<unsigned int>(area.right - area.left + 1)}, height{static_cast<unsigned int>(area.bottom - area.top + 1)};
  if(width != preview_frame.image.width || height != preview_frame.image.height) { preview_frame = render::framebuffer{width, height}; preview_dirty = true; }
  int x{box.left + 2};
  for(auto name : {"_map_3d_view_sat", "_map_3d_view_point", "_map_3d_view_rand"}) { interface.bounds(name, {x, box.bottom - 17, x + 41, box.bottom - 2}); x += 43; }
  auto const p{view.projection(mission->runtime.trig).project({preview_camera.camera.position[0], preview_camera.camera.position[1]})};
  interface.bounds("_map_3d_position_sprite", {p.x - 7, p.y - 4, p.x + 11, p.y + 7});
}
void briefing_map::preview() {
  auto const &runtime{mission->runtime};
  if(!preview_world) {
    preview_world = std::make_unique<render::landscape>(data, mission->zone);
    preview_world->initialise_model_metadata(runtime.executable);
    preview_world->bind_scene(runtime.scene.base, runtime.scene.extra, preview_world->mobiles, preview_world->effects, preview_world->craters, preview_world->fragments);
  }
  render::landscape_options options; auto const level{settings.detail};
  options.distance = level < 7 ? 1 : (level - 6) * 2 + 1;
  options.far_object_depth = level == 0 ? 0x200000 : 0x400000;
  options.terrain_detail = level < 2 ? 0 : level < 5 ? 1 : level == 5 ? 2 : 3;
  options.models.textures = level >= 3; options.models.gouraud = level >= 6;
  options.models.lod_distance_multiplier = settings.graphics.lod_distance_multiplier;
  preview_frame.horizontal_fov = settings.graphics.field_of_view;
  options.sky.cloud_flags = level == 0 ? 0 : level < 4 ? 2 : 10;
  options.shadows = false;
  preview_world->draw(preview_frame, preview_camera.camera, options);
  preview_dirty = false;
}
void briefing_map::tick(double const seconds) {
  if(!mission || interface.current() != "_map_screen" || !(interface.named("_map_3d_view_box").flags & 1)) return;
  preview_camera.step(std::clamp(seconds, 0.0, 0.5), mission->runtime.trig, [this](int32_t x, int32_t y){ return mission->runtime.terrain.elevation(x, y); });
  preview_dirty = true;
}
void briefing_map::finish(render::framebuffer &target) {
  if(!mission || interface.current() != "_map_screen") return;
  render::palette_rgb palette;
  for(size_t i{0}; i != 256; ++i) { auto const &c{target.image.colours[i]}; palette[i * 3] = c.r; palette[i * 3 + 1] = c.g; palette[i * 3 + 2] = c.b; }
  mission->world.palette.apply_terrain_palette(palette);
  for(size_t i{0}; i != 256; ++i) target.image.colours[i] = {palette[i * 3], palette[i * 3 + 1], palette[i * 3 + 2]};
}
void briefing_map::weather() {
  auto const &weather{mission->runtime.mission};
  interface.named("_map_met_strength_text").text = std::to_string(weather.wind_strength) + " Kts";
  int degrees{std::bit_cast<int32_t>(weather.wind_direction) / 0xb60b61}; if(degrees < 0) degrees += 360;
  interface.named("_map_met_direction_text").text = std::to_string(degrees) + " Degs";
  interface.named("_map_met_weather_condition_text").text = std::array<std::string, 4>{"Clear", "Overcast", "Light Fog", "Thick Fog"}.at(static_cast<size_t>(weather.weather));
}
void briefing_map::payload() {
  auto &r{mission->runtime}; auto const &weapons{r.mission.weapons};
  r.player_armament->rearm(weapons);
  interface.named("_payload_wingtip_button").text = weapon_name(weapons[4]);
  interface.named("_payload_pod1_button").text = weapon_name(weapons[0]);
  interface.named("_payload_pod2_button").text = weapon_name(weapons[2]);
  for(auto &object : interface.objects()) if(!object.globals.empty() && object.globals.front().starts_with("_payload_") && object.type == 16 && object.globals.front() != "_payload_graphic") object.flags &= ~1u;
  auto const suffix{[](int id){ return id == 2 ? "h" : id == 3 ? "hr" : id == 4 || id == 5 ? "hd" : ""; }};
  for(std::string const side : {"r", "l"}) {
    std::string outer{suffix(weapons[2])};
    if(weapons[4] == 6) visible("_payload_" + side + "w_st" + (outer.empty() ? "" : "_" + outer), true);
    else if(!outer.empty()) visible("_payload_" + side + "o_" + outer, true);
    std::string const inner{suffix(weapons[0])}; if(!inner.empty()) visible("_payload_" + side + "i_" + inner, true);
  }
}
bool briefing_map::activate(widget &object) {
  if(!mission || interface.current() != "_map_screen") return false;
  if(object.action.starts_with("notify_waypoint_tool_") || object.action == "notify_waypoint_type_") {
    if(object.action == "notify_waypoint_tool_button_") editor.select(static_cast<size_t>(object.item));
    else if(object.action == "notify_waypoint_tool_insert_") editor.insert();
    else if(object.action == "notify_waypoint_tool_delete_") editor.erase();
    else if(object.action == "notify_waypoint_type_" && settings.planner) editor.cycle_type();
    else if(object.action == "notify_waypoint_tool_info_") { object.on = !object.on; visible("_map_waypoint_information_box", object.on); if(object.on) show("_map_waypoint_information_box"); }
    if(object.action != "notify_waypoint_type_" && object.action != "notify_waypoint_tool_info_") route_controls();
    route_information(); dirty = true; return true;
  }
  if(object.action == "notify_map_key_button_" && object.item > 0 && object.item < 17) {
    object.on = !object.on; settings.set(key_names[static_cast<size_t>(object.item)], object.on ? 1 : 0); dirty = true; return true;
  }
  if(object.action == "notify_payload_wingtip_button_" || object.action == "notify_payload_pod1_button_" || object.action == "notify_payload_pod2_button_") {
    size_t const slot{object.action == "notify_payload_wingtip_button_" ? 4u : object.action == "notify_payload_pod1_button_" ? 0u : 2u};
    std::vector<std::pair<std::string, int>> choices{{weapon_name(0), 0}};
    if(slot == 4) choices.emplace_back(weapon_name(6), 6);
    else for(int i{2}; i <= 5; ++i) choices.emplace_back(weapon_name(i), i);
    interface.popup(std::move(choices), [this, slot](int kind){ if(!mission) return; mission->runtime.mission.weapons[slot] = mission->runtime.mission.weapons[slot + 1] = kind; payload(); });
    return true;
  }
  if(object.action == "notify_map_3d_tool_") {
    preview_camera.select(static_cast<unsigned int>(object.item));
    for(auto &button : interface.objects()) if(button.action == object.action) button.on = button.item == object.item;
    tick(0); return true;
  }
  if(object.action != "notify_map_tool_") return false;
  switch(object.item) {
    case 2: view.pop(); dirty = true; break;
    case 3: fit(); break;
    case 6: show("_map_key"); break;
    case 7:
      active_tool = editor.actor() ? 7 : 0;
      for(auto &tool : interface.objects()) if(tool.action == "notify_map_tool_" && tool.type == 10) tool.on = tool.item == active_tool;
      route_controls(); if(editor.actor()) show("_map_waypoint_tool_box"); break;
    case 8: weather(); show("_map_met_box"); break;
    case 17: {
      visible("_map_payload_box", false); show("_map_3d_view_box");
      auto const r{interface.named("_map_3d_view_box").area}; interface.bounds("_map_3d_view_box", {r.left, r.top, r.left + 240, r.top + 200});
      for(auto name : {"_map_3d_view_sat", "_map_3d_view_point", "_map_3d_view_rand"}) visible(name, true);
      tick(0); break;
    }
    case 18: visible("_map_3d_view_box", false); payload(); show("_map_payload_box"); break;
    default:
      active_tool = object.item;
      for(auto &tool : interface.objects()) if(tool.action == "notify_map_tool_" && tool.type == 10) tool.on = tool.item == active_tool;
      visible("_map_waypoint_tool_box", false);
      break;
  }
  return true;
}
uint32_t briefing_map::cursor_angle() const {
  int32_t const x{cursor_world.x - view.position.x}, y{cursor_world.y - view.position.y};
  if(!x && !y) return 0;
  long double const turns{std::atan2(static_cast<long double>(x), static_cast<long double>(y)) * static_cast<long double>(0.3183098861837697) * 2147483647.0L};
  return 0u - static_cast<uint32_t>(static_cast<int64_t>(turns));
}
bool briefing_map::pointer(widget &object, pointer_action const action, int const x, int const y) {
  if(!mission || interface.current() != "_map_screen" || &object != &interface.named("_map_screen")) return false;
  cursor_world = view.projection(mission->runtime.trig).unproject(x, y);
  if(action == pointer_action::right_down) { view.centre(cursor_world.x, cursor_world.y); dirty = true; return true; }
  if(action == pointer_action::left_down) {
    if(active_tool == 7) {
      editor.click(cursor_world.x, cursor_world.y, view, mission->runtime.trig); dragging = 7;
      route_controls(); route_information(); dirty = true; return true;
    }
    preview_camera.target(cursor_world.x, cursor_world.y);
    start_x = x; start_y = y; half_width = 20; half_height = 15;
    dragging = active_tool == 1 || active_tool == 4 ? active_tool : 0; start_angle = cursor_angle();
  } else if(action == pointer_action::move && dragging == 7) {
    editor.drag(cursor_world.x, cursor_world.y, view.position.zoom); route_controls(); route_information(); dirty = true;
  } else if(action == pointer_action::move && dragging == 1) {
    half_width = std::max(20, std::abs(x - start_x)); half_height = std::max(15, std::abs(y - start_y));
    if(half_height * 640 / 480 < half_width) half_height = half_width * 480 / 640;
    else half_width = half_height * 640 / 480;
  } else if(action == pointer_action::left_up) {
    if(dragging == 1) {
      auto const centre{view.projection(mission->runtime.trig).unproject(start_x, start_y)};
      view.push(); view.set(centre.x, centre.y, static_cast<int32_t>(int64_t{view.position.zoom} * half_width / 320), view.position.rotation); dirty = true;
    } else if(dragging == 4) { view.rotate(view.position.rotation + cursor_angle() - start_angle); dirty = true; }
    dragging = 0;
  }
  return true;
}
} // namespace apache::frontend
