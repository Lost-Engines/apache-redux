#pragma once
#include "resources/data_store.h"

namespace apache::frontend {
struct presentation_clip {
  std::string path;
  bool movie{false};
  bool title{false};
};
std::vector<presentation_clip> intro_clips(resources::data_store const &data, bool animations, bool skip);
std::string campaign_movie(resources::data_store const &data, int zone, int level);
} // namespace apache::frontend
