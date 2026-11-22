#include "animation.h"
#include <algorithm>
#include <bit>
#include <stdexcept>

namespace apache::resources {
animation::animation(bytes data) : source{std::move(data)} {
  reader input{source, "ANIM movie"};
  if(input.text(4) != "ANIM") input.fail("Invalid signature");
  auto const frames{input.u16()}; image.width = input.u16(); image.height = input.u16();
  if(!image.width || !image.height || image.width % 8 || image.height % 8 || uint64_t{image.width} * image.height > 16 * 1024 * 1024 || input.u16() != 256) input.fail("Invalid indexed frame dimensions");
  image.pixels.resize(static_cast<size_t>(image.width) * image.height);
  packets.reserve(frames);
  for(unsigned int i{0}; i != frames; ++i) {
    auto const flags{input.u8()}; size_t const size{input.u32()}, palette_size{flags & 32 ? input.u32() : 768u};
    if(palette_size > size) input.fail("Palette exceeds packet");
    packets.push_back({flags, input.position(), palette_size, size}); input.skip(size);
  }
  if(input.remaining()) input.fail("Unexpected trailing movie bytes");
}
void animation::palette_frame(std::span<uint8_t const> const data, bool const runs) {
  reader input{data, "ANIM palette"};
  auto const colours{[&](unsigned int start, unsigned int count){
    if(start + count > 256) input.fail("Palette run exceeds 256 colours");
    for(unsigned int i{start}; i != start + count; ++i) image.colours[i] = {static_cast<uint8_t>(input.u8() * 4), static_cast<uint8_t>(input.u8() * 4), static_cast<uint8_t>(input.u8() * 4)};
  }};
  if(runs) { auto const count{input.u8()}; for(unsigned int i{0}; i != count; ++i) { auto const start{input.u8()}, size{input.u8()}; colours(start, size); } } else colours(0, 256);
  if(input.remaining()) input.fail("Unexpected palette bytes");
}
void animation::blocks(std::span<uint8_t const> const data) {
  reader input{data, "ANIM blocks"};
  for(unsigned int y{0}; y < image.height; y += 8) for(unsigned int x{0}; x < image.width; x += 8) {
    auto const code{input.u8()};
    if(code == 5) continue;
    if(code <= 4) {
      auto const colours{input.take(1u << code)}, bits{input.take(8u * code)};
      for(unsigned int k{0}; k != 64; ++k) {
        unsigned int value{0};
        if(code) {
          unsigned int const bit{k * code}, offset{bit / 8};
          value = bits[offset]; if(offset + 1 < bits.size()) value |= static_cast<unsigned int>(bits[offset + 1]) << 8;
          value = (value >> (bit % 8)) & ((1u << code) - 1);
        }
        image.pixels[(y + k / 8) * image.width + x + k % 8] = colours[value];
      }
    } else if(code == 6 || code == 7) {
      unsigned int const rows{code == 6 ? input.u8() : 255u}; auto const masks{input.take(static_cast<size_t>(std::popcount(rows)))}; size_t row{0};
      for(unsigned int dy{0}; dy != 8; ++dy) if(rows & (1u << dy)) {
        auto const mask{masks[row++]};
        for(unsigned int dx{0}; dx != 8; ++dx) if(mask & (1u << dx)) image.pixels[(y + dy) * image.width + x + dx] = input.u8();
      }
    } else input.fail("Unknown block opcode");
  }
  if(input.remaining()) input.fail("Unexpected block bytes");
}
void animation::delta(std::span<uint8_t const> const data) {
  reader input{data, "ANIM delta"}; auto const masks{input.take(image.pixels.size() / 8)}; input.skip(1);
  for(size_t i{0}; i != masks.size(); ++i) for(unsigned int bit{0}; bit != 8; ++bit) if(masks[i] & (128u >> bit)) image.pixels[i * 8 + bit] = input.u8();
  if(input.remaining()) input.fail("Unexpected delta bytes");
}
bool animation::advance() {
  if(next_frame == packets.size()) return false;
  auto const &packet{packets[next_frame]}; auto const data{std::span{source}.subspan(packet.offset, packet.size)};
  palette_frame(data.first(packet.palette_size), (packet.flags & 32) != 0);
  auto const pixels{data.subspan(packet.palette_size)};
  if(packet.flags & 1) {
    if(pixels.size() != image.pixels.size()) throw std::runtime_error{"ANIM raw frame has the wrong size"};
    std::ranges::copy(pixels, image.pixels.begin());
  } else if(packet.flags & 2) delta(pixels);
  else if(packet.flags & 16) blocks(pixels);
  else throw std::runtime_error{"ANIM packet has no supported frame encoding"};
  ++next_frame; return true;
}
} // namespace apache::resources
