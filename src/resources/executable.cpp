#include "executable.h"
#include <stdexcept>

namespace apache::resources {
executable_data::executable_data(data_store const &data) : file{data.read(data.contains("SOFTWARE/APACHE95.EXE") ? "SOFTWARE/APACHE95.EXE" : "APACHE95.EXE")} {
  if(fingerprint(file) != 0x536bfb9c4282ab3bull) throw std::runtime_error{"Embedded resources require the January 1998 software APACHE95.EXE supplied on the European CD"};
}
std::span<uint8_t const> executable_data::at(uint32_t const address, size_t const count) const {
  reader input{file, "Embedded executable resource"};
  if(input.u16() != 0x5a4d) input.fail("Missing DOS header");
  input.seek(0x3c);
  auto const header{input.u32()};
  input.seek(header);
  if(input.u32() != 0x4550 || input.u16() != 0x14c) input.fail("Expected an i386 PE image");
  auto const sections{input.u16()};
  input.skip(12);
  auto const optional_size{input.u16()};
  input.skip(2);
  size_t const optional{input.position()};
  if(input.u16() != 0x10b || optional_size < 32) input.fail("Expected PE32 optional header");
  input.seek(optional + 28);
  auto const base{input.u32()};
  if(address < base) input.fail("Resource precedes image base");
  uint32_t const rva{address - base};
  for(unsigned int i{0}; i != sections; ++i) {
    input.seek(optional + optional_size + i * 40 + 12);
    auto const start{input.u32()};
    auto const size{input.u32()};
    auto const offset{input.u32()};
    if(rva >= start && rva - start <= size && count <= size - (rva - start)) return input.slice(size_t{offset} + rva - start, count);
  }
  input.fail("Resource is outside initialized executable sections");
}
} // namespace apache::resources
