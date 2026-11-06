#include "frontend/mission_session.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>
int main(int argc, char **argv) try {
  if(argc != 3) throw std::runtime_error{"Usage: graphics_scene_check DATA_DIRECTORY OUTPUT_DIRECTORY"};
  apache::resources::data_store data{argv[1]}; std::filesystem::path const output{argv[2]}; std::filesystem::create_directories(output);
  for(double fov : {30., 90., 150.}) {
    apache::frontend::mission_options options; options.detail=4; options.graphics.field_of_view=fov;
    apache::frontend::mission_session session{data,61,options}; session.step(10); session.paused=true;
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
