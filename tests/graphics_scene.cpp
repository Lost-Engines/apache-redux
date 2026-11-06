#include "frontend/mission_session.h"
#include "render/display_layout.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>
namespace {
void check_cockpit_sections(apache::resources::data_store const &data) {
  using namespace apache::render;
  for(unsigned int mode : {0u, 1u}) {
    cockpit panels{data, mode};
    unsigned int const width{mode ? 640u : 320u}, height{mode ? 480u : 240u};
    for(unsigned int view : {2u, 4u}) {
      framebuffer full{width, height}, left{width, height}, right{width, height};
      for(auto *image : {&full, &left, &right}) {
        image->clear(17); image->coverage.assign(image->image.pixels.size(), 0);
      }
      panels.draw(full, view, true);
      panels.draw(left, view, true, cockpit_section::left);
      panels.draw(right, view, true, cockpit_section::right);
      // The upper corner is disconnected from the lower rail. A vertical cut
      // through the assembled picture would incorrectly sever that long rail.
      for(unsigned int y{0}; y < height; ++y) for(unsigned int x{0}; x < width; ++x) {
        size_t const at{size_t{y} * width + x};
        bool const upper_corner{y < height / 2 && (view == 2 ? x >= width / 2 : x < width / 2)};
        bool const belongs_right{view == 2 ? upper_corner : !upper_corner};
        bool const painted{full.coverage[at] != 0};
        if(bool(left.coverage[at]) != (painted && !belongs_right) || bool(right.coverage[at]) != (painted && belongs_right))
          throw std::runtime_error{"90-degree cockpit piece assigned to the wrong edge"};
        auto const value{left.coverage[at] ? left.image.pixels[at] : right.image.pixels[at]};
        if(value != full.image.pixels[at]) throw std::runtime_error{"Splitting cockpit pieces changed the original artwork"};
      }
      for(auto size : {apache::configuration::resolution{width,height}, {1920,1080}, {3840,2160}, {1437,911}, {800,1000}}) {
        rectangle const original{0,0,static_cast<int>(width)-1,static_cast<int>(height)-1};
        display_layout const a{size.width,size.height,width,height,original,90,horizontal_anchor::left};
        display_layout const b{size.width,size.height,width,height,original,90,horizontal_anchor::right};
        framebuffer image{size.width,size.height}; image.clear(17);
        a.composite(left,image); b.composite(right,image);
        if(size.width == width && image.image.pixels != full.image.pixels)
          throw std::runtime_error{"Original-resolution cockpit placement changed"};
        // Follow every opaque source sample to each edge, including the lower
        // rail beyond the halfway point, at both integer and fractional scales.
        for(auto const &pair : {std::pair{&a,&left}, std::pair{&b,&right}}) {
          for(unsigned int y{0}; y < height; ++y) for(unsigned int x{0}; x < width; ++x) {
            size_t const at{size_t{y} * width + x};
            if(!pair.second->coverage[at]) continue;
            auto const box{pair.first->map({static_cast<int>(x),static_cast<int>(y),static_cast<int>(x),static_cast<int>(y)})};
            if(image.image.pixels[size_t(box.top)*size.width+box.left] != pair.second->image.pixels[at])
              throw std::runtime_error{"Anchored cockpit lost or overlapped a source pixel"};
          }
        }
      }
    }
  }
  std::cout << "Both 90-degree cockpit sections preserve their artwork and screen edges at both source resolutions\n";
}
}
int main(int argc, char **argv) try {
  if(argc != 3) throw std::runtime_error{"Usage: graphics_scene_check DATA_DIRECTORY OUTPUT_DIRECTORY"};
  apache::resources::data_store data{argv[1]}; std::filesystem::path const output{argv[2]}; std::filesystem::create_directories(output);
  check_cockpit_sections(data);
  for(double fov : {30., 90., 150.}) {
    apache::frontend::mission_options options; options.detail=4; options.graphics.field_of_view=fov;
    auto owner{std::make_unique<apache::frontend::mission_session>(data,61,options)};
    auto &session{*owner}; session.step(10); session.paused=true;
    for(auto size : {apache::configuration::resolution{640,480},{1920,1080},{3840,2160},{800,1000}}) {
      session.resize(size.width,size.height);
      for(unsigned int view : {0u,1u,2u,3u,4u,5u,6u,7u,8u,9u,10u,12u,13u}) {
        session.select_view(view); session.draw();
        if(session.frame.image.width!=size.width || session.frame.image.height!=size.height) throw std::runtime_error{"Wrong render dimensions"};
        if(fov==90 && size.width==1920) apache::resources::write_ppm((output/("view-"+std::to_string(view)+".ppm")).string(),session.frame.image);
        std::cout << size.width << 'x' << size.height << " FOV " << fov << " view " << view << '\n';
      }
    }
    session.quit_prompt=true; session.draw(); session.resize(1280,720); session.draw();
  }
  return 0;
} catch(std::exception const &error) { std::cerr << error.what() << '\n'; return 1; }
