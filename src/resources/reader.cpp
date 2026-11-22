#include "reader.h"
#include <algorithm>
#include <bit>
#include <stdexcept>

namespace apache::resources {
reader::reader(std::span<uint8_t const> const source, std::string_view const name)
  : data{source},
    context{name} {
}
size_t reader::size() const {
  return data.size();
}
size_t reader::position() const {
  return cursor;
}
size_t reader::remaining() const {
  return data.size() - cursor;
}
void reader::seek(size_t const position) {
  if(position > data.size()) fail("offset is outside the resource");
  cursor = position;
}
void reader::skip(size_t const count) {
  take(count);
}
std::span<uint8_t const> reader::take(size_t const count) {
  auto const result{slice(cursor, count)};
  cursor += count;
  return result;
}
std::span<uint8_t const> reader::slice(size_t const position, size_t const count) const {
  if(position > data.size() || count > data.size() - position) fail("truncated record");
  return data.subspan(position, count);
}
uint8_t reader::u8() {
  return take(1)[0];
}
uint16_t reader::u16() {
  auto const b{take(2)};
  return static_cast<uint16_t>(b[0] | (static_cast<unsigned int>(b[1]) << 8));
}
int16_t reader::i16() {
  return std::bit_cast<int16_t>(u16());
}
uint32_t reader::u32() {
  auto const b{take(4)};
  return b[0] | (uint32_t{b[1]} << 8) | (uint32_t{b[2]} << 16) | (uint32_t{b[3]} << 24);
}
int32_t reader::i32() {
  return std::bit_cast<int32_t>(u32());
}
uint16_t reader::be16() {
  return std::byteswap(u16());
}
uint32_t reader::be32() {
  return std::byteswap(u32());
}
std::string reader::text(size_t const count) {
  auto const value{take(count)};
  return {reinterpret_cast<char const*>(value.data()), value.size()};
}
std::string reader::terminated_text(size_t const count) {
  auto value{text(count)};
  value.resize(value.find('\0') == std::string::npos ? value.size() : value.find('\0'));
  return value;
}
void reader::fail(std::string_view const message) const {
  throw std::runtime_error{context + " at byte " + std::to_string(cursor) + ": " + std::string{message}};
}

std::vector<uint32_t> read_offset_table(std::span<uint8_t const> const data) {
  /// Read relative offsets without trusting a count or a missing record
  reader input{data, "offset archive"};
  auto const count{input.u32()};
  if(count > input.remaining() / 4) input.fail("offset table exceeds archive");
  size_t const header{4 + size_t{count} * 4};
  std::vector<uint32_t> result;
  result.reserve(count);
  for(uint32_t i{0}; i != count; ++i) {
    auto const offset{input.u32()};
    if(offset && (offset < header || offset >= data.size())) input.fail("record offset is outside payload");
    result.push_back(offset);
  }
  return result;
}

uint64_t fingerprint(std::span<uint8_t const> const data) {
  /// A reproducible non-cryptographic fingerprint for decoder comparisons
  uint64_t result{0xcbf29ce484222325ull};
  for(auto const byte : data) result = (result ^ byte) * 0x100000001b3ull;
  return result;
}
} // namespace apache::resources
