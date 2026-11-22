#include "models.h"
#include <algorithm>
#include <bit>
#include <set>
#include <stdexcept>

namespace apache::resources {
namespace {
std::array<std::array<unsigned int, 2>, 22> constexpr layouts{{{3, 1}, {3, 1}, {3, 1}, {4, 1}, {3, 1}, {3, 1}, {2, 1}, {5, 3}, {5, 3}, {4, 1}, {4, 1}, {4, 1}, {5, 1}, {4, 1}, {4, 1}, {6, 3}, {6, 3}, {4, 2}, {5, 2}, {6, 4}, {6, 4}, {2, 1}}};
uint32_t relative(reader &input) {
  auto const distance{input.i16()};
  int64_t const result{static_cast<int64_t>(input.position()) + distance};
  if(result < 0 || result >= static_cast<int64_t>(input.size())) input.fail("Drawing branch leaves its model record");
  return static_cast<uint32_t>(result);
}
primitive read_primitive(reader &input, size_t const vertices) {
  auto const start{input.position()};
  primitive result;
  result.type = input.u8();
  auto const count{input.u8()};
  if(result.type >= layouts.size()) input.fail("Unknown model primitive");
  auto const [prefix, stride]{layouts[result.type]};
  auto const attributes{input.take(prefix - 2)};
  result.attributes.assign(attributes.begin(), attributes.end());
  for(unsigned int i{0}; i != count; ++i) {
    auto const index{input.u8()};
    if(index >= vertices) input.fail("Model primitive references an unavailable vertex");
    result.indices.push_back(index);
    if(result.type >= 17 && result.type <= 20) result.normals.push_back(input.u8());
    if(stride >= 3) result.uv.push_back({input.u8(), input.u8()});
  }
  auto const packet{input.slice(start, input.position() - start)};
  result.packet.assign(packet.begin(), packet.end());
  return result;
}
} // anonymous namespace
model read_model(std::span<uint8_t const> const data) {
  reader input{data, "3D model"};
  model result;
  result.kind = input.u32();
  if(result.kind > 1) input.fail("Unknown coordinate encoding");
  result.lod_distance = input.i32();
  result.lod_next = input.i32();
  for(auto &value : result.bounds) value = input.i32();
  result.radius = input.i32();
  result.height = input.i32();
  for(auto &offset : result.offsets) {
    offset = input.u32();
    if(offset && (offset < 188 || offset > data.size())) input.fail("Section leaves model payload");
  }
  result.name = input.terminated_text(16);
  result.description = input.terminated_text(64);
  result.properties = input.u32();
  for(auto &value : result.metadata) value = input.i32();
  if(result.offsets[0]) {
    input.seek(result.offsets[0]);
    auto const count{input.u32()};
    if(count > input.remaining() / 52) input.fail("Attachment table exceeds model");
    for(uint32_t i{0}; i != count; ++i) {
      attachment child;
      child.model = input.u32();
      for(auto &value : child.translation) value = input.i32();
      for(auto &value : child.rotation) value = input.i32();
      result.children.push_back(child);
    }
  }
  if(result.offsets[1]) {
    input.seek(result.offsets[1]);
    for(size_t i{0}; i != 8; ++i) {
      auto const raw{input.take(6)};
      std::array<uint8_t, 6> packed;
      std::ranges::copy(raw, packed.begin());
      result.bounding_vertices.push_back(packed);
    }
  }
  if(result.offsets[2]) {
    input.seek(result.offsets[2]);
    auto const count{input.u16()};
    if(count > input.remaining() / 6) input.fail("Vertex table exceeds model");
    for(unsigned int i{0}; i != count; ++i) {
      auto const raw{input.take(6)};
      std::array<uint8_t, 6> packed;
      std::ranges::copy(raw, packed.begin());
      result.packed_vertices.push_back(packed);
      math::vector3 position;
      reader vertex{raw};
      for(size_t axis{0}; axis != 3; ++axis) position[axis] = result.kind ? (static_cast<int>(raw[axis]) - 32) * 4096 + (static_cast<int>(raw[axis + 3]) - 32) * 64 : vertex.i16() * 16;
      result.vertices.push_back(position);
    }
  }
  if(result.offsets[3]) {
    input.seek(result.offsets[3]);
    auto const count{input.u16()};
    result.shaded_normals = input.u16();
    if(count > input.remaining() / 3) input.fail("Normal table exceeds model");
    for(unsigned int i{0}; i != count; ++i) {
      math::vector3 normal;
      for(auto &value : normal) value = std::bit_cast<int8_t>(input.u8());
      result.normals.push_back(normal);
    }
  }
  std::vector<uint32_t> pending;
  if(result.offsets[4]) pending.push_back(result.offsets[4]);
  while(!pending.empty()) {
    auto pc{pending.back()};
    pending.pop_back();
    while(!result.program.contains(pc)) {
      input.seek(pc);
      drawing_node node;
      node.opcode = input.u8();
      switch(node.opcode) {
      case 7: break;
      case 0:
      case 1: {
        auto const skip{node.opcode == 1 ? input.u8() : 0};
        node.packet_offset = static_cast<uint32_t>(input.position());
        node.face = read_primitive(input, result.vertices.size());
        if(node.opcode == 1 && skip != static_cast<int>(node.face.packet.size())) input.fail("Secondary primitive length disagrees with its packet");
        node.next = static_cast<uint32_t>(input.position());
        break;
      }
      case 2: {
        auto const count{input.u8()};
        auto const plane{input.take(count)};
        node.plane.assign(plane.begin(), plane.end());
        for(auto const v : node.plane) if(v >= result.vertices.size()) input.fail("Decision plane references an unavailable vertex");
        node.next = relative(input);
        node.branches = {relative(input), relative(input)};
        break;
      }
      case 3:
      case 4:
        node.mask = input.u32();
        node.next = relative(input);
        node.branches.push_back(relative(input));
        if(node.opcode == 4) node.branches.push_back(relative(input));
        break;
      case 5: {
        auto const count{input.u8()};
        node.mask = input.u32();
        node.shift = input.u8();
        if(node.shift >= 32) input.fail("Invalid animation shift");
        node.next = relative(input);
        for(unsigned int i{0}; i != count; ++i) node.branches.push_back(relative(input));
        break;
      }
      case 6:
        node.child = input.u8();
        if(node.child >= result.children.size()) input.fail("Unavailable attachment slot");
        node.next = static_cast<uint32_t>(input.position());
        break;
      default: input.fail("Unknown drawing opcode");
      }
      pending.insert(pending.end(), node.branches.begin(), node.branches.end());
      auto const next{node.next};
      bool const done{node.opcode == 7};
      result.program.emplace(pc, std::move(node));
      if(done) break;
      pc = next;
    }
  }
  auto const section{[&](size_t const slot){
    bytes value;
    auto const start{result.offsets[slot]};
    if(start) {
      auto end{data.size()};
      for(auto const offset : result.offsets) if(offset > start) end = std::min(end, static_cast<size_t>(offset));
      value.assign(data.begin() + start, data.begin() + static_cast<ptrdiff_t>(end));
    }
    return value;
  }};
  result.shadow = section(5);
  result.grid_lighting = section(6);
  return result;
}
model_archive read_models(std::span<uint8_t const> const data) {
  auto const offsets{read_offset_table(data)};
  std::set<size_t> ends(offsets.begin(), offsets.end());
  ends.insert(data.size());
  model_archive result;
  result.reserve(offsets.size());
  for(auto const start : offsets) {
    if(!start) result.emplace_back();
    else result.push_back(read_model(data.subspan(start, *ends.upper_bound(start) - start)));
  }
  return result;
}
void execute_model(model const &object, uint32_t const flags, int winding, drawing_callbacks const &callbacks) {
  if(!object.offsets[4]) return;
  std::vector<uint32_t> pending{object.offsets[4]};
  size_t steps{0};
  while(!pending.empty()) {
    auto pc{pending.back()};
    pending.pop_back();
    while(true) {
      if(++steps > 1000000) throw std::runtime_error{"Cyclic or excessive model drawing program"};
      auto const found{object.program.find(pc)};
      if(found == object.program.end()) throw std::runtime_error{"Invalid drawing-program continuation"};
      auto const &node{found->second};
      if(node.opcode == 7) break;
      pc = node.next;
      switch(node.opcode) {
      case 0: {
        auto const next_winding{callbacks.face(node.face, node.packet_offset, false)};
        if(node.face.type != 0 && node.face.type != 1 && node.face.type != 9 && node.face.type != 10) winding = next_winding;
        break;
      }
      case 1:
        if(winding >= 0) callbacks.face(node.face, node.packet_offset, true);
        break;
      case 2:
        winding = callbacks.winding(node.plane);
        pending.push_back(node.next);
        pending.push_back(node.branches[winding >= 0 ? 1 : 0]);
        pc = node.branches[winding >= 0 ? 0 : 1];
        break;
      case 3:
        if(flags & node.mask) {
          pending.push_back(node.next);
          pc = node.branches[0];
        }
        break;
      case 4:
        pending.push_back(node.next);
        pc = node.branches[flags & node.mask ? 0 : 1];
        break;
      case 5: {
        auto const selection{(flags >> node.shift) & node.mask};
        if(selection >= node.branches.size()) throw std::runtime_error{"Animation state has no authored case"};
        pending.push_back(node.next);
        pc = node.branches[selection];
        break;
      }
      case 6: callbacks.child(node.child); break;
      default: throw std::runtime_error{"Invalid decoded drawing opcode"};
      }
    }
  }
}
} // namespace apache::resources
