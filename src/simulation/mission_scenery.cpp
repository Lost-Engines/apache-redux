#include "mission_scenery.h"
#include <algorithm>
#include <deque>
#include <stdexcept>

namespace apache::simulation {
namespace {
std::array<unsigned int, 8> constexpr offsets{0, 2, 4, 8, 12, 13, 15, 19}, capacities{2, 2, 4, 4, 1, 2, 4, 4};
std::array<std::string_view, 8> constexpr tags{"GAMEPLAY AAA", "GAMEPLAY SAM", "GAMEPLAY TANKS", "GAMEPLAY APC", "GAMEPLAY RECON", "GAMEPLAY LASER", "GAMEPLAY KEEP", "GAMEPLAY EXTRA"};
}
uint32_t mission_scenery::model(unsigned int const kind, int32_t const side, unsigned int const map) {
  if(map < 1 || map > 4) throw std::runtime_error{"Invalid warzone"};
  switch(kind) {
  case 1:
    return side != 2 || map == 4 ? 211 : map == 3 ? 450 : 99;
  case 2:
    return side != 2 || map == 4 ? 80 : map == 3 ? 469 : 203;
  case 3:
    return map == 3 ? 456 : side == 2 || map == 4 ? 90 : 244;
  case 4:
    return side != 2 || map == 3 ? 208 : 54;
  default:
    return 0;
  }
}
int32_t mission_scenery::owner(int32_t const x, int32_t const y) const {
  return x >= 0 && y >= 0 && x < 64 && y < 64 ? sectors[static_cast<size_t>(x * 64 + y)].side : 0;
}
mission_scenery::mission_scenery(resources::mission_definition const &mission, std::vector<resources::scenery_placement> scenery, resources::terrain const &terrain, resources::model_archive const &models, unsigned int const map) : base{std::move(scenery)} {
  auto const settings{[&](std::string_view name)->std::span<std::vector<int32_t> const> {
    auto const found{mission.settings.find(name)};
    return found == mission.settings.end() ? std::span<std::vector<int32_t> const>{} : found->second;
  }};
  for(auto const &row : settings("GAMEPLAY SIDES")) {
    if(row.size() != 3) throw std::runtime_error{"Invalid sector allegiance"};
    if(row[0] < 0 || row[1] < 0 || row[0] >= 64 || row[1] >= 64) continue;
    int32_t const side{row[2] & 7};
    sectors[static_cast<size_t>(row[0] * 64 + row[1])].side = side & 4 ? side - 8 : side;
  }
  for(size_t kind{0}; kind != tags.size(); ++kind) {
    for(auto const &row : settings(tags[kind])) {
      if(row.size() != 2) throw std::runtime_error{"Invalid gameplay placement"};
      int32_t const x{row[0]};
      int32_t const y{row[1]};
      if(x < 0 || y < 0 || x >= 0x10000000 || y >= 0x10000000 || terrain.elevation(x, y) > 0 || objects.size() >= 2000) continue;
      auto &sector{sectors[static_cast<size_t>((x >> 22) * 64 + (y >> 22))]};
      for(size_t slot{offsets[kind]}; slot != offsets[kind] + capacities[kind]; ++slot) {
        if(!sector.objects[slot]) {
          sector.objects[slot] = objects.size();
          gameplay_object item;
          item.kind = static_cast<unsigned int>(kind + 1);
          item.combat.position = {x / 256 * 256, y / 256 * 256, 0};
          objects.push_back(std::move(item));
          break;
        }
      }
    }
  }
  calculate_front();
  auto const types{actor_types()};
  for(size_t sector{0}; sector != sectors.size(); ++sector) {
    if(sector / 64 == 63 || sector % 64 == 63) continue;
    std::vector<resources::scenery_placement> records;
    for(size_t slot{0}; slot != 12; ++slot) {
      if(auto const entry{sectors[sector].objects[slot]}) {
        auto const &item{objects[*entry]};
        uint32_t const id{model(item.kind, sectors[sector].side, map)};
        bool const alternate{sectors[sector].side == 2 && map != 2 && (types.at(id).flags & 2)};
        resources::scenery_placement placement{static_cast<uint16_t>(sector), static_cast<uint16_t>(id), item.combat.position[0], item.combat.position[1], static_cast<uint16_t>(sectors[sector].side == 2 ? 0x8000 : 0), 0, static_cast<uint16_t>(((sectors[sector].side & 3) << 1) | (alternate ? 0x2000 : 0))};
        auto const at{std::ranges::find_if(records, [&](auto const &p){return models.at(p.model)->height < models.at(id)->height;})};
        records.insert(at, placement);
      }
    }
    for(auto const &record : records) {
      extra_sectors[sector].push_back(extra.size());
      extra.push_back(record);
    }
  }
  for(size_t sector{0}; sector != sectors.size(); ++sector) {
    for(size_t slot{0}; slot != 4; ++slot) {
      if(auto const entry{sectors[sector].objects[slot]}) {
        auto &item{objects[*entry]};
        for(auto i : extra_sectors[sector]) {
          if(extra[i].x == item.combat.position[0] && extra[i].y == item.combat.position[1]) {
            item.scenery = i;
            item.combat.self = std::make_shared<target_reference>(target_reference{{target_kind::scenery, 1, static_cast<uint32_t>(i)}, item.combat.position, 1});
            break;
          }
        }
      }
    }
  }
  resources::initialise_scenery(base);
  for(size_t i{0}; i != base.size(); ++i) {
    auto &p{base[i]};
    base_sectors[p.sector].push_back(i);
    if(models.at(p.model)->metadata[7] & 1) p.status = static_cast<uint16_t>((p.status & ~6u) | ((sectors[p.sector].side & 3) << 1));
  }
  mark_laser_objects();
}
void mission_scenery::calculate_front() {
  front_distance.fill(255);
  std::deque<size_t> queue;
  for(int32_t x{1}; x < 63; ++x) {
    for(int32_t y{1}; y < 63; ++y) {
      int32_t const side{owner(x, y)};
      if(owner(x - 1, y) != side || owner(x + 1, y) != side || owner(x, y - 1) != side || owner(x, y + 1) != side) {
        size_t const i{static_cast<size_t>(x * 64 + y)};
        front_distance[i] = 0;
        queue.push_back(i);
      }
    }
  }
  while(!queue.empty()) {
    auto const i{queue.front()};
    queue.pop_front();
    int const x{static_cast<int>(i / 64)};
    int const y{static_cast<int>(i % 64)};
    for(auto const offset : std::array<std::array<int, 2>, 4>{{{-1, 0}, {1, 0}, {0, -1}, {0, 1}}}) {
      int const nx{x + offset[0]};
      int const ny{y + offset[1]};
      if(nx < 0 || ny < 0 || nx >= 64 || ny >= 64) continue;
      size_t const next{static_cast<size_t>(nx * 64 + ny)};
      unsigned int const distance{static_cast<unsigned int>(front_distance[i]) + 1};
      if(distance < front_distance[next]) {
        front_distance[next] = static_cast<uint8_t>(distance);
        queue.push_back(next);
      }
    }
  }
}
void mission_scenery::mark_laser_objects() {
  for(auto *layer : {&base, &extra}) {
    for(auto &p : *layer) {
      if(p.sector / 64 < 63 && p.sector % 64 < 63) p.status &= ~uint16_t{0x800};
    }
  }
  for(size_t sector{0}; sector != sectors.size(); ++sector) {
    if(sector / 64 == 63 || sector % 64 == 63) continue;
    for(size_t slot{13}; slot != 15; ++slot) {
      if(auto const entry{sectors[sector].objects[slot]}) {
        auto const &position{objects[*entry].combat.position};
        resources::scenery_placement *best{nullptr};
        uint32_t closest{UINT32_MAX};
        auto const scan{[&](auto &layer, auto const &indices){
          for(size_t i : indices) {
            auto &p{layer[i]};
            if(p.status & 0x800) continue;
            int64_t const dx{(p.x - position[0]) / 256};
            int64_t const dy{(p.y - position[1]) / 256};
            uint32_t const distance{math::integer_sqrt(static_cast<uint64_t>(dx * dx + dy * dy))};
            if(!best || distance < closest) {
              best = &p;
              closest = distance;
            }
          }
        }};
        scan(base, base_sectors[sector]);
        scan(extra, extra_sectors[sector]);
        if(best) best->status |= 0x800;
      }
    }
  }
}
} // namespace apache::simulation
