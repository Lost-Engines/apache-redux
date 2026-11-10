#include "horizon.h"
#include <bit>
#include "math/geometry.h"

namespace apache::render {
horizon::horizon(framebuffer &destination, rectangle const viewport, scene_palette const &colours, math::trigonometry const &angles)
  : horizon{destination, viewport, destination.projection_reference.value_or(viewport), colours, angles} {}
horizon::horizon(framebuffer &destination, rectangle const viewport, rectangle const reference, scene_palette const &colours, math::trigonometry const &angles)
  : target{destination}, area{viewport}, view{reference.left, reference.top, reference.right - reference.left + 1, reference.bottom - reference.top + 1, destination.horizontal_fov},
    plane{viewport, {-(reference.right - reference.left + 1) * 500,
      -(reference.bottom - reference.top + 1) * 500 - (viewport.bottom - reference.bottom) * 1000,
      (reference.right - reference.left + 1) * 500,
      (reference.bottom - reference.top + 1) * 500 + (reference.top - viewport.top) * 1000}},
    raster{destination}, palette{colours}, trig{angles} {
  half_width = (reference.right - reference.left + 1) * 500;
  half_height = (reference.bottom - reference.top + 1) * 500;
}
void horizon::fill(std::array<vertex2, 4> const &points, uint8_t const colour) {
  raster.polygon(plane.clip(points), palette.visual_remap[colour] * 0x01010101u);
}
void horizon::ground(uint32_t const pitch_angle, uint32_t const roll_angle, sky_conditions const &conditions) {
  pitch = std::bit_cast<int32_t>(pitch_angle);
  sine = trig.sine(roll_angle); cosine = trig.cosine(roll_angle);
  if(conditions.cloud_mode == 1 && ((conditions.cloud_flags & 2) || conditions.weather == 1)) return;
  if(pitch >= 0x38e38e39) return;
  if(pitch <= -0x38e38e39) {
    uint8_t const colour{conditions.cloud_mode == 2 && ((conditions.cloud_flags & 2) || conditions.weather == 1) ? uint8_t{166} : conditions.ground_colour};
    target.fill(area, palette.visual_remap[colour]);
    return;
  }
  auto const rotation{trig.transformation(0, pitch_angle, roll_angle)};
  auto const endpoint{[&](int32_t const x){
    auto const eye{view.prepare(math::transform(rotation, {x, 0x6400, 0})).eye};
    return vertex2{static_cast<int32_t>(int64_t{eye[0]} * half_width / eye[1]), static_cast<int32_t>(int64_t{eye[2]} * half_height / eye[1]), 0};
  }};
  left = endpoint(-0x271000); right = endpoint(0x271000);
  extension = {math::multiply_q31(-sine, 0x10000000), math::multiply_q31(cosine, 0x10000000)};
  std::array<vertex2, 4> const quad{{{math::subtract(left.x, extension[0]), math::subtract(left.y, extension[1]), 0}, left, right, {math::subtract(right.x, extension[0]), math::subtract(right.y, extension[1]), 0}}};
  fill(quad, conditions.cloud_mode == 2 && (conditions.cloud_flags & 2) ? uint8_t{166} : conditions.ground_colour);
}
void horizon::sky(sky_conditions const &conditions) {
  bool const clouds{(conditions.cloud_flags & 2) && !(conditions.cloud_mode == 0 && (conditions.weather == 2 || conditions.weather == 3))};
  if(conditions.cloud_mode == 1 && (clouds || conditions.weather == 1)) { target.fill(area, palette.visual_remap[166]); return; }
  if(pitch < -0x38e38e38) return;
  if(pitch >= 0x38e38e39) { target.fill(area, palette.visual_remap[156]); return; }
  int32_t const height{view.prepare({0, 1, half_height >> 2}).eye[2]};
  std::array<int32_t, 2> const step{math::multiply_q31(-sine, height), math::multiply_q31(cosine, height)};
  vertex2 const upper_left{math::add(left.x, step[0]), math::add(left.y, step[1]), 0}, upper_right{math::add(right.x, step[0]), math::add(right.y, step[1]), 0};
  fill({upper_left, {math::add(left.x, extension[0]), math::add(left.y, extension[1]), 0}, {math::add(right.x, extension[0]), math::add(right.y, extension[1]), 0}, upper_right}, 156);
  left.intensity = INT32_MAX; right.intensity = INT32_MAX;
  std::array<vertex2, 4> const band{left, upper_left, upper_right, right};
  if(conditions.cloud_mode == 2 || !clouds) raster.fill(plane.clip(band), {.colours{0xa6a7a6a7}, .remapping{}, .light{shading::gouraud}, .intensity_map{palette.intensity}});
  else fill(band, 166);
}
} // namespace apache::render
