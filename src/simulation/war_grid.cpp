#include "war_grid.h"
#include <algorithm>
#include <stdexcept>

namespace apache::simulation {
namespace {
bool valid(int32_t x, int32_t y, unsigned int side) {
  return x >= 0 && y >= 0 && x < 64 && y < 64 && side >= 1 && side <= 2;
}
bool air(int32_t classification) {
  return classification == 1 || classification == 2;
}
bool ground(int32_t classification) {
  return classification == 3 || classification == 4 || classification == 5 || classification == 7 || classification == 8;
}
size_t floor_index(int32_t x, int32_t y) {
  return static_cast<size_t>((x % 3) * 3 + y % 3);
}
int32_t patch_distance(std::array<int32_t, 2> const &a, std::array<int32_t, 2> const &b) {
  int32_t const x{std::abs(a[0] - b[0])}, y{std::abs(a[1] - b[1])};
  return std::max(x, y) + std::min(x, y) / 4;
}
}
bool radar_model(uint32_t const model) {
  return model == 46 || model == 77 || model == 111 || model == 328 || model == 526;
}
int32_t radar_range(uint32_t const model) {
  return model == 46 || model == 111 ? 10 : radar_model(model) ? 5 : 0;
}
int32_t radar_floor(uint32_t const model) {
  return model == 46 || model == 111 ? 128000 : model == 328 ? 76800 : model == 77 || model == 526 ? 12800 : 0;
}
war_sector const *war_grid::sector(int32_t const x, int32_t const y, unsigned int const side) const {
  return valid(x, y, side) ? cells[static_cast<size_t>(x * 64 + y)][side - 1].get() : nullptr;
}
war_sector *war_grid::ensure(int32_t const x, int32_t const y, unsigned int const side) {
  if(!valid(x, y, side)) return nullptr;
  auto &cell{cells[static_cast<size_t>(x * 64 + y)][side - 1]};
  if(!cell) { cell = std::make_unique<war_sector>(); ++used[side - 1]; maximum[side - 1] = std::max(maximum[side - 1], used[side - 1]); }
  return cell.get();
}
bool war_grid::insert(int32_t const x, int32_t const y, war_contact const &contact) {
  if(x < 0 || y < 0 || x >= 64 || y >= 64) return false;
  unsigned int const side{(contact.flags & 6) >> 1};
  if(contact.task == 20 || side < 1 || side > 2) return true;
  auto *cell{ensure(x, y, side)};
  if(air(contact.classification)) cell->air.insert(contact.target, contact.player);
  else if(ground(contact.classification)) cell->ground.insert(contact.target);
  else if(contact.classification != 6) throw std::runtime_error{"Invalid war-model actor class"};
  return true;
}
void war_grid::remove(int32_t const x, int32_t const y, war_contact const &contact, math::vector3 const &position, std::function<std::vector<war_contact>(int32_t, int32_t)> const &rescan) {
  unsigned int const side{(contact.flags & 6) >> 1};
  if(!valid(x, y, side) || contact.task == 20 || contact.classification == 6) return;
  auto &cell{cells[static_cast<size_t>(x * 64 + y)][side - 1]};
  if(!cell) return;
  bool const airborne{air(contact.classification)};
  if(!airborne && !ground(contact.classification)) throw std::runtime_error{"Invalid war-model actor class"};
  auto &bucket{airborne ? cell->air : cell->ground};
  if(bucket.remove(contact.target)) {
    bucket = {};
    for(auto const &item : rescan(position[0] >> 22, position[1] >> 22)) if(((item.flags & 6) >> 1) == side && (airborne ? air(item.classification) : ground(item.classification))) bucket.insert(item.target, airborne && item.player);
  }
  if(cell->empty()) { cell.reset(); --used[side - 1]; }
}
void war_grid::radar_patch(unsigned int const side, shared_target const &target, uint32_t const model, std::array<int32_t, 2> const &source, std::array<int32_t, 2> const &destination, std::function<int32_t(int32_t, int32_t)> const &height) {
  int32_t const x{destination[0]}, y{destination[1]};
  if(x < 0 || y < 0 || x >= 192 || y >= 192) return;
  auto *cell{ensure(x / 3, y / 3, side)}; if(!cell) return;
  int32_t value{radar_floor(model)};
  auto const patch_height{[&](auto const &p){ return height(p[0] * 0x155555 + 0xaaaaa, p[1] * 0x155555 + 0xaaaaa); }};
  if(source != destination) {
    std::array<int32_t, 2> previous{destination};
    for(size_t i{0}; i != 2; ++i) previous[i] += (source[i] > destination[i]) - (source[i] < destination[i]);
    auto const *prior{sector(previous[0] / 3, previous[1] / 3, side)};
    int32_t prior_floor{prior ? prior->floor[floor_index(previous[0], previous[1])] : INT32_MAX};
    if(prior_floor == INT32_MAX) prior_floor = patch_height(previous);
    if(previous != source && prior_floor != patch_height(previous) && prior_floor != value) {
      int32_t const divisor{patch_distance(source, previous)};
      if(divisor) prior_floor = std::bit_cast<int32_t>(static_cast<uint32_t>(patch_distance(source, destination)) * static_cast<uint32_t>(prior_floor / divisor));
    }
    value = std::max({value, prior_floor, patch_height(destination)});
  }
  auto &current{cell->floor[floor_index(x, y)]};
  if(!cell->radar || value < current) { cell->radar = target; current = value; }
}
void war_grid::insert_radar(int32_t const x, int32_t const y, unsigned int const side, shared_target const &target, uint32_t const model, math::vector3 const &position, std::function<int32_t(int32_t, int32_t)> const &height) {
  if(!valid(x, y, side) || !target || !radar_model(model)) return;
  std::array<int32_t, 2> const source{x * 3 + (position[0] - x * 0x400000) / 0x155555, y * 3 + (position[1] - y * 0x400000) / 0x155555};
  if(source[0] < 0 || source[1] < 0 || source[0] >= 192 || source[1] >= 192) return;
  radar_patch(side, target, model, source, source, height);
  int32_t const radius{radar_range(model) / 2 * 3 + 1};
  for(int32_t r{1}; r <= radius; ++r) for(int32_t i{0}; i <= 2 * r; ++i) {
    radar_patch(side, target, model, source, {source[0] - r, source[1] - r + i}, height);
    radar_patch(side, target, model, source, {source[0] - r + i, source[1] + r}, height);
    radar_patch(side, target, model, source, {source[0] + r, source[1] + r - i}, height);
    radar_patch(side, target, model, source, {source[0] + r - i, source[1] - r}, height);
  }
}
shared_target war_grid::radar(int32_t const x, int32_t const y, unsigned int const side) const {
  auto const *cell{sector(x >> 22, y >> 22, side)};
  return cell ? cell->radar : shared_target{};
}
int32_t war_grid::floor(int32_t const x, int32_t const y, unsigned int const side) const {
  if(x < 0 || y < 0) return INT32_MAX;
  int32_t const px{x / 0x155555}, py{y / 0x155555}; auto const *cell{sector(px / 3, py / 3, side)};
  return cell ? cell->floor[floor_index(px, py)] : INT32_MAX;
}
int32_t war_grid::status(int32_t const x, int32_t const y, unsigned int const side, std::function<int32_t(target_reference const &)> const &target_status) const {
  auto const *cell{sector(x, y, side)};
  return cell && cell->radar ? target_status(*cell->radar) : 1;
}
} // namespace apache::simulation
