#pragma once
#include "draw_queue.h"
#include "horizon.h"
#include "sector_scan.h"
#include "resources/terrain.h"

namespace apache::render {
sector_bounds scan_cloud_sectors(math::pose const &camera, math::trigonometry const &trig, uint32_t heading, uint32_t pitch, unsigned int cloud_mode, bool expanded_frustum = false);
unsigned int cloud_patch_type(int x, int y);
struct cloud_vertex {
  math::vector3 eye{0, 0, 0};
  int32_t intensity{0};
};
class cloud_mesh {
  struct patch {
    int x{0};
    int y{0};
    unsigned int type{0};
  };
  sector_bounds sectors;
  unsigned int mode{0};
  std::vector<patch> patches;
  size_t index(int x, int y) const;
public:
  int left{0};
  int bottom{0};
  unsigned int columns{0};
  unsigned int rows{0};
  std::vector<cloud_vertex> vertices;
  cloud_mesh(resources::terrain const &terrain, math::pose const &camera, sector_bounds bounds, int height, unsigned int cloud_mode, unsigned int weather);
  void submit(draw_queue &queue, math::pose const &camera);
  void draw(rasterizer &raster, projection const &view, scene_palette const &palette, uint32_t payload) const;
};
} // namespace apache::render
