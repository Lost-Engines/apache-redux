#include "player_weapon_mounts.h"
#include <algorithm>
#include <stdexcept>

namespace apache::simulation {
namespace {
math::vector3 difference(math::vector3 const &a, math::vector3 const &b) {
  math::vector3 r;
  for(size_t i{0}; i != 3; ++i) r[i] = math::subtract(a[i], b[i]);
  return r;
}
std::array<int32_t, 2> aim_angles(math::vector3 const &local, math::trigonometry const &trig) {
  auto const horizontal{math::integer_sqrt(math::product_bits(local[0], local[0]) + math::product_bits(local[1], local[1]))};
  auto const length{math::integer_sqrt(uint64_t{horizontal} * horizontal + math::product_bits(local[2], local[2]))};
  auto const angle{[&](int32_t v, uint32_t size)->int32_t {
    if(!size) return 0;
    if(static_cast<int64_t>(v) == size) return 0x40000000;
    auto const ratio{int64_t{v} * 0x80000000ll / size};
    return std::bit_cast<int32_t>(trig.arcsine(static_cast<int32_t>(std::clamp<int64_t>(ratio, INT32_MIN, INT32_MAX))));
  }};
  return {angle(local[0], horizontal), angle(local[2], length)};
}
render::model_instance *child(render::model_instance &object, size_t slot) {
  return slot < object.children.size() ? object.children[slot].get() : nullptr;
}
} // anonymous namespace
std::array<uint32_t, 24> player_pod_models(std::array<player_pylon, 9> const &pylons, resources::model_archive const &models) {
  std::array<uint32_t, 24> result{};
  static std::array<size_t, 7> constexpr slots{1, 4, 2, 5, 3, 6, 0};
  for(size_t i{0}; i != slots.size(); ++i) {
    switch(pylons[i].weapon) {
    case 2:
    case 3:
      result[slots[i]] = 136;
      break;
    case 4:
    case 5:
      result[slots[i]] = 137;
      break;
    case 6:
      result[slots[i]] = 147;
      break;
    default:
      break;
    }
  }
  if(models.at(136) && !models[136]->children.empty()) {
    for(size_t i{23}; i-- > 0;) {
      if(result[i] == 136) {
        for(size_t j{24}; j-- > i + 5;) result[j] = result[j - 4];
        std::fill(result.begin() + static_cast<ptrdiff_t>(i + 1), result.begin() + static_cast<ptrdiff_t>(std::min<size_t>(24, i + 5)), 0);
      }
    }
  }
  return result;
}
void player_weapon_mounts::update_hydra(int32_t const now, int32_t const delta, int32_t const input, math::pose const &body, std::optional<math::vector3> target, int32_t const target_height) {
  hydras_locked = false;
  if(!target) {
    hydra_elevation = math::subtract(hydra_elevation, math::multiply_q31(math::multiply_q31(input, -0xe38e38e), delta));
  } else {
    if(hydra_time == now) return;
    (*target)[2] = math::add((*target)[2], target_height / 2);
    auto const local{math::transform(body.rotation, difference(*target, body.position))};
    hydra_elevation = aim_angles(local, trig)[1];
    hydra_time = now;
  }
  auto const requested{hydra_elevation};
  hydra_elevation = std::clamp(hydra_elevation, -0x71c71c7, 0);
  hydras_locked = requested == hydra_elevation;
}
unsigned int player_weapon_mounts::position_gun(int32_t const now, unsigned int const weapon, render::model_instance &body, std::optional<movement_target> const &target, math::vector3 const &helmet) {
  if(gun_time == now) return gun_status;
  auto *mount{child(body, 0)};
  auto *barrel{mount ? child(*mount, 0) : nullptr};
  if(!mount || !barrel) throw std::runtime_error{"Apache has no chain-gun attachment"};
  math::vector3 aim;
  bool const tracking{weapon == 1 && target.has_value()};
  if(tracking) {
    intercept = target->speed ? intercept_point(body.world.position, 768000, *target, 3) : target->position;
    aim = difference(intercept, barrel->world.position);
  } else {
    aim = helmet;
    for(auto &v : aim) v >>= 16;
  }
  auto const angles{aim_angles(math::transform(body.world.rotation, aim), trig)};
  int32_t const heading{std::clamp(angles[0], -0x471c71c7, 0x471c71c7)};
  int32_t const pitch{std::clamp(angles[1], -0x2aaaaaab, 0x7d27d28)};
  int32_t const sh{trig.sine(static_cast<uint32_t>(heading))};
  int32_t const ch{trig.cosine(static_cast<uint32_t>(heading))};
  int32_t const sp{trig.sine(static_cast<uint32_t>(pitch))};
  int32_t const cp{trig.cosine(static_cast<uint32_t>(pitch))};
  mount->relative.rotation = {ch, math::negate(sh), 0, sh, ch, 0, 0, 0, INT32_MAX};
  barrel->relative.rotation = {INT32_MAX, 0, 0, 0, cp, sp, 0, math::negate(sp), cp};
  gun_status = tracking ? heading == angles[0] && pitch == angles[1] ? 3 : 8 : 1;
  gun_time = now;
  render::maintain_model(body);
  return gun_status;
}
render::model_instance *player_weapon_mounts::pylon(render::model_instance &body, size_t const slot) {
  static std::array<size_t, 6> constexpr slots{2, 6, 3, 7, 4, 8};
  return slot < slots.size() ? child(body, slots[slot]) : nullptr;
}
math::pose player_weapon_mounts::launch_pose(unsigned int const weapon, size_t const slot, int32_t const ammunition, render::model_instance &body) const {
  render::maintain_model(body);
  if(weapon == 1) {
    auto *mount{child(body, 0)};
    auto *barrel{mount ? child(*mount, 0) : nullptr};
    return barrel ? barrel->world : body.world;
  }
  auto *pod{pylon(body, slot)};
  math::pose result{pod ? pod->world : body.world};
  if(weapon == 2 || weapon == 3) {
    if(pod && ammunition > 0) {
      if(auto *round{child(*pod, static_cast<size_t>(ammunition - 1))}) result = round->world;
    }
  } else if(weapon == 4 || weapon == 5) {
    result.position[2] = math::subtract(result.position[2], 256);
    result.rotation = math::multiply(trig.transformation(0, static_cast<uint32_t>(hydra_elevation), 0), result.rotation);
  } else if(weapon == 7 || weapon == 8) {
    result = body.world;
    result.position[2] = math::subtract(result.position[2], 1280);
  }
  return result;
}
void player_weapon_mounts::set_gun_flash(render::model_instance &body, resources::model_archive const &models, bool const active) {
  auto *mount{child(body, 0)};
  auto *barrel{mount ? child(*mount, 0) : nullptr};
  if(!barrel || barrel->children.empty()) return;
  if(!active) {
    barrel->children[0].reset();
  } else if(!barrel->children[0]) {
    auto const &definition{models.at(barrel->id)};
    if(!definition || definition->children.empty() || definition->metadata[5] < 0) return;
    auto const id{static_cast<uint32_t>(definition->metadata[5])};
    if(id >= models.size() || !models[id]) return;
    auto const &attachment{definition->children[0]};
    barrel->children[0] = std::make_unique<render::model_instance>(render::construct_model(models, id, {attachment.translation, attachment.rotation}));
  }
  render::maintain_model(body);
}
void player_weapon_mounts::remove_missile(render::model_instance &body, size_t const slot, int32_t const ammunition) {
  auto *pod{pylon(body, slot)};
  if(pod && pod->id == 136 && ammunition >= 0 && static_cast<size_t>(ammunition) < pod->children.size()) pod->children[static_cast<size_t>(ammunition)].reset();
}
} // namespace apache::simulation
