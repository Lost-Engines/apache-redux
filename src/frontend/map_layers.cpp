#include "map_layers.h"

namespace apache::frontend {
void draw_map_overlay(render::framebuffer &target, map_view const &map, math::trigonometry const &trig, simulation::mission_scenery const &scene, std::span<std::array<int32_t, 2> const> const destroyed, map_layers const &options) {
  auto view{map.projection(trig)};
  render::rasterizer raster{target};
  auto const line{[&](render::vertex2 const &a, render::vertex2 const &b, uint8_t colour){ if(auto clipped{view.clip_line(a, b)}) raster.line((*clipped)[0], (*clipped)[1], colour); }};
  auto const circle{[&](render::vertex2 const &centre, int32_t radius, uint8_t colour){ auto const p{view.project(centre)}; raster.circle_outline(p.x, p.y, view.radius(radius), colour); }};
  if(options.grid) for(int32_t const spacing : {0xcd0d7, 0x802866}) {
    if(map.position.zoom >= (spacing == 0xcd0d7 ? 0xa40a : 0x271000)) continue;
    for(int32_t y{0}; y < 0x10000000; y += spacing) line({0, y}, {0xfffffff, y}, 203);
    for(int32_t x{0}; x < 0x10000000; x += spacing) line({x, 0}, {x, 0xfffffff}, 203);
  }
  if(options.compass) {
    render::projection2 compass{{480, 320, 639, 479}, {-0xc800, -0xc800, 0xc800, 0xc800}};
    compass.sine = trig.sine(map.position.rotation); compass.cosine = trig.cosine(map.position.rotation);
    auto const axis{[&](render::vertex2 const &a, render::vertex2 const &b){ if(auto clipped{compass.clip_line(a, b)}) raster.line((*clipped)[0], (*clipped)[1], 251); }};
    axis({0, -0x3200}, {0, 0xc800});
    auto const p{compass.project({0, 0})}; raster.circle_outline(p.x, p.y, compass.radius(0x2800), 251);
    axis({-0x3200, 0}, {0x3200, 0});
  }
  auto const territory{[&](int x, int y, int side){
    size_t const index{static_cast<size_t>(x * 64 + y)};
    if(!options.sides || scene.sectors[index].side != side) return;
    view.origin = {x * 0x400000 + 0x200000, y * 0x400000 + 0x200000};
    int const radius{view.radius((static_cast<int32_t>(scene.front_distance[index]) + 1) * 0x1f4000)};
    for(auto const &offset : std::array<render::vertex2, 4>{{{0x100000, 0x100000}, {0x100000, -0x100000}, {-0x100000, 0x100000}, {-0x100000, -0x100000}}}) {
      auto const p{view.project(offset)};
      raster.circle(p.x, p.y, radius, {.colours{static_cast<uint32_t>(204 + side) * 0x01010101u}, .pattern{render::fill_pattern::coarse}, .remapping{}, .intensity_map{}});
    }
    view.origin = {0, 0};
  }};
  for(int y{0}; y != 64; ++y) for(int x{0}; x != 64; ++x) territory(x, y, 1);
  for(int y{0}; y != 64; ++y) for(int x{0}; x != 64; ++x) {
    territory(x, y, 2);
    auto const &sector{scene.sectors[static_cast<size_t>(x * 64 + y)]};
    if(sector.side != 1 && sector.side != 2) continue;
    uint8_t const colour{static_cast<uint8_t>(204 + sector.side)};
    for(auto const &index : sector.objects) if(index) {
      auto const &object{scene.objects.at(*index)}; auto const kind{object.kind}; auto const &p{object.combat.position};
      if(kind == 1 || kind == 2) {
        bool const enabled{sector.side == 1 ? (kind == 1 ? options.allied_aaa : options.allied_sam) : (kind == 1 ? options.enemy_aaa : options.enemy_sam)};
        if(enabled) circle({p[0], p[1]}, kind == 1 ? 0x2f7800 : 0x76ac00, colour);
      } else {
        if(kind == 3 || kind == 4) { if(!(sector.side == 1 ? options.allied_tanks : options.enemy_tanks)) continue; } else if(kind < 5 || kind > 8) continue;
        int const dx{kind == 3 || kind == 4 ? 0x3200 : kind == 5 ? 0x3e800 : 0xc800};
        int const dy{kind == 3 || kind == 4 ? 0x1900 : dx};
        render::vertex2 const a{p[0] - dx, p[1] - dy}, b{p[0] + dx, p[1] - dy}, c{p[0] + dx, p[1] + dy}, d{p[0] - dx, p[1] + dy};
        if(kind != 7) { line(a, b, colour); line(d, c, colour); }
        if(kind < 7) { line(a, d, colour); line(b, c, colour); }
        if(kind >= 5) { line(a, c, colour); line(b, d, colour); }
      }
    }
  }
  if(options.destroyed) for(auto const &p : destroyed) circle({p[0], p[1]}, 10000, 1);
}
} // namespace apache::frontend
