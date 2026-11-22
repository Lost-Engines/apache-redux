#include "mission_spatial.h"
#include <stdexcept>

namespace apache::simulation {
mission_spatial::mission_spatial(resources::mission_definition const &definition, mission_scenery const &scenery, mission_actors &units, render::mobile_scene &scene, resources::terrain const &terrain) : mission{definition}, actors{units}, mobiles{scene} {
  if(mobiles.on_place || mobiles.on_remove) throw std::runtime_error{"The mobile scene already has a contact registry"};
  for(size_t i{0}; i != actors.units.size(); ++i) {
    uint32_t const id{mobiles.register_object(actors.units[i].object, false)}; actor_indices.emplace(id, i);
    targets.push_back(std::make_shared<target_reference>(target_reference{{target_kind::mobile, 0, id}, {}, 0}));
  }
  initialise_mission_radars(grid, scenery, actors, targets, terrain);
  mobiles.on_place = [&](uint32_t id, size_t sector){ placed(id, sector); };
  mobiles.on_remove = [&](uint32_t id, size_t sector){ removed(id, sector); };
  for(auto const &group : actors.groups) {
    size_t const leader{group.units.front()}; if(!(actors.units[leader].conditions & 0x400)) place(leader);
    for(size_t i{group.units.size()}; i-- > 1;) place(group.units[i]);
  }
}
mission_spatial::~mission_spatial() {
  mobiles.on_place = {}; mobiles.on_remove = {};
  for(auto const &target : targets) if(mobiles.contains(target->identity.index)) mobiles.remove(target->identity.index);
}
std::optional<size_t> mission_spatial::actor_index(uint32_t const id) const {
  auto const found{actor_indices.find(id)}; return found == actor_indices.end() ? std::nullopt : std::optional{found->second};
}
war_contact mission_spatial::contact(size_t const index) const {
  auto const &actor{actors.units.at(index)};
  return {targets[index], actor.object.status, static_cast<int32_t>(actor.classification), mission.tasks.at(actor.location.task).type, player == index};
}
void mission_spatial::placed(uint32_t const id, size_t const sector) {
  if(auto const index{actor_index(id)}; index && !(actors.units[*index].object.status & 1)) grid.insert(static_cast<int32_t>(sector / 64), static_cast<int32_t>(sector % 64), contact(*index));
}
void mission_spatial::removed(uint32_t const id, size_t const sector) {
  auto const index{actor_index(id)}; if(!index) return;
  grid.remove(static_cast<int32_t>(sector / 64), static_cast<int32_t>(sector % 64), contact(*index), actors.units[*index].object.world.position, [&](int32_t x, int32_t y){
    std::vector<war_contact> found; if(x < 0 || y < 0 || x >= 64 || y >= 64) return found;
    for(auto const item : mobiles.sector(static_cast<size_t>(x * 64 + y))) if(auto const actor{actor_index(item)}) found.push_back(contact(*actor));
    return found;
  });
}
void mission_spatial::move(actor_state &actor, math::vector3 const &position) {
  size_t const index{static_cast<size_t>(&actor - actors.units.data())}; auto pose{actor.object.world}; pose.position = position; mobiles.move(mobile_id(index), pose);
}
void mission_spatial::change_status(size_t const index, uint32_t const status) {
  auto &unit{actors.units.at(index)}; auto const &position{unit.object.world.position};
  bool const inside{position[0] >= 0 && position[0] < 0x10000000 && position[1] >= 0 && position[1] < 0x10000000};
  size_t const sector{inside ? static_cast<size_t>((position[0] >> 22) * 64 + (position[1] >> 22)) : 0};
  if(inside) removed(mobile_id(index), sector);
  unit.object.status = status;
  if(inside) placed(mobile_id(index), sector);
}
void mission_spatial::place(size_t const actor) {
  mobiles.place(mobile_id(actor));
}
void mission_spatial::unplace(size_t const actor) {
  mobiles.unplace(mobile_id(actor));
}
} // namespace apache::simulation
