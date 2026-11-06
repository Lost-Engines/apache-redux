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
  for(auto dimensions : {configuration::resolution{640,480}, {641,480}, {1920,1080}, {3840,2160}, {800,1000}}) {
    render::rectangle const viewport{0,0,639,479};
    render::display_layout const centre{dimensions.width,dimensions.height,640,480,viewport,90};
    render::display_layout const left{dimensions.width,dimensions.height,640,480,viewport,90,render::horizontal_anchor::left};
    render::display_layout const right{dimensions.width,dimensions.height,640,480,viewport,90,render::horizontal_anchor::right};
    require(left.artwork.left == 0 && right.artwork.right == static_cast<int>(dimensions.width)-1, "side cockpit reaches screen edge");
    require(left.artwork.right-left.artwork.left == right.artwork.right-right.artwork.left && left.artwork.top == centre.artwork.top, "anchoring preserves cockpit size and vertical placement");
    require(left.scene.left == centre.scene.left && right.scene.right == centre.scene.right && left.instrument_fov == centre.instrument_fov, "cockpit anchoring leaves world projection unchanged");
  }
  // Compose a side panel, then a centred helmet mark over the sampled scene.
  for(auto anchor : {render::horizontal_anchor::left, render::horizontal_anchor::right}) {
    render::framebuffer scene{12,6}, layer{4,3}; scene.clear(17);
    render::display_layout const panel{12,6,4,3,{0,0,3,2},90,anchor};
    render::display_layout const helmet{12,6,4,3,{0,0,3,2},90};
    panel.background(scene,layer);
    layer.point(anchor == render::horizontal_anchor::left ? 0 : 3,0,224);
    panel.composite(layer,scene);
    helmet.background(scene,layer); layer.point(2,1,208); helmet.composite(layer,scene);
    auto const edge{anchor == render::horizontal_anchor::left ? 0u : 11u};
    require(scene.image.pixels[edge] == 224, "cockpit artwork stays at the outer edge");
    require(scene.image.pixels[2*12+6] == 208, "helmet mark stays centred independently of cockpit artwork");
    require(std::ranges::count(scene.image.pixels,224) == 4 && std::ranges::count(scene.image.pixels,208) == 4, "two-layer composition preserves both masks");
  }
  // Every logical rectangle must map to precisely the destination samples it owns,
  // including fractional scaling and a smaller-than-authored window.
  for(auto size : {configuration::resolution{3,2}, {9,7}, {13,11}, {16,9}}) {
    render::display_layout const layout{size.width,size.height,4,3,{0,0,3,2},90};
    for(int top{0}; top < 3; ++top) for(int left{0}; left < 4; ++left) {
      render::rectangle const logical{left,top,3,2};
      auto const mapped{layout.map(logical)};
      render::framebuffer world{size.width,size.height}, layer{4,3}; world.clear(0);
      layout.background(world,layer); layer.fill(logical,1); layout.composite(layer,world);
      for(unsigned int y{0}; y < size.height; ++y) for(unsigned int x{0}; x < size.width; ++x)
        require((world.image.pixels[size_t(y)*size.width+x] != 0) == mapped.contains(x,y), "native preview boundaries match cockpit sampling");
    }
  }
  render::framebuffer world{8,6}, overlay{4,3}; world.clear(17);
  render::display_layout layout{8,6,4,3,{0,0,3,2},90}; layout.background(world,overlay);
  resources::indexed_image black; black.width=2;black.height=1;black.pixels={0,0};
  overlay.blit(black,1,1); overlay.point(3,0,255); layout.composite(overlay,world);
  require(std::ranges::count(world.image.pixels,0)==8 && std::ranges::count(world.image.pixels,255)==4 && std::ranges::count(world.image.pixels,17)==36, "opaque black and palette 255 survive overlay composition");
  layout.background(world,overlay); render::rasterizer raster{overlay};
  std::array<render::screen_vertex,4> quad{{{0,0},{2,0},{2,2},{0,2}}}; raster.polygon(quad,0);
  require(std::ranges::count(overlay.coverage,1)==9, "rasterized instrument coverage");
  // Compare all output pixels with the direct sampling equation. Holes must
  // retain distinct world pixels even when several rows share a source row.
  for(auto size : {configuration::resolution{3,2}, {19,11}, {40,35}, {121,65}, {3840,2160}})
    for(auto anchor : {render::horizontal_anchor::left, render::horizontal_anchor::centre, render::horizontal_anchor::right}) {
      render::framebuffer image{size.width,size.height}, layer{17,13};
      for(size_t i{0}; i != image.image.pixels.size(); ++i) image.image.pixels[i] = static_cast<uint8_t>(i * 17 + i / size.width * 13);
      auto expected{image.image.pixels};
      render::display_layout const sampling{size.width,size.height,17,13,{0,0,16,12},90,anchor};
      sampling.background(image,layer);
      int const w{sampling.artwork.right-sampling.artwork.left+1}, h{sampling.artwork.bottom-sampling.artwork.top+1};
      for(unsigned int y{0}; y < 13; ++y) for(unsigned int x{0}; x < 17; ++x) {
        size_t const at{size_t{y}*17+x};
        size_t const from{size_t(sampling.artwork.top + y*h/13)*size.width + sampling.artwork.left + x*w/17};
        require(layer.image.pixels[at] == expected[from] && !layer.coverage[at], "background samples changed");
        layer.image.pixels[at] = static_cast<uint8_t>(at * 37);
        layer.coverage[at] = (at % 7 < 3 || y == 5) ? 7 : 0;
      }
      for(int y{0}; y < h; ++y) for(int x{0}; x < w; ++x) {
        size_t const from{size_t(y*13/h)*17 + x*17/w};
        if(layer.coverage[from]) expected[size_t(y+sampling.artwork.top)*size.width+x+sampling.artwork.left] = layer.image.pixels[from];
      }
      sampling.composite(layer,image);
      require(image.image.pixels == expected, "covered runs changed nearest-neighbour pixels or overwrote a hole");
    }
  // A HUD mark projected at the reference FOV must land on the same world ray
  // after the complete HUD is zoomed, in cockpit and full-screen views.
  for(unsigned int source_width : {320u,640u}) for(auto size : {configuration::resolution{640,480}, {1437,911}, {3840,2160}, {800,1000}})
    for(double fov : {30.,60.,120.,150.}) for(bool cockpit : {false,true}) {
      unsigned int const source_height{source_width*3/4};
      int const view_height{cockpit ? static_cast<int>(source_width*31/80) : static_cast<int>(source_height)};
      render::rectangle const viewport{0,0,static_cast<int>(source_width)-1,view_height-1};
      render::display_layout const layout{size.width,size.height,source_width,source_height,viewport,fov};
      render::display_layout const reference{size.width,size.height,source_width,source_height,viewport,90};
      render::projection const source{0,0,static_cast<int>(source_width),view_height,reference.instrument_fov};
      render::projection const destination{layout.scene.left,layout.scene.top,layout.scene.right-layout.scene.left+1,layout.scene.bottom-layout.scene.top+1,fov};
      render::framebuffer image{size.width,size.height}, layer{source_width,source_height};
      layer.coverage.assign(layer.image.pixels.size(),0); image.clear(17);
      std::array<math::vector3,3> const rays{{{-1000,20000,500},{0,20000,0},{1000,20000,-500}}};
      for(size_t i{0}; i < rays.size(); ++i) {
        auto const point{*source.project_unclipped(rays[i])};
        layer.point(point.x,point.y,static_cast<uint8_t>(220+i));
      }
      layout.composite_hud(layer,image,viewport,fov);
      double const zoom{destination.lateral_scale()};
      int const tolerance{static_cast<int>(std::ceil(2.0*(layout.artwork.right-layout.artwork.left+1)/source_width*zoom))+2};
      for(size_t i{0}; i < rays.size(); ++i) {
        auto const point{*destination.project_unclipped(rays[i])};
        bool found{false};
        for(int y{std::max(0,point.y-tolerance)}; y <= std::min(static_cast<int>(size.height)-1,point.y+tolerance); ++y)
          for(int x{std::max(0,point.x-tolerance)}; x <= std::min(static_cast<int>(size.width)-1,point.x+tolerance); ++x)
            found = found || image.image.pixels[size_t(y)*size.width+x] == 220+i;
        require(found, "FOV-scaled HUD marker disappeared or does not follow its world ray");
      }
      layer.coverage.assign(layer.image.pixels.size(),1);
      std::ranges::fill(layer.image.pixels,223);
      image.clear(17); layout.composite_hud(layer,image,viewport,fov);
      for(unsigned int y{0}; y < size.height; ++y) for(unsigned int x{0}; x < size.width; ++x)
        if(!layout.scene.contains(static_cast<int>(x),static_cast<int>(y)))
          require(image.image.pixels[size_t{y}*size.width+x] == 17, "enlarged HUD overwrote the instrument panel or letterbox");
    }
  // Reusing the upload buffer must still reflect palette changes and resizing.
  std::vector<render::rgba> upload;
  for(unsigned int width : {257u, 16u, 1000u}) {
    render::framebuffer image{width,2};
    for(size_t i{0}; i != image.image.pixels.size(); ++i) image.image.pixels[i] = static_cast<uint8_t>(i);
    for(unsigned int phase : {0u, 23u}) {
      for(unsigned int i{0}; i < 256; ++i) image.image.colours[i] = {static_cast<uint8_t>(i+phase),static_cast<uint8_t>(255-i),static_cast<uint8_t>(i*17+phase)};
      image.colour_pixels(upload);
      require(upload.size() == image.image.pixels.size(), "RGBA upload buffer has stale dimensions");
      auto const allocated{image.colour_pixels()};
      for(size_t i{0}; i != upload.size(); ++i) {
        auto const colour{image.image.colours[image.image.pixels[i]]};
        require(upload[i].r == colour.r && upload[i].g == colour.g && upload[i].b == colour.b && upload[i].a == 255, "RGBA palette conversion changed");
        require(allocated[i].r == upload[i].r && allocated[i].g == upload[i].g && allocated[i].b == upload[i].b && allocated[i].a == 255, "allocating and reusable conversions disagree");
      }
    }
  }
  std::cout << "Resolution, FOV, clipping, square pixels and cockpit alignment checks passed\n";
  return 0;
} catch(std::exception const &e) { std::cerr << e.what() << '\n'; return 1; }
