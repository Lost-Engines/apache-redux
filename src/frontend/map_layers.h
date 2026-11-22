#pragma once
#include "map_view.h"
#include "simulation/mission_scenery.h"

namespace apache::frontend {
struct map_layers {
  bool grid{true};
  bool compass{true};
  bool sides{true};
  bool destroyed{true};
  bool allied_aaa{true};
  bool allied_sam{true};
  bool allied_tanks{false};
  bool enemy_aaa{true};
  bool enemy_sam{true};
  bool enemy_tanks{false};
};
void draw_map_overlay(render::framebuffer &target, map_view const &map, math::trigonometry const &trig, simulation::mission_scenery const &scene, std::span<std::array<int32_t, 2> const> destroyed, map_layers const &options);
} // namespace apache::frontend
