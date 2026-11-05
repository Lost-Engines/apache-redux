#include "configuration/graphics.h"
#include "render/display_layout.h"
#include "render/projection.h"
#include "render/rasterizer.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace apache;
void require(bool value, char const *message) { if(!value) throw std::runtime_error{message}; }
int main() try {
  require(!configuration::parse_resolution("auto"), "automatic resolution");
  auto const size{*configuration::parse_resolution("3840x2160")};
  require(size.width == 3840 && size.height == 2160, "4K resolution");
  for(auto text : {"", "640", "0x480", "1920x1080junk", "9000x2160", "640x", "640x480x2", "160x8192"}) {
    bool failed{false}; try { configuration::parse_resolution(text); } catch(std::runtime_error const &) { failed = true; }
    require(failed, "invalid resolution accepted");
  }
  for(auto text : {"nan", "inf", "0", "151", "90junk"}) {
    bool failed{false}; try { configuration::parse_field_of_view(text); } catch(std::runtime_error const &) { failed = true; }
    require(failed, "invalid FOV accepted");
  }
  require(configuration::parse_field_of_view("72.5") == 72.5, "fractional FOV");
  render::projection normal{0, 0, 1920, 1080}, wide{0, 0, 1920, 1080, 120}, narrow{0, 0, 1920, 1080, 60};
  auto const point{normal.prepare({10000, 10000, 0})};
  require(point.screen.x == 1919, "45-degree ray reaches edge at 90-degree FOV");
  require(!wide.prepare({10000, 10000, 0}).outcode, "wide FOV must include a 45-degree ray");
  require(narrow.prepare({10000, 10000, 0}).outcode != 0, "narrow FOV must clip a 45-degree ray");
  require(wide.prepare({0, -1, 0}).outcode & 128, "behind-camera clipping");
  auto clipped{narrow.clip_line(narrow.prepare({-10000,10000,0}), narrow.prepare({10000,10000,0}))};
  require(clipped && (*clipped)[0].screen.x == 0 && (*clipped)[1].screen.x == 1919, "wide segment clips to both FOV boundaries");
  for(auto fov : {30., 60., 90., 120., 150.}) for(auto dimensions : {configuration::resolution{640,480}, {1920,1080}, {3840,2160}, {800,1000}, {8192,160}}) {
    render::projection view{0,0,static_cast<int>(dimensions.width),static_cast<int>(dimensions.height),fov};
    auto const centre{*view.project_unclipped({0,10000,0})};
    auto const x{*view.project_unclipped({1000,10000,0})}, z{*view.project_unclipped({0,10000,1000})};
    require(std::abs((x.x-centre.x)-(centre.y-z.y)) <= 1, "square-pixel projection");
    auto const restored{view.unscale(view.prepare({12345,100000,-23456}).eye)};
    require(std::abs(restored[0]-12345) <= 4 && std::abs(restored[2]+23456) <= 4, "projection inverse for sector culling");
  }
  for(auto size : {configuration::resolution{1280,720}, {3840,2160}, {800,1000}}) for(auto fov : {30.,90.,150.}) {
    render::display_layout const layout{size.width,size.height,640,480,{0,0,639,247},fov};
    render::projection world{layout.scene.left,layout.scene.top,layout.scene.right+1,layout.scene.bottom-layout.scene.top+1,fov};
    render::projection hud{0,0,640,248,layout.instrument_fov};
    auto const a{*world.project_unclipped({1000,10000,1000})}, b{*hud.project_unclipped({1000,10000,1000})};
    double const scale{double(layout.artwork.right-layout.artwork.left+1)/640};
    require(std::abs(a.x-(layout.artwork.left+b.x*scale)) <= 7 && std::abs(a.y-(layout.artwork.top+b.y*scale)) <= 7, "target marker alignment after cockpit scaling");
  }
  render::framebuffer world{8,6}, overlay{4,3}; world.clear(17);
  render::display_layout layout{8,6,4,3,{0,0,3,2},90}; layout.background(world,overlay);
  resources::indexed_image black; black.width=2;black.height=1;black.pixels={0,0};
  overlay.blit(black,1,1); overlay.point(3,0,255); layout.composite(overlay,world);
  require(std::ranges::count(world.image.pixels,0)==8 && std::ranges::count(world.image.pixels,255)==4 && std::ranges::count(world.image.pixels,17)==36, "opaque black and palette 255 survive overlay composition");
  layout.background(world,overlay); render::rasterizer raster{overlay};
  std::array<render::screen_vertex,4> quad{{{0,0},{2,0},{2,2},{0,2}}}; raster.polygon(quad,0);
  require(std::ranges::count(overlay.coverage,1)==9, "rasterized instrument coverage");
  std::cout << "Resolution, FOV, clipping, square pixels and cockpit alignment checks passed\n";
  return 0;
} catch(std::exception const &e) { std::cerr << e.what() << '\n'; return 1; }
