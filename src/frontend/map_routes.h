#pragma once
#include "map_view.h"
#include "render/instrument_font.h"
#include "resources/missions.h"

namespace apache::frontend {
struct map_route_command {
  uint32_t kind{0};
  int32_t x{0};
  int32_t y{0};
};
std::vector<map_route_command> map_route_commands(resources::mission_task const &task, size_t selected, bool others);
void draw_map_routes(render::framebuffer &target, map_view const &map, std::span<map_route_command const> commands, math::trigonometry const &trig, render::instrument_font const &font, bool primary, bool others);
} // namespace apache::frontend
