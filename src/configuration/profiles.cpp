#include "profiles.h"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace apache::configuration {
namespace {
resources::bytes read(std::filesystem::path const &path) {
  std::ifstream input{path, std::ios::binary};
  if(!input) throw std::runtime_error{"Cannot read pilot file: " + path.string()};
  auto const length{std::filesystem::file_size(path)};
  if(length > 65536) throw std::runtime_error{"Pilot file is too large: " + path.string()};
  resources::bytes result(static_cast<size_t>(length));
  if(!input.read(reinterpret_cast<char*>(result.data()), static_cast<std::streamsize>(result.size()))) throw std::runtime_error{"Cannot read pilot file: " + path.string()};
  return result;
}
void write(std::filesystem::path const &path, resources::bytes const &data) {
  auto temporary{path}; temporary += ".tmp";
  {
    std::ofstream output{temporary, std::ios::binary | std::ios::trunc};
    if(!output || !output.write(reinterpret_cast<char const*>(data.data()), static_cast<std::streamsize>(data.size())) || !output.flush()) throw std::runtime_error{"Cannot save pilot file: " + path.string()};
  }
  std::filesystem::rename(temporary, path);
}
resources::bytes text(std::string const &value) {
  resources::bytes result{value.begin(), value.end()}; result.push_back(0); return result;
}
}
std::filesystem::path default_save_directory() {
#ifdef _WIN32
  if(auto const *path{std::getenv("LOCALAPPDATA")}; path && *path) return std::filesystem::path{path} / "apache-redux";
#endif
  if(auto const *path{std::getenv("XDG_DATA_HOME")}; path && *path && std::filesystem::path{path}.is_absolute()) return std::filesystem::path{path} / "apache-redux";
  if(auto const *path{std::getenv("HOME")}; path && *path) return std::filesystem::path{path} / ".local/share/apache-redux";
  return "apache-redux-saves";
}
profiles::profiles(uint32_t const now) {
  for(size_t i{0}; i != users.size(); ++i) users[i].name = "User " + std::to_string(i + 1);
  ensure_pilot(now);
}
void profiles::ensure_pilot(uint32_t const now) {
  auto &profile{users.at(user)};
  if(std::ranges::none_of(profile.pilots, [](auto const &slot){ return slot.log.protected_profile != 0; })) {
    if(profile.pilots.size() == 10) throw std::runtime_error{"Pilot group is full and has no default pilot"};
    create(now); auto &pilot{current()}; pilot.name = "de Fault"; pilot.rank = 6; pilot.protected_profile = 1;
  }
  if(profile.selected >= profile.pilots.size()) profile.selected = 0;
}
simulation::pilot_log &profiles::current() { return users.at(user).pilots.at(users.at(user).selected).log; }
simulation::pilot_log const &profiles::current() const { return users.at(user).pilots.at(users.at(user).selected).log; }
void profiles::select_user(size_t const index, uint32_t const now) {
  if(index >= users.size()) throw std::runtime_error{"Invalid pilot group"};
  user = index; ensure_pilot(now);
  for(auto &slot : users[user].pilots) slot.log.recover(now);
}
bool profiles::create(uint32_t const now) {
  auto &profile{users.at(user)};
  if(profile.pilots.size() >= 10) return false;
  unsigned int file{1};
  while(std::ranges::any_of(profile.pilots, [&](auto const &slot){ return slot.file == file; })) ++file;
  simulation::pilot_log pilot; pilot.rank = 2; pilot.status_hour = now;
  profile.pilots.push_back({file, std::move(pilot)}); profile.selected = profile.pilots.size() - 1; return true;
}
bool profiles::erase() {
  if(current().protected_profile) return false;
  auto &profile{users[user]}; profile.pilots.erase(profile.pilots.begin() + static_cast<ptrdiff_t>(profile.selected)); profile.selected = 0; return true;
}
void profiles::load(std::filesystem::path const &path, uint32_t const now) {
  profiles loaded{now}; loaded.directory = path;
  auto const root{path / "log"};
  if(std::filesystem::exists(root / "current.log")) {
    auto const bytes{read(root / "current.log")}; std::istringstream input{std::string{bytes.begin(), bytes.end()}};
    if(!(input >> loaded.user) || loaded.user >= loaded.users.size()) throw std::runtime_error{"Invalid current pilot group"};
  }
  for(size_t i{0}; i != loaded.users.size(); ++i) {
    auto &profile{loaded.users[i]}; profile.pilots.clear(); unsigned int selected{0};
    auto const identity{root / ("user." + std::to_string(i))};
    if(std::filesystem::exists(identity)) {
      auto const bytes{read(identity)}; std::string source{bytes.begin(), bytes.end()}; source.resize(source.find('\0') == std::string::npos ? source.size() : source.find('\0'));
      std::istringstream input{source};
      if(!(input >> selected) || selected > 10) throw std::runtime_error{"Invalid pilot selection"};
      std::getline(input >> std::ws, profile.name); if(profile.name.size() > 15) throw std::runtime_error{"Pilot group name is too long"};
    }
    bool protected_seen{false};
    for(unsigned int slot{1}; slot <= 10; ++slot) {
      auto const file{root / ("log" + std::to_string(i) + "." + std::to_string(slot))};
      if(!std::filesystem::exists(file)) continue;
      auto pilot{simulation::pilot_log::decode(read(file))};
      if(pilot.rank > 6 || pilot.status > 9 || pilot.campaign.warzone > 3 || pilot.campaign.level > 9 || pilot.campaign.lives > 4) throw std::runtime_error{"Invalid pilot rank, status or campaign"};
      if(pilot.protected_profile && protected_seen) continue;
      protected_seen |= pilot.protected_profile != 0; pilot.recover(now);
      if(slot == selected) profile.selected = profile.pilots.size();
      profile.pilots.push_back({slot, std::move(pilot)});
    }
  }
  loaded.ensure_pilot(now); *this = std::move(loaded);
}
void profiles::save() const {
  if(directory.empty()) return;
  auto const root{directory / "log"};
  std::vector<std::pair<std::filesystem::path, resources::bytes>> files;
  std::vector<std::filesystem::path> removed;
  for(size_t i{0}; i != users.size(); ++i) {
    auto const &profile{users[i]}; if(profile.pilots.empty()) continue;
    if(profile.name.size() > 15 || profile.name.find('\0') != std::string::npos || profile.name.find('\n') != std::string::npos) throw std::runtime_error{"Invalid pilot group name"};
    auto const selected{profile.pilots.at(profile.selected).file};
    for(unsigned int slot{1}; slot <= 10; ++slot) {
      auto const path{root / ("log" + std::to_string(i) + "." + std::to_string(slot))};
      auto const found{std::ranges::find(profile.pilots, slot, &pilot_slot::file)};
      if(found == profile.pilots.end()) removed.push_back(path);
      else files.emplace_back(path, found->log.encode());
    }
    files.emplace_back(root / ("user." + std::to_string(i)), text(std::to_string(selected) + " " + profile.name));
  }
  files.emplace_back(root / "current.log", text(std::to_string(user)));
  std::filesystem::create_directories(root);
  for(auto const &[path, bytes] : files) write(path, bytes);
  for(auto const &path : removed) std::filesystem::remove(path);
}
} // namespace apache::configuration
