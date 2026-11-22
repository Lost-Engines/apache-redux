#include "preferences.h"
#include <algorithm>
#include <cstdint>
#include <limits>

namespace apache::configuration {
namespace {
bool same_key(std::string_view const a, std::string_view const b) {
  auto const lower{[](char const c){return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c;}};
  return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [&](char const x, char const y){return lower(x) == lower(y);});
}
} // anonymous namespace

void preferences::load(std::string_view text) {
  /// Preserve original ordering and spaces while bounding malformed input
  entries.clear();
  while(!text.empty()) {
    auto const end{text.find_first_of("\r\n")};
    auto const line{text.substr(0, end)};
    if(auto const equals{line.find('=')}; equals != std::string_view::npos) entries.insert(entries.begin(), {std::string{line.substr(0, equals)}, std::string{line.substr(equals + 1)}});
    if(end == std::string_view::npos) break;
    text.remove_prefix(end + 1);
  }
}
std::string preferences::serialise() const {
  std::string result;
  for(auto const &[key, value] : entries) result += key + '=' + value + "\r\n";
  return result;
}
std::optional<std::string_view> preferences::get(std::string_view const key) const {
  for(auto const &[name, value] : entries) {
    if(same_key(name, key)) return value;
  }
  return std::nullopt;
}
std::optional<int> preferences::integer(std::string_view const key) const {
  auto const value{get(key)};
  if(!value) return std::nullopt;
  auto text{*value};
  while(!text.empty() && (text.front() == ' ' || text.front() == '\t' || text.front() == '\v' || text.front() == '\f')) text.remove_prefix(1);
  bool const negative{!text.empty() && text.front() == '-'};
  if(!text.empty() && (text.front() == '-' || text.front() == '+')) text.remove_prefix(1);
  uint64_t magnitude{0};
  uint64_t const limit{negative ? uint64_t{std::numeric_limits<int>::max()} + 1 : uint64_t{std::numeric_limits<int>::max()}};
  for(char const c : text) {
    if(c < '0' || c > '9') break;
    magnitude = std::min(limit, magnitude * 10 + static_cast<unsigned int>(c - '0'));
  }
  return static_cast<int>(negative ? -static_cast<int64_t>(magnitude) : static_cast<int64_t>(magnitude));
}
void preferences::set(std::string_view const key, std::string_view const value) {
  for(auto &[name, current] : entries) {
    if(!same_key(name, key)) continue;
    current = value;
    return;
  }
  entries.insert(entries.begin(), {std::string{key}, std::string{value}});
}
void preferences::set(std::string_view const key, int const value) {
  set(key, std::to_string(value));
}
} // namespace apache::configuration
