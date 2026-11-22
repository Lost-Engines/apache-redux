#pragma once
#include <array>
#include <span>
#include <vector>
#include "math/geometry.h"
#include "horizon.h"

namespace apache::render {
struct star {
  uint8_t colour{0};
  std::array<uint8_t, 3> grid{32, 32, 32};
};
std::vector<star> read_stars(std::span<uint8_t const> data);
struct sky_lighting {
  uint32_t heading{0};
  uint32_t pitch{0xc0000000};
  int32_t night{0};
};
math::vector3 light_vector(math::trigonometry const &trig, sky_lighting const &lighting);
class celestial {
  std::vector<star> stars;
public:
  celestial(resources::data_store const &data, std::string_view zone);
  int32_t draw(framebuffer &target, math::matrix3 const &camera, math::trigonometry const &trig, scene_palette const &palette, sky_conditions const &conditions, sky_lighting const &lighting) const;
};
} // namespace apache::render
