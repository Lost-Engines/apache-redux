#pragma once
#include "math/trigonometry.h"
#include "cockpit.h"
#include "instrument_font.h"
#include "projection2.h"

namespace apache::render {
class panel_instruments {
  cockpit const &panels;
  math::trigonometry const &trig;
  instrument_font font;
  bool high;
  projection2 dial(framebuffer &target, int x, int y, size_t source_offset) const;
  void hand(framebuffer &target, projection2 &plane, uint32_t angle, int32_t length, uint8_t colour) const;
  void shade(framebuffer &target, std::string_view name, int x, int y, uint8_t base, uint8_t excluded) const;
public:
  panel_instruments(cockpit const &panels, math::trigonometry const &trig, resources::executable_data const &executable);
  void airspeed(framebuffer &target, int32_t speed) const;
  void altimeter(framebuffer &target, int32_t altitude) const;
  void clock(framebuffer &target, int32_t seconds) const;
  void compass(framebuffer &target, uint32_t heading) const;
  void warnings(framebuffer &target, bool gunner, uint32_t faults) const;
  void communications(framebuffer &target, bool gunner, std::array<std::string, 3> lines) const;
  void accelerated_time(framebuffer &target, unsigned int factor) const;
  void attitude(framebuffer &target, math::matrix3 const &rotation) const;
};
} // namespace apache::render
