#include "render/rasterizer.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {
using apache::render::screen_vertex;
struct random_sequence {
  uint32_t state{0x41504143};
  unsigned int next(unsigned int limit) {
    state = state * 1664525u + 1013904223u;
    return state % limit;
  }
};
struct digest {
  uint64_t value{14695981039346656037ull};
  void add(std::span<uint8_t const> bytes) {
    for(auto byte : bytes) { value ^= byte; value *= 1099511628211ull; }
  }
  void count(size_t number) {
    for(unsigned int i{0}; i != 8; ++i) { uint8_t const byte{static_cast<uint8_t>(number & 255)}; add({&byte,1}); number >>= 8; }
  }
};
std::vector<screen_vertex> shape(random_sequence &random, unsigned int sample, int w, int h) {
  auto const rx{[&]{ return static_cast<int>(random.next(static_cast<unsigned int>(w))); }};
  auto const ry{[&]{ return static_cast<int>(random.next(static_cast<unsigned int>(h))); }};
  if(sample == 0) return {};
  if(sample == 1) return {{0,0}};
  if(sample == 2) return {{w-1,h-1}};
  if(sample == 3) return {{0,0},{w-1,h-1}};
  if(sample == 4) return {{w-1,0},{0,h-1}};
  if(sample == 5) return {{0,0},{w-1,0},{w-1,h-1},{0,h-1}};
  if(sample == 6) return {{0,0},{16,0},{16,1},{0,1}};
  if(sample == 7) return {{0,h-1},{w-1,h-1}};
  int a{rx()}, b{rx()}, c{ry()}, d{ry()};
  if(a > b) std::swap(a,b);
  if(c > d) std::swap(c,d);
  if(sample & 1) return {{a,c},{b,c},{b,d},{a,d}};
  return {{a,c},{b,ry()},{a,d}};
}
std::array<uint64_t,22> fingerprints() {
  using namespace apache::render;
  apache::resources::texture texture{64,std::vector<uint8_t>(4096)};
  std::array<uint8_t,65536> lighting;
  std::array<uint8_t,256> remap;
  for(size_t i{0}; i != texture.pixels.size(); ++i) texture.pixels[i] = static_cast<uint8_t>(i*17 + i/64*3);
  for(size_t i{0}; i != lighting.size(); ++i) lighting[i] = static_cast<uint8_t>(i*11 + (i>>8)*7);
  for(size_t i{0}; i != remap.size(); ++i) remap[i] = static_cast<uint8_t>(i*13 + 7);
  std::array<uint64_t,22> result;
  for(unsigned int mode{0}; mode != result.size(); ++mode) {
    random_sequence random;
    digest hash;
    for(auto size : {std::pair{64u,48u},std::pair{137u,85u}}) for(bool coverage : {false,true}) {
      framebuffer frame{size.first,size.second};
      rasterizer raster{frame};
      if(coverage) frame.coverage.resize(frame.image.pixels.size());
      for(unsigned int sample{0}; sample != 32; ++sample) for(unsigned int clipping{0}; clipping != 4; ++clipping) {
        for(size_t i{0}; i != frame.image.pixels.size(); ++i) frame.image.pixels[i] = static_cast<uint8_t>(i*7 + 19);
        std::ranges::fill(frame.coverage,0);
        frame.reset_clip();
        if(clipping == 1) frame.clip = {7,5,static_cast<int>(size.first)-9,static_cast<int>(size.second)-6};
        if(clipping == 2) frame.clip = {21,13,20,12};
        if(clipping == 3) frame.clip = {-8,-9,static_cast<int>(size.first)+10,static_cast<int>(size.second)+12};
        auto polygon{shape(random,sample,static_cast<int>(size.first),static_cast<int>(size.second))};
        for(auto &v : polygon) {
          v.u = static_cast<int32_t>(random.next(66u<<16));
          v.v = static_cast<int32_t>(random.next(65u<<16));
          v.intensity = static_cast<int32_t>(random.next(0x60000000u)) - 0x01000000;
        }
        if(sample & 2) std::ranges::reverse(polygon);
        if(!polygon.empty()) std::rotate(polygon.begin(),polygon.begin()+random.next(static_cast<unsigned int>(polygon.size())),polygon.end());
        material paint{.colours{sample & 1 ? 0x81468146u : 0x8b8b8b8bu},.remapping{remap},.intensity_map{lighting}};
        if(mode < 5) paint.pattern = static_cast<fill_pattern>(mode);
        if(mode >= 5 && mode < 9) { paint.pattern=fill_pattern::heat_haze; paint.heat_phase=static_cast<uint8_t>(mode-5); }
        if(mode == 9 || mode >= 15) paint.light = shading::gouraud;
        if(mode == 10 || mode == 13 || mode == 14) { paint.light=shading::constant; paint.intensity=sample & 1 ? 0x58312345 : -12345; }
        if(mode >= 11 && mode <= 16) { paint.image=&texture; paint.masked=mode%2 == 0; }
        raster.invalid_texture_samples = 0;
        if(mode == 17) {
          std::vector<perspective_vertex> vertices;
          for(auto const &v : polygon) vertices.push_back({v,static_cast<int32_t>(10000+random.next(30000))});
          raster.perspective(vertices,texture,lighting);
        } else if(mode >= 18) {
          paint.light=shading::none; paint.pattern=static_cast<fill_pattern>(mode-18);
          raster.circle(static_cast<int>(sample*7 % (size.first+40))-20,static_cast<int>(sample*13 % (size.second+40))-20,static_cast<int>(sample*5 % 100),paint);
        } else raster.fill(polygon,paint);
        hash.add(frame.image.pixels); hash.add(frame.coverage); hash.count(raster.invalid_texture_samples);
      }
    }
    result[mode] = hash.value;
  }
  return result;
}
} // namespace
int main(int argc, char **argv) try {
  // Golden digests from the pre-optimization software renderer (0704172).
  // Each material covers 512 draws, both winding orders, empty/degenerate
  // faces, clipping, coverage, transparent texels and invalid UV samples.
  std::array<uint64_t,22> constexpr expected{
    0xebe6bf3b61f26ef0ull,
    0x553660fb5e3a3f79ull,
    0x4c662ee0b585fd4bull,
    0x176d7705a2f14231ull,
    0x1547fd71b5af617aull,
    0x2874a337d8426aull,
    0x8266eecdf12cb120ull,
    0x836d6712a4a4f315ull,
    0x62c97203d8bc6ff9ull,
    0x68fc61825c784304ull,
    0xfe92c3cb8ef47652ull,
    0xf84618fdf01fd6cull,
    0xbe913d911c271203ull,
    0x7faa4ad41a08df3aull,
    0xf45f8344036e8b35ull,
    0x88b7172e12808e9bull,
    0x154e7f0c119c344aull,
    0xdf1c6e7c5b97a433ull,
    0x78770f6759f38c4aull,
    0x5b5eceb836ddc635ull,
    0x5afacee26f5da5c1ull,
    0x3d0e04be3167a85ull,
  };
  auto const actual{fingerprints()};
  if(argc == 2 && std::string_view{argv[1]} == "--fingerprints") {
    for(auto value : actual) std::cout << "0x" << std::hex << value << "ull,\n";
    return EXIT_SUCCESS;
  }
  for(size_t i{0}; i != expected.size(); ++i) if(actual[i] != expected[i])
    throw std::runtime_error{"Raster pixels, coverage or rejected samples changed for material " + std::to_string(i)};
  std::cout << "11,264 raster cases match the pre-optimization renderer\n";
  return EXIT_SUCCESS;
} catch(std::exception const &error) { std::cerr << error.what() << '\n'; return EXIT_FAILURE; }
