#include "trigonometry.h"
#include <cstdlib>

namespace apache::math {
trigonometry::trigonometry(resources::data_store const &data) {
  auto const load{[&](std::string_view const name, size_t const count, std::vector<int32_t> &table){
    auto const file{data.read(name)};
    resources::reader input{file, name};
    if(input.size() != count * 4) input.fail("Unexpected trigonometry table size");
    table.reserve(count);
    while(input.remaining()) table.push_back(input.i32());
  }};
  load("SINCOS.TBL", 10241, sin_cos);
  load("ARCSIN.TBL", 8193, arc_sin);
  load("ARCCOS.TBL", 8193, arc_cos);
  load("ARCTAN.TBL", 4097, arc_tan);
}
int32_t trigonometry::sine(uint32_t const angle) const {
  return sin_cos[(angle >> 19) + ((angle >> 18) & 1)];
}
int32_t trigonometry::cosine(uint32_t const angle) const {
  return sin_cos[2048 + (angle >> 19) + ((angle >> 18) & 1)];
}
uint32_t trigonometry::arcsine(int32_t const value) const {
  auto const magnitude{static_cast<uint32_t>(std::abs(int64_t{value}))};
  auto const angle{static_cast<uint32_t>(arc_sin[(magnitude >> 18) + ((magnitude >> 17) & 1)])};
  return value < 0 ? 0u - angle : angle;
}
uint32_t trigonometry::arccosine(int32_t const value) const {
  auto const magnitude{static_cast<uint32_t>(std::abs(int64_t{value}))};
  auto const angle{static_cast<uint32_t>(arc_cos[(magnitude >> 18) + ((magnitude >> 17) & 1)])};
  return value < 0 ? 0x7fffffffu - angle : angle;
}
uint32_t trigonometry::arctangent(int32_t const x, int32_t const y) const {
  if(!x) return y < 0 ? 0x80000000u : 0;
  if(!y) return x < 0 ? 0xc0000000u : 0x40000000u;
  auto const ax{static_cast<uint64_t>(std::abs(int64_t{x}))};
  auto const ay{static_cast<uint64_t>(std::abs(int64_t{y}))};
  uint32_t angle{0x20000000};
  if(ax != ay) {
    uint64_t const ratio{ax < ay ? (ax << 31) / ay : (ay << 31) / ax};
    angle = static_cast<uint32_t>(arc_tan[(ratio >> 19) + ((ratio >> 18) & 1)]);
    if(ax > ay) angle = 0x40000000u - angle;
  }
  if(y < 0) angle = 0x80000000u - angle;
  return x < 0 ? 0u - angle : angle;
}
std::array<uint32_t, 3> trigonometry::angles(matrix3 const &matrix) const {
  auto const magnitude{static_cast<uint32_t>(std::abs(int64_t{matrix[5]}))};
  uint32_t pitch{static_cast<uint32_t>(arc_sin[(magnitude >> 18) + ((magnitude >> 17) & 1)])};
  if(matrix[5] < 0) pitch = 0u - pitch;
  if(pitch == 0x40000000 || pitch == 0xc0000000) return {0, pitch, 0};
  return {arctangent(matrix[3], matrix[4]), pitch, arctangent(negate(matrix[2]), matrix[8])};
}
matrix3 trigonometry::transformation(uint32_t const heading, uint32_t const pitch, uint32_t const roll) const {
  int32_t const sh{sine(heading)};
  int32_t const ch{cosine(heading)};
  int32_t const sp{sine(pitch)};
  int32_t const cp{cosine(pitch)};
  int32_t const sr{sine(roll)};
  int32_t const cr{cosine(roll)};
  int32_t const sr_sp{multiply_q31(sr, sp)};
  int32_t const negative_cr_sp{multiply_q31(negate(cr), sp)};
  return {
    rounded_q31(product_bits(cr, ch) + product_bits(sh, sr_sp)),
    rounded_q31(product_bits(negate(cr), sh) + product_bits(ch, sr_sp)),
    multiply_q31(negate(sr), cp),
    multiply_q31(cp, sh),
    multiply_q31(cp, ch),
    sp,
    rounded_q31(product_bits(sr, ch) + product_bits(sh, negative_cr_sp)),
    rounded_q31(product_bits(negate(sr), sh) + product_bits(ch, negative_cr_sp)),
    multiply_q31(cr, cp),
  };
}
matrix3 trigonometry::rotation(vector3 const &axis, uint32_t const angle) const {
  int32_t const c{cosine(angle)};
  int32_t const s{sine(angle)};
  int32_t const xs{multiply_q31(axis[0], s)};
  int32_t const ys{multiply_q31(axis[1], s)};
  int32_t const zs{multiply_q31(axis[2], s)};
  int32_t const xx{multiply_q31(axis[0], axis[0])};
  int32_t const yy{multiply_q31(axis[1], axis[1])};
  int32_t const zz{multiply_q31(axis[2], axis[2])};
  auto const cross{[&](size_t const a, size_t const b){
    int32_t const v{multiply_q31(axis[a], axis[b])};
    return subtract(v, multiply_q31(v, c));
  }};
  int32_t const xy{cross(0, 1)};
  int32_t const xz{cross(0, 2)};
  int32_t const yz{cross(1, 2)};
  return {add(xx, multiply_q31(subtract(INT32_MAX, xx), c)), subtract(xy, zs), add(xz, ys), add(xy, zs), add(yy, multiply_q31(subtract(INT32_MAX, yy), c)), subtract(yz, xs), subtract(xz, ys), add(yz, xs), add(zz, multiply_q31(subtract(INT32_MAX, zz), c))};
}
} // namespace apache::math
