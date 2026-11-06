#include "frontend/mission_session.h"
#include "render/display_layout.h"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace apache;
void require(bool value, char const *message) { if(!value) throw std::runtime_error{message}; }
void gear(render::model_instance const &object, bool down) {
  require(bool(object.flags & 4) == down, "A-10 lower LOD gear flag");
  auto const check{[&](render::model_instance const &parent, size_t slot, uint32_t id){
    auto const &child{parent.children.at(slot)};
    require(down ? child && child->id == id : !child, "A-10 gear attachment");
    if(child) require(child->world.position == parent.world.position, "gear world transform maintained");
  }};
  check(object,2,1219); check(*object.children.at(0),1,1217); check(*object.children.at(1),3,1214);
}
int main(int argc, char **argv) try {
  if(argc < 2 || argc > 3) throw std::runtime_error{"Usage: redux_aircraft_check DATA_DIRECTORY [CAPTURE_DIRECTORY]"};
  resources::data_store data{argv[1]};
  frontend::mission_options options; options.detail=4;
  auto session_owner{std::make_unique<frontend::mission_session>(data,frontend::mission_session::quick_start(data,3),options)};
  auto &session{*session_owner};
  auto &runtime{session.runtime}; auto &actors{runtime.actors}; auto &weapons{*runtime.weapons};
  std::vector<size_t> a10s;
  for(size_t i{0}; i < actors.units.size(); ++i) if(actors.units[i].object.id == 3) a10s.push_back(i);
  require(a10s.size() == 4, "Cyprus quickstart contains four A-10s");
  for(auto index : a10s) {
    auto &unit{actors.units[index]}; auto &object{unit.object};
    auto const &route{runtime.mission.tasks[unit.location.task].actors[unit.location.group].waypoints};
    auto const departure{std::ranges::find_if(route,[](auto const &point){ return point.position[2] > 0; })};
    require(departure != route.end() && route.back().position[2] == 0, "A-10 route has flight and parking waypoints");
    auto position{route.back().position};
    int32_t const ground{runtime.terrain.elevation(position[0],position[1])};
    auto const place{[&](int feet){ position[2]=ground+feet*256; runtime.spatial.move(unit,position); actors.synchronize_models(); }};
    unit.waypoint=route.size()-1; unit.speed=0; unit.conditions &= ~10u;
    auto *wing{object.children[0].get()}; auto *pylon{wing->children[0].get()};
    place(1500); gear(object,false);
    place(800); gear(object,true);
    place(0); gear(object,true); require(actors.on_ground(index), "parked A-10 is grounded");
    auto *nose{object.children[2].get()}; actors.synchronize_models();
    require(object.children[2].get() == nose && object.children[0].get() == wing && wing->children[0].get() == pylon, "gear update preserves the model tree");
    unit.speed=5000; place(0); gear(object,true); require(actors.on_ground(index), "taxiing aircraft is grounded");
    unit.waypoint=static_cast<size_t>(departure-route.begin());
    place(50); gear(object,true); require(!actors.on_ground(index), "airborne after takeoff");
    place(150); gear(object,false);
    unit.waypoint=route.size()-1; place(0); unit.speed=0;
  }
  std::cout << "Four A-10s: approach, parked, taxi, takeoff and retraction passed\n";

  std::vector<unsigned int> launched;
  runtime.events.launched=[&](size_t, auto const &round){ launched.push_back(round.state.weapon); };
  size_t notifications{0};
  runtime.events.projectile=[&](size_t, auto &, auto const &event){ if(event.kind == simulation::projectile_event::notify) ++notifications; };
  auto const definitions{simulation::weapon_definitions()};
  auto const incoming{[&](size_t target, unsigned int guidance){
    weapons.rounds.clear(); launched.clear(); notifications=0;
    weapons.decoys[target].deadlines={};
    for(auto &slot : weapons.decoys[target].targets) *slot={};
    auto const missile{std::ranges::find_if(definitions,[&](auto const &w){ return w.movement_class == 2 && w.guidance == guidance; })};
    require(missile != definitions.end(), "guided missile exists");
    simulation::projectile_request request; request.weapon=static_cast<unsigned int>(missile-definitions.begin());
    request.source=*runtime.spatial.targets[*runtime.timing.player]; request.selected=*runtime.spatial.targets[target];
    request.launch.pose=actors.units[target].object.world;
    request.launch.pose.position[1]-=100000; request.launch.pose.position[2]+=100000;
    auto const index{weapons.launch_player(request,false)}; require(index.has_value(), "incoming missile launch");
    auto &round{weapons.rounds.at(*index)};
    // Start with a confirmed seeker lock, then exercise the real projectile notify path.
    round.adjusted.selected=true; round.state.body.seeker={1,INT32_MAX,simulation::seeker_target::selected};
    round.state.body.motion.distance=missile->guidance_start_distance; round.state.body.motion.speed=500000;
    launched.clear();
    weapons.update({.now=100,.delta=0,.seconds=0});
    require(notifications == 1, "incoming notification reached the mission");
  }};
  for(auto index : a10s) for(unsigned int guidance : {2u,3u}) {
    auto &unit{actors.units[index]}; auto const seed{unit.random}; auto const conditions{unit.conditions};
    incoming(index,guidance);
    require(launched.empty() && unit.random == seed && unit.conditions == conditions && weapons.decoys[index].deadlines == std::array<int32_t,2>{}, "parked AI must not release, retreat or reschedule countermeasures");
  }
  size_t const target{a10s.front()}; auto &unit{actors.units[target]}; auto position{unit.object.world.position};
  position[2]+=400*256; runtime.spatial.move(unit,position); actors.synchronize_models();
  for(unsigned int guidance : {2u,3u}) {
    incoming(target,guidance);
    require(launched == std::vector<unsigned int>{guidance == 2 ? 7u : 8u}, "airborne AI still releases countermeasures");
  }
  // Zero speed alone must not disable countermeasures: a helicopter can hover.
  unit.classification=1; unit.speed=0;
  incoming(target,2); require(launched == std::vector<unsigned int>{7}, "hovering helicopter releases chaff");
  unit.conditions |= 0x400;
  for(unsigned int id : {7u,8u}) { launched.clear(); weapons.fire_remote(target,id,1,{}); require(launched.empty(), "carrier-parked aircraft cannot release remote decoys"); }
  unit.conditions &= ~0x400u; unit.classification=2;
  position[2]-=400*256; runtime.spatial.move(unit,position); actors.synchronize_models();
  weapons.rounds.clear();
  for(bool const arcade : {false,true}) {
    frontend::mission_options player_options; player_options.arcade=arcade;
    auto player_owner{std::make_unique<frontend::mission_session>(data,frontend::mission_session::quick_start(data,3),player_options)};
    auto &player{*player_owner};
    auto &pilot{*player.runtime.pilot}; auto &counter{*player.runtime.weapons};
    auto p{pilot.readings().position.position}; p[2]=player.runtime.terrain.elevation(p[0],p[1])+2304;
    pilot.reposition(p); pilot.realistic.landed=pilot.arcade.landed=true;
    auto &controls{counter.player_countermeasures}; controls.mode=2; controls.ammunition={10,10}; controls.pylon_ammunition={10,10}; controls.deadline={}; controls.faults=0;
    require(!counter.release_countermeasure(7,0) && !counter.release_countermeasure(8,0), "landed player cannot release countermeasures");
    require(controls.ammunition == std::array<int32_t,2>{10,10}, "blocked release consumes no ammunition");
    pilot.realistic.landed=pilot.arcade.landed=false; p[2]+=100*256; pilot.reposition(p);
    require(counter.release_countermeasure(7,0) && counter.release_countermeasure(8,0), "airborne player can release both countermeasures");
  }
  std::cout << "Grounded, carrier, airborne, hovering and both player flight models passed\n";

  // Exercise actual F3 composition against a separately drawn native target image.
  session.paused=true; session.select_view(5);
  runtime.sensors->state.current=*runtime.spatial.targets[target]; runtime.sensors->state.locked=false;
  render::mfd_renderer displays{session.panels,runtime.executable};
  auto const logical{displays.area(render::mfd_panel::gunner_middle)};
  for(auto size : {configuration::resolution{640,480},{1920,1080},{3840,2160},{1437,911}}) {
    session.resize(size.width,size.height); session.draw();
    auto const &state{session.instruments->state}; require(state.target_model != nullptr, "F3 has the selected target");
    render::display_layout layout{size.width,size.height,640,480,{0,0,639,479},90};
    auto const area{layout.map(logical)};
    render::framebuffer expected{size.width,size.height}, old{size.width,size.height}, overlay{640,480};
    expected.clear(208); expected.clip=area;
    auto const name{session.world.draw_flir(expected,*state.target_model,state.target_camera_aim,state.helmet.aircraft.position,session.visual.lighting)};
    layout.background(expected,overlay);
    displays.flir(overlay,logical,session.world,state.target_model,state.target_camera_aim,state.helmet.aircraft.position,session.visual.lighting,[&](auto){ return name; });
    layout.composite(overlay,expected);
    layout.background(old,overlay);
    displays.flir(overlay,logical,session.world,state.target_model,state.target_camera_aim,state.helmet.aircraft.position,session.visual.lighting);
    layout.composite(overlay,old);
    size_t changed{0}, ink{0};
    for(int y{area.top}; y <= area.bottom; ++y) for(int x{area.left}; x <= area.right; ++x) {
      size_t const at{size_t(y)*size.width+x};
      require(session.frame.image.pixels[at] == expected.image.pixels[at], "F3 composition preserves native target detail and caption");
      changed += expected.image.pixels[at] != old.image.pixels[at]; ink += expected.image.pixels[at] != 208;
    }
    require(ink > 100, "target preview has visible geometry");
    require(size.width == 640 ? changed == 0 : changed > 100, "native target preview differs from enlarged low-resolution image");
    if(argc == 3 && size.width == 1920) {
      std::filesystem::create_directories(argv[2]);
      resources::write_ppm((std::filesystem::path{argv[2]}/"f3-native.ppm").string(),session.frame.image);
    }
    std::cout << size.width << 'x' << size.height << ": native F3 pixels match; " << changed << " differ from upscaling\n";
  }
  // The low-resolution cockpit artwork uses the same native target pass.
  for(double fov : {30.,150.}) {
    frontend::mission_options low_options; low_options.video_mode=0; low_options.detail=4; low_options.graphics.field_of_view=fov;
    auto low_owner{std::make_unique<frontend::mission_session>(data,frontend::mission_session::quick_start(data,3),low_options)};
    auto &low{*low_owner};
    low.paused=true; low.select_view(5); low.resize(1920,1080);
    low.runtime.sensors->state.current=*low.runtime.spatial.targets[a10s.front()];
    low.draw(); auto const &state{low.instruments->state}; require(state.target_model != nullptr, "320 cockpit target");
    render::mfd_renderer screens{low.panels,low.runtime.executable};
    auto const box{screens.area(render::mfd_panel::gunner_middle)};
    render::display_layout const layout{1920,1080,320,240,{0,0,319,239},fov};
    render::framebuffer expected{1920,1080}, layer{320,240}; expected.clear(208); expected.clip=layout.map(box);
    auto const name{low.world.draw_flir(expected,*state.target_model,state.target_camera_aim,state.helmet.aircraft.position,low.visual.lighting)};
    layout.background(expected,layer);
    screens.flir(layer,box,low.world,state.target_model,state.target_camera_aim,state.helmet.aircraft.position,low.visual.lighting,[&](auto){ return name; });
    layout.composite(layer,expected);
    auto const area{layout.map(box)};
    for(int y{area.top}; y <= area.bottom; ++y) for(int x{area.left}; x <= area.right; ++x) {
      size_t const at{size_t(y)*1920+x};
      require(low.frame.image.pixels[at] == expected.image.pixels[at], "320 cockpit preview stays native and independent of world FOV");
    }
  }
  {
    render::framebuffer image{800,600}; image.clear(208); image.image.colours=session.frame.image.colours;
    auto &object{unit.object}; object.distance=0;
    math::pose camera{object.world.position};
    camera.position[0]+=16000; camera.position[1]-=22000; camera.position[2]+=5500;
    math::vector3 direction;
    for(size_t i{0}; i < 3; ++i) direction[i]=object.world.position[i]-camera.position[i];
    direction[2]+=1600; camera.rotation=math::direction_matrix(math::normalise_q31(direction));
    auto const textures{resources::read_object_textures(data.read("3DDATA/OBJTEXT.BIN"))};
    render::model_renderer drawer{image,runtime.models,textures,session.world.palette};
    std::array<bool,3> drawn{};
    drawer.face_observer=[&](uint32_t model, uint32_t){ if(model==1214) drawn[0]=true; if(model==1217) drawn[1]=true; if(model==1219) drawn[2]=true; };
    drawer.draw(object,camera);
    require(std::ranges::all_of(drawn,[](bool value){ return value; }), "all three gear models reach the renderer");
    if(argc == 3) resources::write_ppm((std::filesystem::path{argv[2]}/"a10-gear.ppm").string(),image.image);
  }
  // Damage and empty-target states must not reveal the previous frame's target.
  runtime.sensors->state.current={}; session.draw();
  require(!session.instruments->state.target_model, "empty target display");
  runtime.pilot->set_faults(0x100); session.draw();
  auto const area{render::display_layout{1437,911,640,480,{0,0,639,479},90}.map(logical)};
  for(int y{area.top}; y <= area.bottom; ++y) for(int x{area.left}; x <= area.right; ++x) {
    auto const value{session.frame.image.pixels[size_t(y)*1437+x]};
    require(value == 208 || value == 215, "damaged F3 preview contains only failure stripes");
  }
  unit.object.children[0].reset(); actors.synchronize_models();
  require(!unit.object.children[0], "gear update must not recreate a destroyed wing");
  std::cout << "F3 target, empty and damaged displays passed\n";
  return 0;
} catch(std::exception const &e) { std::cerr << e.what() << '\n'; return 1; }
