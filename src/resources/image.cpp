#include "image.h"
#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace apache::resources {
bytes read_byterun(reader &input, size_t const expected) {
  /// Decode IFF ByteRun1, including the reserved no-op control byte
  bytes result;
  result.reserve(expected);
  while(result.size() < expected) {
    unsigned int const command{input.u8()};
    if(command == 128) continue;
    size_t const count{command < 128 ? command + 1 : 257 - command};
    if(count > expected - result.size()) input.fail("ByteRun1 exceeds image extent");
    if(command < 128) {
      auto const run{input.take(count)};
      result.insert(result.end(), run.begin(), run.end());
    } else {
      result.insert(result.end(), count, input.u8());
    }
  }
  return result;
}

indexed_image read_iff(std::span<uint8_t const> const data) {
  /// Decode original packed or planar IFF pixels and retain the indexed palette
  reader input{data, "IFF image"};
  if(input.text(4) != "FORM") input.fail("missing FORM signature");
  auto const form_size{input.be32()};
  if(form_size < 4 || form_size > input.remaining()) input.fail("invalid FORM length");
  size_t const end{8 + size_t{form_size}};
  auto const kind{input.text(4)};
  if(kind != "PBM " && kind != "ILBM") input.fail("unsupported image form");
  std::span<uint8_t const> header;
  std::span<uint8_t const> body;
  std::span<uint8_t const> cmap;
  while(input.position() < end) {
    if(end - input.position() < 8) input.fail("short chunk header");
    auto const tag{input.text(4)};
    auto const size{input.be32()};
    if(size > end - input.position()) input.fail("chunk exceeds FORM");
    auto const chunk{input.take(size)};
    if(tag == "BMHD") header = chunk;
    if(tag == "BODY") body = chunk;
    if(tag == "CMAP") cmap = chunk;
    if(size & 1) input.skip(1);
  }
  reader bitmap{header, "IFF BMHD"};
  indexed_image result;
  result.width = bitmap.be16();
  result.height = bitmap.be16();
  bitmap.skip(4);
  auto const planes{bitmap.u8()};
  auto const mask{bitmap.u8()};
  auto const compression{bitmap.u8()};
  if(!result.width || !result.height || result.width > 8192 || result.height > 8192 || !planes || planes > 8) bitmap.fail("unsupported dimensions or bit depth");
  if(mask > 2 || cmap.size() > 768 || cmap.size() % 3) bitmap.fail("invalid mask or palette");
  size_t const stride{kind == "PBM " ? (result.width + 1u) & ~1u : ((result.width + 15u) / 16u) * 2u};
  size_t const row_bytes{stride * (kind == "PBM " ? 1u : planes + static_cast<unsigned int>(mask == 1))};
  size_t const expected{row_bytes * result.height};
  reader payload{body, "IFF BODY"};
  bytes raw;
  if(compression == 1) {
    raw = read_byterun(payload, expected);
  } else if(compression == 0) {
    auto const unpacked{payload.take(expected)};
    raw.assign(unpacked.begin(), unpacked.end());
  } else {
    payload.fail("unsupported compression");
  }
  result.pixels.resize(size_t{result.width} * result.height);
  for(size_t y{0}; y != result.height; ++y) {
    for(size_t x{0}; x != result.width; ++x) {
      if(kind == "PBM ") {
        result.pixels[y * result.width + x] = raw[y * stride + x];
      } else {
        unsigned int value{0};
        for(unsigned int plane{0}; plane != planes; ++plane) value |= ((raw[y * row_bytes + plane * stride + x / 8] >> (7 - x % 8)) & 1u) << plane;
        result.pixels[y * result.width + x] = static_cast<uint8_t>(value);
      }
    }
  }
  for(size_t i{0}; i != cmap.size() / 3; ++i) result.colours[i] = {cmap[i * 3], cmap[i * 3 + 1], cmap[i * 3 + 2]};
  return result;
}

void write_ppm(std::string const &filename, indexed_image const &image) {
  /// Write a portable lossless comparison image without a graphics dependency
  std::ofstream output{filename, std::ios::binary};
  output << "P6\n" << image.width << ' ' << image.height << "\n255\n";
  for(auto const index : image.pixels) {
    auto const &colour{image.colours[index]};
    char const rgb_bytes[]{static_cast<char>(colour.r), static_cast<char>(colour.g), static_cast<char>(colour.b)};
    output.write(rgb_bytes, 3);
  }
  if(!output) throw std::runtime_error{"Cannot write image: " + filename};
}
} // namespace apache::resources
