#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>
#include "math/fixed.h"
#include "reader.h"

namespace apache::resources {
struct attachment {
  uint32_t model{0};
  math::vector3 translation;
  math::matrix3 rotation;
};
struct primitive {
  uint8_t type{0};
  bytes attributes;
  bytes indices;
  std::vector<std::array<uint8_t, 2>> uv;
  bytes normals;
  bytes packet;
};
struct drawing_node {
  uint8_t opcode{7};
  uint32_t next{0};
  uint32_t packet_offset{0};
  primitive face;
  bytes plane;
  uint32_t mask{0};
  uint8_t shift{0};
  uint8_t child{0};
  std::vector<uint32_t> branches;
};
struct model {
  std::string name;
  std::string description;
  uint32_t kind{0};
  int32_t lod_distance{0};
  int32_t lod_next{-1};
  std::array<int32_t, 6> bounds;
  int32_t radius{0};
  int32_t height{0};
  std::array<uint32_t, 7> offsets;
  std::vector<attachment> children;
  std::vector<std::array<uint8_t, 6>> bounding_vertices;
  std::vector<math::vector3> vertices;
  std::vector<std::array<uint8_t, 6>> packed_vertices;
  std::vector<math::vector3> normals;
  uint16_t shaded_normals{0};
  std::map<uint32_t, drawing_node> program;
  uint32_t properties{0};
  std::array<int32_t, 8> metadata;
  bytes shadow;
  bytes grid_lighting;
};
using model_archive = std::vector<std::optional<model>>;
class executable_data;
void initialise_model_metadata(model_archive &models, executable_data const &executable);
model read_model(std::span<uint8_t const> data);
model_archive read_models(std::span<uint8_t const> data);

struct drawing_callbacks {
  std::function<int(primitive const &, uint32_t, bool)> face;
  std::function<int(std::span<uint8_t const>)> winding;
  std::function<void(unsigned int)> child;
};
void execute_model(model const &object, uint32_t flags, int initial_winding, drawing_callbacks const &callbacks);
} // namespace apache::resources
