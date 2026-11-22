#include "actor_types.h"
#include <initializer_list>

namespace apache::simulation {
std::array<actor_type, 1315> actor_types() {
  // initialise_drone_headers_, dinfo.c (0x429b00). The original setters and
  // like_base_/like_weapons_ copies resolve to these shared classifications
  // and loadouts; model geometry remains in the selected game data.
  std::array<actor_type, 1315> result;
  auto const base{[&](std::initializer_list<unsigned int> ids, uint32_t flags, uint32_t classification, uint32_t aircraft, uint32_t ship){
    for(auto const id : ids) {
      auto &type{result[id]};
      type.flags = flags;
      type.classification = classification;
      type.aircraft = aircraft;
      type.ship = ship;
    }
  }};
  base({3}, 1, 2, 1, 6);
  base({34}, 3, 2, 0, 6);
  base({54, 71, 77, 84, 87, 99, 201, 203, 208, 211, 219, 256, 398, 450, 460, 464, 469, 496, 526, 532}, 3, 4, 4, 6);
  base({64, 103, 307, 607, 686}, 1, 1, 3, 6);
  base({68, 244}, 1, 3, 4, 6);
  base({74, 204, 205, 299}, 3, 5, 4, 6);
  base({80, 96, 216, 422, 426}, 1, 4, 4, 6);
  base({90, 93, 240, 456}, 3, 3, 4, 6);
  base({130}, 1, 7, 4, 1);
  base({206, 207}, 1, 5, 4, 6);
  base({214, 215, 257, 447, 472, 473, 994, 995}, 1, 6, 4, 6);
  base({222, 507, 990}, 3, 1, 3, 6);
  base({225, 231, 234, 453, 504, 577}, 1, 2, 2, 6);
  base({228}, 3, 2, 2, 6);
  base({237}, 3, 2, 1, 6);
  base({253, 419, 484, 571}, 1, 7, 4, 2);
  base({316, 514}, 1, 7, 4, 5);
  base({320, 489}, 1, 7, 4, 3);
  base({347, 401, 516, 565, 1014, 1016}, 1, 7, 4, 4);
  base({416}, 3, 7, 4, 2);
  base({523, 556, 562}, 1, 8, 4, 6);
  base({559}, 3, 8, 4, 6);
  base({574}, 1, 7, 4, 0);
  auto const arm{[&](std::initializer_list<unsigned int> ids, std::array<int32_t, 5> weapons, std::array<int32_t, 5> ammunition, uint32_t main){
    for(auto const id : ids) {
      auto &type{result[id]};
      type.weapons = weapons;
      type.ammunition = ammunition;
      type.main_weapon = main;
    }
  }};
  arm({3}, {30, 28, 0, 0, 0}, {INT32_MAX, 4, 0, 0, 0}, 0);
  arm({54}, {0, 31, 23, 0, 0}, {0, 5, 500, 0, 0}, 1);
  arm({64}, {1, 3, 6, 0, 0}, {1200, 16, 4, 0, 0}, 0);
  arm({68, 90, 93, 240, 244, 456}, {0, 20, 0, 0, 0}, {0, INT32_MAX, 0, 0, 0}, 1);
  arm({71}, {0, 26, 0, 0, 0}, {0, 1, 0, 0, 0}, 1);
  arm({74}, {0, 21, 0, 0, 0}, {0, 12, 0, 0, 0}, 1);
  arm({80}, {0, 0, 37, 0, 0}, {0, 0, INT32_MAX, 0, 0}, 2);
  arm({84}, {0, 0, 39, 0, 0}, {0, 0, INT32_MAX, 0, 0}, 2);
  arm({87}, {0, 0, 40, 0, 0}, {0, 0, INT32_MAX, 0, 0}, 2);
  arm({96}, {0, 31, 0, 0, 0}, {0, 4, 0, 0, 0}, 1);
  arm({99, 130, 211, 450}, {0, 0, 32, 0, 0}, {0, 0, INT32_MAX, 0, 0}, 2);
  arm({201}, {0, 0, 52, 0, 0}, {0, 0, 5, 0, 0}, 2);
  arm({203}, {0, 0, 34, 0, 0}, {0, 0, INT32_MAX, 0, 0}, 2);
  arm({204, 205}, {0, 41, 0, 0, 0}, {0, INT32_MAX, 0, 0, 0}, 1);
  arm({206}, {0, 53, 0, 0, 0}, {0, INT32_MAX, 0, 0, 0}, 1);
  arm({207}, {0, 54, 0, 0, 0}, {0, 1, 0, 0, 0}, 1);
  arm({208}, {0, 0, 23, 52, 0}, {0, 0, INT32_MAX, 5, 0}, 2);
  arm({222, 607}, {30, 4, 48, 5, 0}, {1200, 128, 4000, 128, 0}, 0);
  arm({225, 577}, {30, 28, 42, 0, 0}, {512, 4, 4, 0, 0}, 0);
  arm({228}, {30, 28, 46, 0, 0}, {1200, 4, 4, 0, 0}, 0);
  arm({231, 453}, {30, 28, 42, 43, 0}, {675, 4, 4, 4, 0}, 0);
  arm({234}, {0, 0, 45, 44, 0}, {0, 0, 4, 4, 0}, 0);
  arm({237}, {30, 0, 45, 0, 0}, {250, 0, 2, 0, 0}, 0);
  arm({256}, {0, 31, 0, 0, 0}, {0, 11, 0, 0, 0}, 1);
  arm({299}, {47, 0, 0, 0, 0}, {INT32_MAX, 0, 0, 0, 0}, 0);
  arm({307}, {0, 0, 48, 0, 0}, {INT32_MAX, 0, 0, 0, 0}, 2);
  arm({316}, {0, 49, 23, 38, 0}, {0, 4, INT32_MAX, 4, 0}, 1);
  arm({320, 489, 514}, {23, 51, 50, 0, 0}, {INT32_MAX, 4, 36, 0, 0}, 0);
  arm({469}, {0, 0, 38, 0, 0}, {0, 0, INT32_MAX, 0, 0}, 2);
  arm({507}, {1, 3, 6, 0, 0}, {600, 16, 4, 0, 0}, 0);
  arm({686}, {22, 0, 0, 0, 0}, {40, 0, 0, 0, 0}, 0);
  arm({990}, {19, 18, 10, 46, 0}, {1470, 4, 500, 4, 0}, 0);
  return result;
}
} // namespace apache::simulation
