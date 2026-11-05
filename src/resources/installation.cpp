#include "installation.h"
#include "data_store.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <stdexcept>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace apache::resources {
namespace {
struct entry { char const *name; uint32_t sector, size; uint64_t hash; };
constexpr entry files[]{
#include "reference_files.inc"
};
bool looks_installed(std::filesystem::path const &directory) {
  std::error_code error;
  for(auto const *path : {"3DDATA/3DOBJS.BIN", "3ddata/3dobjs.bin"})
    if(std::filesystem::is_regular_file(directory / path, error)) return true;
  return false;
}
std::string resource_name(data_store const &data, entry const &file) {
  if(std::string_view{file.name} == "APACHE95.EXE" && data.contains("SOFTWARE/APACHE95.EXE")) return "SOFTWARE/APACHE95.EXE";
  return file.name;
}
bytes read_file(std::filesystem::path const &path) {
  std::ifstream input{path, std::ios::binary};
  auto const size{std::filesystem::file_size(path)};
  if(size > 256 * 1024 * 1024) throw std::runtime_error{"File is too large: " + path.string()};
  bytes result(static_cast<size_t>(size));
  if(!input.read(reinterpret_cast<char*>(result.data()), static_cast<std::streamsize>(size))) throw std::runtime_error{"Cannot read " + path.string()};
  return result;
}
std::filesystem::path checked_destination(std::filesystem::path const &directory, std::filesystem::path const &path) {
  auto const relative{std::filesystem::weakly_canonical(path).lexically_relative(std::filesystem::weakly_canonical(directory))};
  if(relative.empty() || *relative.begin() == "..") throw std::runtime_error{"Game file leaves the selected directory: " + path.string()};
  return path;
}
}
std::filesystem::path default_game_directory() {
#ifdef _WIN32
  std::wstring filename(32768, L'\0');
  auto const size{GetModuleFileNameW(nullptr, filename.data(), static_cast<DWORD>(filename.size()))};
  if(!size || size == filename.size()) throw std::runtime_error{"Cannot locate the Apache executable directory"};
  filename.resize(size);
  return std::filesystem::path{filename}.parent_path() / "data";
#else
  // Share original game files with Apache C++, independently of Redux saves.
  if(auto const *path{std::getenv("XDG_DATA_HOME")}; path && *path && std::filesystem::path{path}.is_absolute()) return std::filesystem::path{path} / "apache/game";
  if(auto const *path{std::getenv("HOME")}; path && *path) return std::filesystem::path{path} / ".local/share/apache/game";
  return "apache-saves/game";
#endif
}
std::filesystem::path select_game_directory(std::optional<std::filesystem::path> const &requested, std::filesystem::path const &save_directory) {
  if(requested) return *requested;
  for(auto const *candidate : {".", "data"}) if(looks_installed(candidate)) return candidate;
  auto const record{save_directory / "game-directory.txt"};
  std::error_code error;
  if(std::filesystem::is_regular_file(record, error) && std::filesystem::file_size(record, error) < 32768) {
    std::ifstream input{record}; std::string path;
    if(std::getline(input, path) && !path.empty()) {
      std::filesystem::path const remembered{std::u8string{reinterpret_cast<char8_t const*>(path.data()), path.size()}};
      if(looks_installed(remembered)) return remembered;
    }
  }
  auto const fallback{default_game_directory()};
  if(looks_installed(fallback)) return fallback;
  throw std::runtime_error{"Apache game data were not found. Run scripts/fetch-assets.sh (fetch-assets.bat on Windows), or select an extracted CD or installation with --data-dir DIRECTORY. Default location: " + fallback.string()};
}
void remember_game_directory(std::filesystem::path const &directory, std::filesystem::path const &save_directory) {
  auto const path{std::filesystem::weakly_canonical(directory).generic_u8string()};
  if(path.find_first_of(u8"\r\n") != std::u8string::npos) throw std::runtime_error{"Game directory cannot contain a line break"};
  std::filesystem::create_directories(save_directory);
  // A small preference only: no original resources are copied or changed here.
  std::ofstream output{save_directory / "game-directory.txt", std::ios::binary | std::ios::trunc};
  if(!output.write(reinterpret_cast<char const*>(path.data()), static_cast<std::streamsize>(path.size())) || !output.put('\n').flush()) throw std::runtime_error{"Cannot remember the game data directory"};
}
void check_reference_installation(std::filesystem::path const &directory) {
  data_store data{directory};
  for(auto const &file : files) {
    auto const contents{data.read(resource_name(data, file))};
    if(contents.size() != file.size || fingerprint(contents) != file.hash) throw std::runtime_error{"Game file does not match the reference CD: " + std::string{file.name}};
  }
}
void install_reference_cd(std::filesystem::path const &image, std::filesystem::path const &directory) {
  auto const size{std::filesystem::file_size(image)};
  bool const raw{size == 375002880};
  if(!raw && size != 375002880ull / 2352 * 2048) throw std::runtime_error{"Expected the October 2000 European Apache CD (MODE1/2352 BIN or its 2048-byte-sector ISO)"};
  std::ifstream source{image, std::ios::binary};
  auto const read{[&](entry const &file) {
    bytes result(file.size);
    for(size_t offset{0}; offset < result.size(); offset += 2048) {
      uint64_t const sector{file.sector + offset / 2048};
      source.seekg(static_cast<std::streamoff>(sector * (raw ? 2352 : 2048) + (raw ? 16 : 0)));
      if(!source.read(reinterpret_cast<char*>(result.data() + offset), static_cast<std::streamsize>(std::min(size_t{2048}, result.size() - offset)))) throw std::runtime_error{"Truncated CD image"};
    }
    if(fingerprint(result) != file.hash) throw std::runtime_error{"CD content does not match the reference: " + std::string{file.name}};
    return result;
  }};
  std::unique_ptr<data_store> existing;
  if(std::filesystem::exists(directory)) existing = std::make_unique<data_store>(directory);
  // Validate the entire source and every existing destination before writing.
  std::vector<entry const*> missing;
  for(auto const &file : files) {
    read(file);
    auto const name{existing ? resource_name(*existing, file) : std::string{file.name}};
    if(existing && existing->contains(name)) {
      auto const contents{existing->read(name)};
      if(contents.size() != file.size || fingerprint(contents) != file.hash) throw std::runtime_error{"Existing game file differs; choose an empty destination: " + name};
    } else {
      auto const target{checked_destination(directory, directory / file.name)};
      if(std::filesystem::exists(target)) throw std::runtime_error{"Cannot replace existing path: " + target.string()};
      missing.push_back(&file);
    }
  }
  if(missing.empty()) return;
  std::filesystem::create_directories(directory);
  auto const staging{directory / (".apache-install-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))};
  if(!std::filesystem::create_directory(staging)) throw std::runtime_error{"Cannot create temporary installation directory"};
  try {
    for(auto const *file : missing) {
      auto const contents{read(*file)};
      auto const temporary{staging / file->name};
      std::filesystem::create_directories(temporary.parent_path());
      std::ofstream output{temporary, std::ios::binary};
      if(!output.write(reinterpret_cast<char const*>(contents.data()), static_cast<std::streamsize>(contents.size())) || !output.flush()) throw std::runtime_error{"Cannot extract " + std::string{file->name}};
      output.close();
      if(fingerprint(read_file(temporary)) != file->hash) throw std::runtime_error{"Extracted file failed verification: " + std::string{file->name}};
    }
    for(auto const *file : missing) {
      auto const target{checked_destination(directory, directory / file->name)};
      std::filesystem::create_directories(target.parent_path());
      if(std::filesystem::exists(target)) throw std::runtime_error{"Destination appeared during extraction: " + target.string()};
      std::filesystem::rename(staging / file->name, target);
    }
    std::filesystem::remove_all(staging);
  } catch(...) {
    std::error_code ignored; std::filesystem::remove_all(staging, ignored); throw;
  }
}
} // namespace apache::resources
