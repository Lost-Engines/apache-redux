#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include "configuration/profiles.h"

namespace {
void require(bool value, char const *message) {
  if(!value) throw std::runtime_error{message};
}
struct temporary_directory {
  std::filesystem::path path{std::filesystem::temp_directory_path() / ("apache-profile-tests-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))};
  temporary_directory() {
    if(!std::filesystem::create_directory(path)) throw std::runtime_error{"Cannot create test directory"};
  }
  ~temporary_directory() {
    std::error_code error;
    std::filesystem::remove_all(path, error);
  }
};
}
int main() try {
  temporary_directory directory;
  apache::configuration::profiles book;
  book.load(directory.path, 1000);
  require(book.current().name == "de Fault" && book.current().protected_profile && book.current().rank == 6, "Default pilot missing");
  require(!book.erase(), "Default pilot may not be deleted");
  require(book.create(1000), "Pilot creation failed");
  book.current().name = "Test Pilot";
  book.current().total_time = 12345;
  book.current().missions.push_back({81, apache::simulation::mission_result::success});
  book.current().campaign = {1, 3, 2, 1234, {{3, apache::simulation::mission_result::failure}}};
  auto const expected{book.current().encode()};
  book.users[0].name = "Flight Team";
  book.save();
  apache::configuration::profiles restored;
  restored.load(directory.path, 1000);
  require(restored.current().encode() == expected, "Pilot state changed on disk");
  require(restored.users[0].name == "Flight Team" && restored.users[0].selected == 1, "Pilot group selection changed");
  for(int i{0}; i != 8; ++i) require(restored.create(1000), "Available pilot slot rejected");
  require(!restored.create(1000), "More than ten pilots allowed");
  restored.users[0].selected = 1;
  require(restored.erase(), "Pilot deletion failed");
  restored.save();
  require(!std::filesystem::exists(directory.path / "log/log0.2"), "Deleted pilot still exists on disk");
  require(restored.create(1000) && restored.users[0].pilots.back().file == 2, "Deleted file number was not reused");
  restored.current().name = "Replacement";
  restored.save();
  restored.select_user(4, 1000);
  require(restored.current().protected_profile, "New pilot group has no default");
  require(restored.create(1000), "Second pilot group creation failed");
  restored.current().name = "Second Team";
  restored.save();
  book.load(directory.path, 1000);
  require(book.user == 4 && book.current().name == "Second Team", "Current pilot group not restored");
  book.select_user(0, 1000);
  require(book.current().name == "Replacement", "Previous group's selected pilot lost");
  book.current().name = std::string(64, 'x');
  bool rejected{false};
  try {
    book.save();
  } catch(std::runtime_error const &) {
    rejected = true;
  }
  require(rejected, "Invalid pilot was saved");
  restored.load(directory.path, 1000);
  restored.select_user(0, 1000);
  require(restored.current().name == "Replacement", "Rejected save damaged existing pilot");
  auto const old{restored.current().encode()};
  {
    std::ofstream out{directory.path / "log/current.log"};
    out << "100";
  }
  rejected = false;
  try {
    restored.load(directory.path, 1000);
  } catch(std::runtime_error const &) {
    rejected = true;
  }
  require(rejected && restored.current().encode() == old, "Failed load replaced in-memory pilots");
  std::cout << "Pilot storage checks passed\n";
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << error.what() << '\n';
  return EXIT_FAILURE;
}
