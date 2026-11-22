#include "presentation.h"
#include <array>

namespace apache::frontend {
std::vector<presentation_clip> intro_clips(resources::data_store const &data, bool const animations, bool const skip) {
  std::vector<presentation_clip> result;
  bool di{false}, imagic{false};
  auto const add{[&](std::string const &path, bool movie, bool title = false){
    if(!data.contains(path)) return false;
    result.push_back({path, movie, title}); return true;
  }};
  if(animations && !skip) {
    di = add("ANIMS/DI.ANM", true); imagic = add("ANIMS/IMAGIC.ANM", true); add("ANIMS/INTRO.ANM", true);
  }
  if(!skip) {
    if(!di) add("MENUSCRN/DI.LBM", false);
    if(!imagic) add("MENUSCRN/IM.LBM", false);
  }
  add("MENUSCRN/TITLE.LBM", false, true);
  return result;
}
std::string campaign_movie(resources::data_store const &data, int const zone, int const level) {
  // play_campaign_animation_, original tables at 0x4da450, 0x4da470, 0x4da490.
  if(zone < 1 || zone > 3 || level < 1 || level > 8) return {};
  auto const path{"ANIMS/" + std::string{std::array<std::string_view, 3>{"YEMEN", "KOREA", "CYPRUS"}[static_cast<size_t>(zone - 1)]} + std::to_string(level) + ".Q3"};
  return data.contains(path) ? path : std::string{};
}
} // namespace apache::frontend
