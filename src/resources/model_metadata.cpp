#include "executable.h"
#include "models.h"

namespace apache::resources {
void initialise_model_metadata(model_archive &models, executable_data const &executable) {
  for(size_t i{0}; i != models.size(); ++i) if(models[i]) {
    auto &object{*models[i]};
    object.name = "!Unnamed " + std::to_string(i) + "!";
    object.description = "!! Unnamed Object " + std::to_string(i) + " !!";
    object.metadata[0] = 10000; object.metadata[1] = 0;
    object.metadata[2] = object.metadata[3] = 255;
    object.metadata[6] = 0; object.metadata[7] |= 2;
  }
  reader patches{executable.at(0x44ec00, 336 * 36), "Runtime object properties"};
  auto const text{[&](uint32_t const address, size_t const limit){
    reader value{executable.at(address, limit), "Runtime object name"}; return value.terminated_text(limit);
  }};
  for(unsigned int i{0}; i != 336; ++i) {
    auto const id{patches.u32()}; auto const classification{patches.i32()}, protection{patches.i32()};
    auto const description{patches.u32()}, name{patches.u32()}, flags{patches.u32()};
    auto const symbol{patches.i32()}, damage{patches.i32()}, score{patches.i32()};
    if(id >= models.size()) patches.fail("Object patch exceeds archive");
    if(!models[id]) continue;
    auto &object{*models[id]};
    object.name = text(name, 16); object.description = text(description, 64);
    object.metadata[0] = damage; object.metadata[1] = score;
    object.metadata[2] = protection; object.metadata[3] = classification;
    object.metadata[6] = symbol; object.metadata[7] = (object.metadata[7] & ~2) | static_cast<int32_t>((flags & 1u) << 1);
  }
}
} // namespace apache::resources
