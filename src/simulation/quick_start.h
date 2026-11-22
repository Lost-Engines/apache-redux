#pragma once
#include "resources/models.h"

namespace apache::simulation {
inline int32_t quick_start_points(resources::model const &object, int const difficulty) {
  int32_t base{100};
  if(object.metadata[7] & 1) {
    switch(object.metadata[0]) {
    case 10000:
      base = 1000;
      break;
    case 30000:
      base = 2500;
      break;
    case 50000:
      base = 5000;
      break;
    default:
      base = 0;
      break;
    }
  }
  return static_cast<int32_t>(static_cast<int64_t>(base) * (difficulty + 6) / 15) + 1;
}
} // namespace apache::simulation
