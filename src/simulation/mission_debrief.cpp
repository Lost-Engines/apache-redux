#include "mission_debrief.h"
#include <algorithm>
#include <stdexcept>
#include "mission_runtime.h"

namespace apache::simulation {
std::optional<target_identity> nearest_ground_object(mission_scenery const &scene, int32_t const x, int32_t const y) {
  if(x < 0 || y < 0 || x >= 0x10000000 || y >= 0x10000000) return {};
  unsigned int const sector{static_cast<unsigned int>((x >> 22) * 64 + (y >> 22))};
  std::optional<target_identity> result;
  uint32_t best{100};
  for(size_t layer : {size_t{1}, size_t{0}}) {
    auto const &objects{layer ? scene.extra : scene.base};
    for(size_t i{0}; i != objects.size(); ++i) {
      auto const &object{objects[i]};
      if(object.sector != sector || object.model == 13 || object.model == 19 || object.model == 141 || object.model == 144) continue;
      int64_t const dx{object.x / 256 - x / 256};
      int64_t const dy{object.y / 256 - y / 256};
      auto const distance{math::integer_sqrt(static_cast<uint64_t>(dx * dx + dy * dy))};
      if(distance < best) {
        best = distance;
        result = target_identity{target_kind::scenery, layer, static_cast<uint32_t>(i)};
      }
    }
  }
  return result;
}
debrief_result evaluate_debrief(mission_runtime const &runtime, debrief_records const &records, uint32_t &random, std::string_view const pilot_name) {
  if(!runtime.pilot || !runtime.timing.player) throw std::runtime_error{"Debrief requires a player mission"};
  debrief_result result;
  result.time = records.elapsed();
  std::array<int32_t, 3> counts{};
  auto const player_id{runtime.spatial.mobile_id(*runtime.timing.player)};
  for(auto const &group : runtime.actors.groups) {
    auto const flags{runtime.actors.units.at(group.units.front()).conditions};
    for(auto index : group.units) {
      if(runtime.actors.units[index].destroyed) {
        if(flags & 0x1000) ++counts[0];
        if(flags & 0x2000) ++counts[1];
        if((flags & 0x4000) && records.player_killed(runtime.spatial.mobile_id(index), player_id)) ++counts[2];
      }
    }
  }
  result.mobiles(counts, {runtime.mission.failure_count, runtime.mission.success_count, runtime.mission.outstanding_count});
  auto const destroyed{[&](target_identity const &target){
    return ((target.layer ? runtime.scene.extra : runtime.scene.base).at(target.index).status & 1) != 0;
  }};
  auto const &actor{runtime.actors.units[*runtime.timing.player]};
  unsigned int required{0};
  unsigned int dead{0};
  for(auto const &waypoint : runtime.mission.tasks[actor.location.task].actors[actor.location.group].waypoints) {
    if(waypoint.type == 4) {
      if(auto target{nearest_ground_object(runtime.scene, waypoint.position[0], waypoint.position[1])}) {
        ++required;
        dead += destroyed(*target);
      }
    }
  }
  result.required_ground(required, dead);
  for(unsigned int kind : {8u, 7u}) {
    for(auto const &object : runtime.scene.objects) {
      if(object.kind == kind) {
        if(auto target{nearest_ground_object(runtime.scene, object.combat.position[0], object.combat.position[1])}) {
          if(kind == 8) {
            result.extra_ground(destroyed(*target));
          } else {
            result.prohibited_ground(destroyed(*target));
          }
        }
      }
    }
  }
  auto const &position{actor.object.world.position};
  auto const x{position[0] >> 22};
  auto const y{position[1] >> 22};
  auto const distance{x >= 0 && y >= 0 && x < 64 && y < 64 ? runtime.scene.front_distance[static_cast<size_t>(x * 64 + y)] : uint8_t{255}};
  result.final_position(runtime.scene.owner(x, y), position[2], runtime.terrain.elevation(position[0], position[1]), distance);
  auto const collision{runtime.pilot->collision_model};
  std::string_view name;
  if(collision && collision < runtime.models.size() && runtime.models[collision]) {
    auto const &model{*runtime.models[collision]};
    name = model.description;
    if(runtime.pilot->death() == 2) {
      // initialise_debrief_ groups wreckage/attachments under their parent name.
      static auto constexpr aliases{[]{
        std::array<uint16_t, 1314> result{};
        for(size_t i{0}; i != result.size(); ++i) result[i] = static_cast<uint16_t>(i);
        result[13] = 0;
        result[144] = 0;
        result[19] = 0;
        result[141] = 0;
        result[25] = 0;
        result[28] = 0;
        result[102] = 0;
        result[103] = 64;
        result[46] = 111;
        result[157] = 156;
        result[158] = 156;
        result[159] = 156;
        result[160] = 156;
        result[161] = 156;
        result[162] = 156;
        result[163] = 156;
        result[164] = 156;
        result[165] = 156;
        result[166] = 156;
        result[167] = 156;
        result[168] = 156;
        result[185] = 156;
        result[186] = 156;
        result[187] = 156;
        result[188] = 156;
        result[351] = 350;
        result[352] = 350;
        result[353] = 350;
        result[354] = 350;
        result[355] = 350;
        result[356] = 350;
        result[357] = 350;
        result[358] = 350;
        result[359] = 350;
        result[360] = 350;
        result[361] = 350;
        result[362] = 350;
        result[363] = 350;
        result[364] = 350;
        result[365] = 350;
        result[366] = 350;
        result[367] = 350;
        result[368] = 350;
        result[369] = 350;
        result[374] = 350;
        result[375] = 350;
        result[376] = 350;
        result[377] = 350;
        result[378] = 350;
        result[379] = 350;
        result[380] = 350;
        result[381] = 350;
        result[382] = 350;
        result[383] = 350;
        result[412] = 408;
        result[38] = 273;
        result[154] = 153;
        result[155] = 153;
        result[264] = 267;
        result[476] = 267;
        result[426] = 216;
        result[257] = 11;
        result[62] = 11;
        result[41] = 11;
        result[195] = 11;
        result[197] = 11;
        return result;
      }()};
      auto const alias{collision < aliases.size() ? aliases[collision] : uint16_t{0}};
      if(alias && alias < runtime.models.size() && runtime.models[alias]) {
        name = runtime.models[alias]->description;
      } else {
        name = model.name;
      }
    }
  }
  result.death(runtime.pilot->death(), collision, position[2], runtime.environment.surface(position[0], position[1]), random, name);
  result.landing(records.worst_landing(), runtime.pilot->death(), random);
  result.pilot_explanation(pilot_name);
  return result;
}
} // namespace apache::simulation
