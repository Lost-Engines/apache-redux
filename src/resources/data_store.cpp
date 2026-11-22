#include "data_store.h"
#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace apache::resources {
data_store::data_store(std::filesystem::path const &directory) : root{std::filesystem::absolute(directory)} {
  /// Read case-insensitive DOS names from a selected installation or extracted CD
  if(!std::filesystem::is_directory(root)) throw std::runtime_error{"Game data directory does not exist: " + root.string()};
  for(auto const &entry : std::filesystem::recursive_directory_iterator{root}) {
    if(!entry.is_regular_file()) continue;
    auto const key{normalise(entry.path().lexically_relative(root).generic_string())};
    auto const [_, inserted]{files.emplace(key, entry.path())};
    if(!inserted) throw std::runtime_error{"Ambiguous game-data filename: " + key};
  }
}
std::filesystem::path const &data_store::directory() const {
  return root;
}
std::filesystem::path data_store::resolve(std::string_view const name) const {
  auto const key{normalise(name)};
  if(auto const found{files.find(key)}; found != files.end()) return found->second;
  if(auto const found{files.find("FILES/" + key)}; found != files.end()) return found->second;
  throw std::runtime_error{"Missing game resource: " + std::string{name}};
}
bool data_store::contains(std::string_view const name) const {
  auto const key{normalise(name)};
  return files.contains(key) || files.contains("FILES/" + key);
}
bytes data_store::read(std::string_view const name) const {
  auto const path{resolve(name)};
  auto const length{std::filesystem::file_size(path)};
  if(length > 256 * 1024 * 1024) throw std::runtime_error{"Game resource is too large: " + path.string()};
  bytes result(static_cast<size_t>(length));
  std::ifstream stream{path, std::ios::binary};
  if(!stream.read(reinterpret_cast<char *>(result.data()), static_cast<std::streamsize>(result.size()))) throw std::runtime_error{"Cannot read game resource: " + path.string()};
  return result;
}
std::vector<std::string> data_store::names() const {
  std::vector<std::string> result;
  for(auto const &[name, _] : files) result.push_back(name);
  return result;
}
std::string data_store::normalise(std::string_view const name) {
  std::string result;
  for(char c : name) {
    if(c == '\\') c = '/';
    if(c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    result.push_back(c);
  }
  if(result.empty() || result.front() == '/' || result.find(':') != std::string::npos) throw std::runtime_error{"Expected a relative game-data filename"};
  size_t begin{0};
  while(begin < result.size()) {
    auto const end{result.find('/', begin)};
    auto const part{result.substr(begin, end == std::string::npos ? end : end - begin)};
    if(part == ".." || part == "." || part.empty()) throw std::runtime_error{"Invalid game-data path component"};
    if(end == std::string::npos) break;
    begin = end + 1;
  }
  return result;
}
} // namespace apache::resources
