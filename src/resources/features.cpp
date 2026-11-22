#include "features.h"
#include <algorithm>
#include <string>
#include "math/fixed.h"

namespace apache::resources {
namespace {
std::vector<feature_face> read_faces(reader &input, size_t const vertex_count) {
  unsigned int const count{input.u16()};
  std::vector<feature_face> result;
  result.reserve(count);
  for(unsigned int i{0}; i != count; ++i) {
    feature_face face;
    face.type = input.u8(); face.surface = input.u8();
    unsigned int const points{input.u8()};
    if(!points || (face.type > 5 && face.type != 17) || (face.type == 1 && points < 2)) input.fail("Invalid terrain feature face");
    face.colours[0] = face.colours[1] = input.u8();
    if(face.type == 3) face.colours[1] = input.u8();
    for(unsigned int j{0}; j != points; ++j) {
      auto const index{input.u8()};
      if(index >= vertex_count) input.fail("Terrain feature references a missing vertex");
      face.indices.push_back(index);
      if(face.type == 17) face.intensities.push_back(input.u8());
    }
    if(face.type < 2) face.render_indices = face.indices;
    else {
      std::array<int, 256> links;
      links.fill(-1);
      for(size_t j{0}; j != face.indices.size(); ++j) links[face.indices[j]] = j + 1 < face.indices.size() ? face.indices[j + 1] : -1;
      std::array<bool, 256> visited{};
      int index{face.indices.front()};
      while(index >= 0) {
        if(visited[static_cast<size_t>(index)]) input.fail("Cyclic terrain feature vertex chain");
        visited[static_cast<size_t>(index)] = true;
        face.render_indices.push_back(static_cast<uint8_t>(index));
        index = links[static_cast<size_t>(index)];
      }
    }
    result.push_back(std::move(face));
  }
  return result;
}
} // anonymous namespace
feature_layer::feature_layer(data_store const &data, std::string_view const zone, bool const is_super) : super{is_super} {
  std::string const prefix{std::string{zone} + ".WZ/"};
  auto const file{data.read(prefix + (super ? "SUPER.BIN" : "FEATURES.BIN"))};
  auto const offsets{read_offset_table(file)};
  auto ordered{offsets};
  std::ranges::sort(ordered);
  ordered.erase(std::unique(ordered.begin(), ordered.end()), ordered.end());
  ordered.push_back(static_cast<uint32_t>(file.size()));
  records.resize(offsets.size() + 1);                                          // On-disk feature numbers start at one.
  for(size_t i{0}; i != offsets.size(); ++i) {
    auto const offset{offsets[i]};
    if(!offset) continue;
    auto const end{*std::upper_bound(ordered.begin(), ordered.end(), offset)};
    reader input{std::span<uint8_t const>{file}.subspan(offset, end - offset), "Terrain feature"};
    size_t const vertex_offset{input.u32()}, face_offset{input.u32()}, distant_offset{input.u32()};
    input.seek(vertex_offset);
    size_t const count{input.u16()};
    if(count > 256) input.fail("Terrain feature exceeds the vertex index range");
    feature object;
    object.vertices.reserve(count);
    for(size_t j{0}; j != count; ++j) {
      feature_vertex vertex;
      vertex.flags = input.u8();
      auto const component{[&](size_t const index, int const centre, unsigned int const maximum){
        for(auto &value : vertex.components[index]) {
          auto const byte{input.u8()};
          if(byte > maximum) input.fail("Terrain feature coordinate exceeds its lookup grid");
          value = static_cast<int16_t>(static_cast<int>(byte) - centre);
        }
      }};
      if(super) { component(0, 8, 16); component(1, 128, 255); } else {
        component(1, 128, 255);
        if(vertex.flags & 4) {
          component(2, 32, 64);
          if(vertex.flags & 8) component(3, 32, 64);
        }
      }
      object.vertices.push_back(vertex);
    }
    if(input.position() != face_offset) input.fail("Terrain feature vertex/face boundary disagrees");
    object.faces = read_faces(input, count);
    object.has_distant = distant_offset != 0;
    if(distant_offset) {
      if(input.position() != distant_offset) input.fail("Terrain feature distant-face boundary disagrees");
      object.distant = read_faces(input, count);
      for(auto const &face : object.distant) for(auto const index : face.indices) if(!(object.vertices[index].flags & 16)) input.fail("Distant feature references a vertex omitted by its scanner");
    }
    if(input.remaining() > 3) input.fail("Unexpected trailing terrain feature data");
    records[i + 1] = std::move(object);
  }
  auto const layer{data.read(prefix + (super ? "SUPER1.LYR" : "FEATURE1.LYR"))};
  reader input{layer, "Terrain feature placements"};
  size_t const cells{super ? 256u : 4096u};
  if(input.size() != cells * 4) input.fail("Invalid terrain feature layer dimensions");
  placements.reserve(cells);
  while(input.remaining()) {
    auto const index{input.u32()};
    if(index >= records.size() || (index && !records[index])) input.fail("Terrain feature placement references a missing record");
    placements.push_back(index);
  }
}
uint8_t feature_surface(feature const &object, int32_t const x, int32_t const y) {
  std::vector<std::array<int32_t, 2>> points;
  for(auto const &v : object.vertices) {
    std::array<int32_t, 2> point{0, 0};
    std::array<int32_t, 4> constexpr scales{1 << 26, 1 << 18, 1 << 12, 1 << 6};
    for(size_t level{0}; level != 4; ++level) for(size_t axis{0}; axis != 2; ++axis) point[axis] += v.components[level][axis] * scales[level];
    points.push_back(point);
  }
  for(auto const &face : object.faces) {
    if(face.type < 2) continue;
    if(face.type > 5) throw std::runtime_error{"Unsupported surface-query face"};
    bool inside{true};
    auto const &indices{face.render_indices};
    for(size_t i{0}; i != indices.size(); ++i) {
      auto const &a{points.at(indices[i])}, &b{points.at(indices[(i + 1) % indices.size()])};
      int64_t const cross{int64_t{math::subtract(b[0], x)} * math::subtract(a[1], b[1]) - int64_t{math::subtract(a[0], b[0])} * math::subtract(b[1], y)};
      if(cross < 0) { inside = false; break; }
    }
    if(inside && !indices.empty()) return face.surface;
  }
  return 0;
}
uint8_t feature_layer::surface_type(int32_t x, int32_t y) const {
  int32_t const offset{super ? 0x18000000 : 0}, limit{super ? 0x40000000 : 0x10000000};
  x = math::add(x, offset); y = math::add(y, offset);
  if(x < 0 || y < 0 || x >= limit || y >= limit) return 0;
  int const shift{super ? 26 : 22}, side{super ? 16 : 64}, mask{(1 << shift) - 1};
  auto const index{placements.at(static_cast<size_t>((x >> shift) * side + (y >> shift)))};
  return index ? feature_surface(*records.at(index), x & mask, y & mask) : 0;
}
uint8_t surface_type(feature_layer const &normal, feature_layer const &super, int32_t const x, int32_t const y) {
  auto const value{normal.surface_type(x, y)};
  return value ? value : super.surface_type(x, y);
}
} // namespace apache::resources
