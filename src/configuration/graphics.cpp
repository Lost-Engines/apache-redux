#include "graphics.h"
#include <charconv>
#include <cmath>
#include <stdexcept>
namespace apache::configuration {
std::optional<resolution> parse_resolution(std::string_view text) {
  if(text == "auto") return {};
  auto const separator{text.find_first_of("xX")};
  auto const dimension{[](std::string_view value) {
    unsigned int result{0};
    auto const [end, error]{std::from_chars(value.data(), value.data() + value.size(), result)};
    if(error != std::errc{} || end != value.data() + value.size() || result < 160 || result > 8192)
      throw std::runtime_error{"Resolution must be auto or WIDTHxHEIGHT, with dimensions from 160 to 8192"};
    return result;
  }};
  if(separator == std::string_view::npos) throw std::runtime_error{"Resolution must be auto or WIDTHxHEIGHT"};
  resolution const result{dimension(text.substr(0, separator)), dimension(text.substr(separator + 1))};
  if(result.width > result.height * 8u || result.height > result.width * 8u)
    throw std::runtime_error{"Resolution aspect ratio must be between 1:8 and 8:1"};
  return result;
}
double parse_field_of_view(std::string_view text) {
  double result{0};
  auto const [end, error]{std::from_chars(text.data(), text.data() + text.size(), result)};
  if(error != std::errc{} || end != text.data() + text.size() || !std::isfinite(result) || result < 30 || result > 150)
    throw std::runtime_error{"Horizontal FOV must be between 30 and 150 degrees"};
  return result;
}
double parse_lod_distance_multiplier(std::string_view text) {
  double result{0};
  auto const [end, error]{std::from_chars(text.data(), text.data() + text.size(), result)};
  if(error != std::errc{} || end != text.data() + text.size() || !std::isfinite(result) || result < 0.1 || result > 32)
    throw std::runtime_error{"LOD distance multiplier must be between 0.1 and 32"};
  return result;
}
} // namespace apache::configuration
