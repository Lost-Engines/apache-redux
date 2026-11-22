#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <vector>
#include "reader.h"

namespace apache::resources {
int32_t constexpr terrain_step{1 << 20};
struct terrain_post {
  uint16_t height{0};
  uint8_t light{0};
  std::array<uint8_t, 2> colours{0, 0};
  uint8_t flags{0};
  std::array<uint8_t, 2> textures{0, 0};
};
struct scenery_placement {
  uint16_t sector{0};
  uint16_t model{0};
  int32_t x{0};
  int32_t y{0};
  uint16_t heading{0};
  uint16_t animation{0};
  uint16_t status{0};
};
class terrain {
  std::vector<terrain_post> posts;
  int32_t maximum{0};

public:
  terrain(std::span<uint8_t const> data);
  terrain_post const &post(unsigned int x, unsigned int y) const;
  int32_t elevation(int32_t x, int32_t y) const;
  int32_t approximate_elevation(int32_t x, int32_t y) const;
  int32_t maximum_height() const {
    return maximum;
  }
};
void initialise_scenery(std::span<scenery_placement> placements);
std::vector<scenery_placement> read_scenery(std::span<uint8_t const> data);
} // namespace apache::resources
