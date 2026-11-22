#include "inspect_mission_scenery.h"
#include <ostream>
#include "simulation/mission_scenery.h"

void inspect_mission_scenery(std::ostream &output, apache::resources::data_store const &data, std::string const &zone, std::string const &path) {
  namespace resources = apache::resources;
  auto const bytes{data.read(path)};
  auto const mission{resources::read_mission_definition(std::string{bytes.begin(), bytes.end()})};
  resources::terrain const terrain{data.read(zone + ".WZ/GRIDPOST.BIN")};
  auto models{resources::read_models(data.read("3DDATA/3DOBJS.BIN"))};
  resources::executable_data const executable{data};
  resources::initialise_model_metadata(models, executable);
  unsigned int const map{zone == "YEMEN" ? 1u : zone == "KOREA" ? 2u : zone == "CYPRUS" ? 3u : zone == "FORT" ? 4u : 0u};
  apache::simulation::mission_scenery const scene{mission, resources::read_scenery(data.read(zone + ".WZ/GNDOBJS.BIN")), terrain, models, map};
  output << "sectors";
  for(size_t i{0}; i != scene.sectors.size(); ++i) {
    output << ' ' << scene.sectors[i].side << ' ' << static_cast<unsigned int>(scene.front_distance[i]);
    for(auto id : scene.sectors[i].objects) output << ' ' << (id ? static_cast<int64_t>(*id) : -1);
  }
  output << '\n';
  for(auto const &item : scene.objects) {
    output << "object " << item.kind;
    for(auto v : item.combat.position) output << ' ' << v;
    output << ' ' << item.combat.heading << ' ' << item.combat.deadline << ' ' << item.combat.shots << ' ' << (item.scenery ? static_cast<int64_t>(*item.scenery) : -1) << '\n';
  }
  for(size_t layer{0}; layer != 2; ++layer) {
    for(auto const &p : layer ? scene.extra : scene.base) output << "scenery " << layer << ' ' << p.sector << ' ' << p.model << ' ' << p.x << ' ' << p.y << ' ' << p.heading << ' ' << p.animation << ' ' << p.status << '\n';
  }
}
