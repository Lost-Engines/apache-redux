#pragma once
#include <functional>
#include <optional>
#include "resources/executable.h"

namespace apache::simulation {
struct view_context {
  unsigned int control_mode{2};
  unsigned int game_mode{0};
  bool following{false};
  bool network{false};
  bool serial_connection{false};
  bool drone{false};
  int32_t crash_timer{0};
  unsigned int target_kind{0};
  std::optional<size_t> current_weapon;
  std::optional<size_t> latest_weapon;
  std::function<bool(size_t)> has_target;
  std::function<bool(size_t)> has_lock;
};
struct view_events {
  std::function<void(unsigned int)> enter;
  std::function<void(unsigned int)> leave;
};
class view_modes {
  std::array<unsigned int, 20> crews;
  std::array<unsigned int, 20> inside;
  std::array<unsigned int, 20> moves;
  std::array<unsigned int, 20> left;
  std::array<unsigned int, 20> right;
  std::array<std::array<std::array<int32_t, 4>, 3>, 20> viewports;
public:
  unsigned int mode{21};
  unsigned int home{0};
  unsigned int crew{2};
  unsigned int movement{0};
  bool changed{true};
  bool in_cockpit{true};
  explicit view_modes(resources::executable_data const &executable, unsigned int control_mode = 2);
  bool valid(unsigned int requested, view_context const &context) const;
  bool select(unsigned int requested, view_context const &context, view_events const &events = {});
  unsigned int left_view() const {
    return mode < left.size() ? left[mode] : home;
  }
  unsigned int right_view() const {
    return mode < right.size() ? right[mode] : home;
  }
  std::array<int32_t, 4> viewport(unsigned int video_mode) const;
};
struct optical_magnification {
  int32_t distance{0};
  int32_t rate{0};
  std::string label;
};
class optical_devices {
  std::array<std::string, 3> names;
  std::array<std::vector<optical_magnification>, 3> levels;
public:
  unsigned int device{0};
  std::array<unsigned int, 3> selected{};
  explicit optical_devices(resources::executable_data const &executable);
  bool select(unsigned int id, uint32_t faults);
  bool cycle(unsigned int view, uint32_t faults);
  void check(uint32_t faults);
  void zoom(unsigned int view, bool raise);
  optical_magnification const &magnification() const {
    return levels.at(device).at(selected.at(device));
  }
  std::string const &name() const {
    return names.at(device);
  }
};
} // namespace apache::simulation
