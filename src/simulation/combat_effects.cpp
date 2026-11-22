#include "combat_effects.h"
#include <algorithm>
#include <stdexcept>

namespace apache::simulation {
namespace {
bool inside(math::vector3 const &p) {
  return p[0] >= 0 && p[0] < 0x10000000 && p[1] >= 0 && p[1] < 0x10000000;
}
int32_t magnitude(int32_t x) {
  return x < 0 ? math::negate(x) : x;
}
int32_t approximate2(int32_t x, int32_t y) {
  x = magnitude(x);
  y = magnitude(y);
  return std::bit_cast<int32_t>(static_cast<uint32_t>(std::max(x, y)) * 4u + static_cast<uint32_t>(std::min(x, y))) >> 2;
}
int32_t approximate3(math::vector3 v) {
  for(auto &x : v) x = magnitude(x);
  std::ranges::sort(v);
  return math::add(math::add(v[2], v[1] >> 2), v[0] >> 2);
}
int32_t remainder(int32_t value, int32_t divisor) {
  // Degenerate model dimensions cause division faults in the original.
  return divisor == 0 ? 0 : static_cast<int32_t>(int64_t{value} % divisor);
}
} // anonymous namespace
combat_effects::combat_effects(resources::model_archive const &definitions, resources::terrain const &ground, math::trigonometry const &angles, effects &sprites, craters &crater_pool, combat_random &rng, std::function<uint8_t(int32_t, int32_t)> materials)
  : models{definitions},
    terrain{ground},
    trig{angles},
    visuals{sprites},
    marks{crater_pool},
    random{rng},
    surface{std::move(materials)} {}
math::vector3 combat_effects::random_point(math::vector3 const &origin, int32_t const radius) {
  math::vector3 result;
  auto const diameter{math::add(radius, radius)};
  for(size_t i{0}; i != 3; ++i) {
    auto const offset{math::subtract(remainder(random.next(), diameter), radius)};
    result[i] = math::add(math::add(origin[i], offset), offset > 0 ? 38400 : -38400);
  }
  result[2] = std::clamp(result[2], 0, INT32_MAX - 1);
  return result;
}
math::vector3 combat_effects::random_point_in_model(math::vector3 const &origin, uint32_t const id, uint32_t const heading) {
  auto const &model{models.at(id).value()};
  uint32_t const angle{static_cast<uint32_t>(random.next())};
  int32_t const radius{math::add(remainder(random.next(), math::subtract(model.radius, 76800)), 76800)};
  int32_t const z{remainder(random.next(), model.height)};
  auto const polar{[&](int32_t r, uint32_t a){
    return math::vector3{math::rounded_q31(0ull - math::product_bits(r, trig.sine(a))), math::multiply_q31(r, trig.cosine(a)), 0};
  }};
  auto point{polar(radius, angle)};
  point[2] = z;
  for(size_t i{0}; i != 3; ++i) point[i] = std::clamp(point[i], model.bounds[i * 2], model.bounds[i * 2 + 1]);
  auto result{polar(approximate2(point[0], point[1]), angle - heading)};
  result[2] = point[2];
  for(size_t i{0}; i != 3; ++i) result[i] = math::add(result[i], origin[i]);
  return result;
}
explosion_audio combat_effects::audio(math::vector3 const &position, int32_t const radius) const {
  math::vector3 offset;
  for(size_t i{0}; i != 3; ++i) offset[i] = math::subtract(position[i], listener[i]);
  int32_t const relative{std::bit_cast<int32_t>(trig.arctangent(offset[0], offset[1]) - listener_heading)};
  int32_t angle{magnitude(relative)};
  if(angle > 0x40000000) angle = math::subtract(INT32_MIN, angle);
  angle = std::min(angle, 0x1f49f4ac);
  int32_t const shift{static_cast<int32_t>(int64_t{angle} * 32768 / 0x20000000)};
  int32_t volume{0};
  if(radius > 0) {
    auto const distance{static_cast<int32_t>(static_cast<double>(approximate3(offset)) / (static_cast<double>(radius) * 1.5))};
    volume = distance <= 2 ? 63 : distance >= 256 ? 10 : std::clamp(64 - distance / 4, 10, 63);
  }
  return {volume, relative < 0 ? math::subtract(32768, shift) : math::add(32768, shift)};
}
void combat_effects::sound(math::vector3 const &position, int32_t const radius) const {
  if(radius && play_sound) play_sound(audio(position, radius));
}
void combat_effects::crater(math::vector3 const &position, int32_t const radius, uint32_t const heading) {
  if(!inside(position)) return;
  auto const material{surface(position[0], position[1])};
  if(material > 7) return;
  if(material >= 1 && material <= 3) {
    marks.add_random(position, 384, true, terrain, random.global_seed);
  } else if(radius == 0) {
    marks.add(position, 62, trig.angles(trig.transformation(heading, 0, 0))[0], true, terrain);
  } else if(material >= 4) {
    marks.add_random(position, 385, false, terrain, random.global_seed);
  } else {
    marks.add_random(position, radius < 19200 ? 11 : 257, false, terrain, random.global_seed);
  }
}
void combat_effects::single(math::vector3 const &position, int32_t const radius, uint32_t const heading) {
  if(position[2] < math::add(terrain.elevation(position[0], position[1]), 5120)) {
    if(radius) visuals.add_ground_explosion(position, radius, terrain);
    crater(position, radius, heading);
  } else if(radius) visuals.add_air_explosion(position, radius);
}
void combat_effects::cluster(math::vector3 const &position, int32_t const radius, uint32_t const id, uint32_t const heading) {
  if(radius < 0 || radius > 0x1000000) throw std::runtime_error{"Invalid explosion cluster radius"};
  single(position, std::min(radius, 38400), heading);
  if(radius < 38400) return;
  int32_t const width{radius / 38400 + 1};
  int32_t const count{width * width};
  int32_t const small{std::min(radius / 2, 28800)};
  for(int32_t i{0}; i != count; ++i) single(id ? random_point_in_model(position, id, heading) : random_point(position, small), small, heading);
}
void combat_effects::ground(resources::scenery_placement &object, math::vector3 const &position, bool const partial) {
  auto const &model{models.at(object.model).value()};
  int32_t const radius{partial ? model.metadata[4] / 4 : model.metadata[4]};
  if(!partial) object.model = static_cast<uint16_t>(std::max(0, model.metadata[5]));
  cluster(position, radius, object.model, object.heading);
  sound(position, radius);
}
void combat_effects::mobile(uint32_t const id, math::pose const &position, bool const partial) {
  auto const &model{models.at(id).value()};
  int32_t const radius{partial ? model.metadata[4] / 4 : model.metadata[4]};
  // create_explosion_ discards the velocity calculated by its callers.
  cluster(position.position, radius, id, trig.arctangent(position.rotation[3], position.rotation[4]));
  sound(position.position, radius);
}
void combat_effects::weapon(unsigned int const id, math::pose const &position, bool const expired, bool const skip_crater, std::optional<math::vector3> const &impact) {
  auto const definition{weapon_definitions().at(id)};
  if(definition.movement_class != 3 && expired) return;
  uint32_t const heading{definition.movement_class == 3 ? 0 : trig.arctangent(position.rotation[3], position.rotation[4])};
  auto const point{impact.value_or(position.position)};
  int32_t const radius{definition.blast_radius};
  if(point[2] < math::add(terrain.elevation(point[0], point[1]), 5120)) {
    if(radius) visuals.add_ground_explosion(point, radius, terrain);
    if(!skip_crater) crater(point, radius / 2, heading);
  } else if(radius) visuals.add_air_explosion(point, radius);
  sound(point, radius);
}
} // namespace apache::simulation
