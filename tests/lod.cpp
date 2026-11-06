#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include "frontend/mission_frontend.h"
#include "render/lod.h"

namespace {
namespace render = apache::render;
namespace resources = apache::resources;
namespace math = apache::math;
void require(bool const condition, char const *message) {
  if(!condition) throw std::runtime_error{message};
}
struct temporary_directory {
  std::filesystem::path path{std::filesystem::temp_directory_path() / ("apache-lod-check-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))};
  temporary_directory() { std::filesystem::create_directory(path); }
  ~temporary_directory() { std::error_code error; std::filesystem::remove_all(path, error); }
};
} // anonymous namespace

int main(int argc, char **argv) try {
  if(argc < 2 || argc > 3) throw std::runtime_error{"Usage: lod_check DATA_DIRECTORY [PREFERENCES_CAPTURE.ppm]"};
  resources::data_store const data{argv[1]};
  temporary_directory directory;
  auto frontend{std::make_unique<apache::frontend::mission_frontend>(data)};
  frontend->load_profiles(directory.path);
  auto &settings{frontend->settings};
  require(settings.graphics.field_of_view == 90 && settings.graphics.lod_distance_multiplier == 6, "fresh graphics defaults");
  settings.values.set("Redux FOV", "nan"); settings.values.set("Redux LOD distance", "-1"); settings.apply();
  require(settings.graphics.field_of_view == 90 && settings.graphics.lod_distance_multiplier == 6, "invalid saved values must use defaults");
  settings.set_field_of_view(100); settings.set_lod_distance_multiplier(2); settings.save();
  settings.override_graphics(112.5, 3.5); settings.set("Sound Volume", 12);
  require(settings.graphics.field_of_view == 112.5 && settings.graphics.lod_distance_multiplier == 3.5, "unrelated preference discarded command-line overrides");
  settings.save();
  apache::configuration::settings restored{data}; restored.load(directory.path);
  require(restored.graphics.field_of_view == 100 && restored.graphics.lod_distance_multiplier == 2, "temporary command-line overrides replaced saved choices");
  frontend->preferences.show();
  render::framebuffer frame{640, 480};
  auto &menu{frontend->interface}; menu.render(frame);
  require(menu.named("_preferences_fov_button").text == "Field of view: 112.5 deg", "menu does not show effective FOV");
  auto const click{[&](int x, int y){ menu.move(x,y); menu.button(true); menu.button(false); menu.render(frame); }};
  click(165,34); click(165,144); // 120 degrees in the FOV dropdown
  require(settings.graphics.field_of_view == 120, "FOV dropdown did not override the command line");
  click(165,59); click(165,125); // 6x in the LOD dropdown
  require(settings.graphics.lod_distance_multiplier == 6, "LOD dropdown did not override the command line");
  if(argc == 3) resources::write_ppm(argv[2], frame.image);
  frontend->back();
  require(frontend->options.graphics.field_of_view == 120 && frontend->options.graphics.lod_distance_multiplier == 6, "menu settings did not reach mission options");
  restored.load(directory.path);
  require(restored.graphics.field_of_view == 120 && restored.graphics.lod_distance_multiplier == 6, "menu settings did not survive restart");
  frontend.reset();
  std::cout << "Saved settings, command-line precedence and actual menu clicks passed\n";

  auto models{resources::read_models(data.read("3DDATA/3DOBJS.BIN"))};
  resources::initialise_model_metadata(models, resources::executable_data{data});
  auto const textures{resources::read_object_textures(data.read("3DDATA/OBJTEXT.BIN"))};
  render::scene_palette palette{data,"CYPRUS"}; palette.prepare(0);
  render::framebuffer image{640,480};
  render::model_renderer drawer{image,models,textures,palette};
  math::trigonometry const trig{data};
  resources::terrain terrain{data.read("CYPRUS.WZ/GRIDPOST.BIN")};
  int const x{32 << 22}, y{32 << 22}, ground{terrain.elevation(x,y)};
  math::pose const camera{{x,y-400000,ground+60000},math::identity};
  math::pose const position{{x,y,ground+30000},trig.transformation(0x10000000,0,0)};
  render::sky_lighting light; light.pitch=0xc0000000;
  render::shadow_projection casting{terrain,trig,light};
  render::shadow_renderer shadow{image,models,casting,palette};
  size_t changes{0}, cases{0};
  auto object{render::construct_model(models,3,position)}; // A-10 and its attached wings/stores
  for(int32_t distance : {0,100000,400000,1000000,3000000}) {
    object.distance=distance; drawer.style.lod_distance_multiplier=1; image.clear(); drawer.draw(object,camera);
    auto const original{image.image.pixels};
    object.distance=distance*6; drawer.style.lod_distance_multiplier=1; image.clear(); drawer.draw(object,camera);
    changes += original != image.image.pixels;
    drawer.style.lod_distance_multiplier=6; image.clear(); drawer.draw(object,camera);
    require(image.image.pixels == original, "scaled model/attachment LOD differs from reference detail");
    object.distance=distance; shadow.lod_distance_multiplier=1; image.clear(); shadow.draw(object,camera);
    auto const original_shadow{image.image.pixels};
    object.distance=distance*6; shadow.lod_distance_multiplier=6; image.clear(); shadow.draw(object,camera);
    require(image.image.pixels == original_shadow, "shadow LOD does not match scaled model LOD");
    ++cases;
  }
  require(changes > 0, "test never crossed a visible model LOD transition");
  std::cout << cases << " model and shadow distances, including attached parts, passed\n";

  auto world{std::make_unique<render::landscape>(data,"CYPRUS")};
  world->initialise_model_metadata(resources::executable_data{data});
  render::landscape_options options; options.distance=1; options.scenery=false; options.features=false;
  options.shadows=false; options.celestial_bodies=false; options.sky.cloud_flags=0;
  options.models.lod_distance_multiplier=1; image.clear(); world->draw(image,camera,options);
  auto const terrain_pixels{image.image.pixels};
  for(double multiplier : {.1,6.,32.}) {
    options.models.lod_distance_multiplier=multiplier; image.clear(); world->draw(image,camera,options);
    require(image.image.pixels == terrain_pixels, "LOD multiplier changed terrain rendering/cutoff");
  }
  math::pose distant{camera}; distant.position[1]+=3 << 22;
  auto const id{world->mobiles.add(3,distant)};
  unsigned int draws{0}; world->mobiles.at(id).maintained=[&](auto const &){ ++draws; };
  options.models.lod_distance_multiplier=1; world->draw(image,camera,options);
  require(draws == 0, "reference distance unexpectedly included distant aircraft");
  options.models.lod_distance_multiplier=6; world->draw(image,camera,options);
  require(draws == 1, "expanded distance did not scan and draw distant aircraft");
  std::cout << "Distant object visibility and unchanged terrain cutoff passed\n";
  return EXIT_SUCCESS;
} catch(std::exception const &error) { std::cerr << error.what() << '\n'; return EXIT_FAILURE; }
