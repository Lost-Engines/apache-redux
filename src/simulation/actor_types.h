#pragma once
#include <array>
#include <cstdint>

namespace apache::simulation {
struct actor_type {
  uint32_t flags{0};
  uint32_t classification{0};
  uint32_t aircraft{4};
  uint32_t ship{6};
  std::array<int32_t, 5> weapons{};
  std::array<int32_t, 5> ammunition{};
  uint32_t main_weapon{0};
};
std::array<actor_type, 1315> actor_types();
} // namespace apache::simulation
