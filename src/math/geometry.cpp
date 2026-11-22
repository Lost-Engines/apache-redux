#include "geometry.h"
#include <cmath>
#include <cstdlib>
#include <stdexcept>

namespace apache::math {
uint32_t integer_sqrt(uint64_t value) {
  uint64_t result{0};
  uint64_t bit{uint64_t{1} << 62};
  while(bit > value) bit >>= 2;
  while(bit) {
    if(value >= result + bit) {
      value -= result + bit;
      result = (result >> 1) + bit;
    } else {
      result >>= 1;
    }
    bit >>= 2;
  }
  return static_cast<uint32_t>(result);
}
int32_t normalise_q31_in_place(vector3 &value) {
  if(std::abs(int64_t{value[0]}) > 0x3fffffff || std::abs(int64_t{value[1]}) > 0x3fffffff || std::abs(int64_t{value[2]}) > 0x3fffffff) {
    for(auto &axis : value) axis /= 2;
  }
  double const x{static_cast<double>(value[0])};
  double const y{static_cast<double>(value[1])};
  double const z{static_cast<double>(value[2])};
  int64_t const length{static_cast<int64_t>(std::sqrt(x * x + y * y + z * z)) + 1};
  for(auto &axis : value) axis = std::abs(int64_t{axis}) < length ? static_cast<int32_t>((int64_t{axis} * 0x80000000ll) / length) : axis > 0 ? INT32_MAX : -INT32_MAX;
  return static_cast<int32_t>(length);
}
vector3 normalise_q31(vector3 value) {
  normalise_q31_in_place(value);
  return value;
}
matrix3 direction_matrix(vector3 const &unit) {
  auto const right{normalise_q31({unit[1], negate(unit[0]), 0})};
  auto const up{normalise_q31({subtract(multiply_q31(right[1], unit[2]), multiply_q31(right[2], unit[1])), subtract(multiply_q31(right[2], unit[0]), multiply_q31(right[0], unit[2])), subtract(multiply_q31(right[0], unit[1]), multiply_q31(right[1], unit[0]))})};
  return {right[0], right[1], right[2], unit[0], unit[1], unit[2], up[0], up[1], up[2]};
}
vector3 transform(matrix3 const &matrix, vector3 const &vector) {
  vector3 result;
  for(size_t i{0}; i != 3; ++i) result[i] = rounded_q31(product_bits(matrix[i * 3], vector[0]) + product_bits(matrix[i * 3 + 1], vector[1]) + product_bits(matrix[i * 3 + 2], vector[2]));
  return result;
}
matrix3 transpose(matrix3 const &matrix) {
  return {matrix[0], matrix[3], matrix[6], matrix[1], matrix[4], matrix[7], matrix[2], matrix[5], matrix[8]};
}
matrix3 multiply(matrix3 const &a, matrix3 const &b) {
  matrix3 result;
  for(size_t y{0}; y != 3; ++y) {
    for(size_t x{0}; x != 3; ++x) result[y * 3 + x] = rounded_q31(product_bits(a[y * 3], b[x]) + product_bits(a[y * 3 + 1], b[x + 3]) + product_bits(a[y * 3 + 2], b[x + 6]));
  }
  return result;
}
pose attach(pose const &parent, pose const &relative) {
  auto position{transform(transpose(parent.rotation), relative.position)};
  for(size_t i{0}; i != 3; ++i) position[i] = add(position[i], parent.position[i]);
  return {position, multiply(relative.rotation, parent.rotation)};
}
int32_t scale_q24(int32_t const value, int32_t const scale) {
  return std::bit_cast<int32_t>(static_cast<uint32_t>((product_bits(value, scale) + 0x800000ull) >> 24));
}
std::array<int32_t, 65> subdivide_grid(int32_t const coefficient, unsigned int const shift) {
  if(shift < 5 || shift > 31) throw std::runtime_error{"Invalid grid subdivision shift"};
  std::array<int32_t, 65> result;
  for(size_t i{0}; i <= 32; ++i) {
    auto const value{static_cast<int32_t>((int64_t{coefficient} * static_cast<int64_t>(i)) >> shift)};
    result[32 + i] = value;
    result[32 - i] = negate(value);
  }
  return result;
}
} // namespace apache::math
