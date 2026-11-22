#include "texture.h"

namespace apache::resources {
std::vector<std::optional<texture>> read_object_textures(std::span<uint8_t const> const data) {
  reader input{data, "Object textures"};
  auto const offsets{read_offset_table(data)};
  std::vector<std::optional<texture>> result;
  result.reserve(offsets.size());
  for(auto const offset : offsets) {
    if(!offset) { result.emplace_back(); continue; }
    input.seek(offset);
    auto const type{input.u32()};
    auto const width{input.u32()};
    input.u32();                                                               // Runtime pixel pointer, relocated by the original loader.
    if(type > 4 || width != (16u << type)) input.fail("Invalid texture dimensions");
    auto const pixels{input.take(static_cast<size_t>(width) * width)};
    result.emplace_back(texture{width, bytes{pixels.begin(), pixels.end()}});
  }
  return result;
}
} // namespace apache::resources
