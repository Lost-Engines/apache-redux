#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <vector>
#include "reader.h"

namespace apache::resources {
struct rgb {
  uint8_t r{0};
  uint8_t g{0};
  uint8_t b{0};
  bool operator==(rgb const &) const = default;
};
using palette = std::array<rgb, 256>;
struct indexed_image {
  unsigned int width{0};
  unsigned int height{0};
  bytes pixels;
  palette colours;
};

indexed_image read_iff(std::span<uint8_t const> data);
bytes read_byterun(reader &input, size_t expected);
void write_ppm(std::string const &filename, indexed_image const &image);
} // namespace apache::resources
