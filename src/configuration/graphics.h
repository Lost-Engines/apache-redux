#pragma once
#include <optional>
#include <string_view>
namespace apache::configuration {
struct resolution { unsigned int width{640}, height{480}; };
struct graphics {
  std::optional<resolution> fixed_resolution;
  double field_of_view{90};
  double lod_distance_multiplier{6};
};
std::optional<resolution> parse_resolution(std::string_view text);
double parse_field_of_view(std::string_view text);
double parse_lod_distance_multiplier(std::string_view text);
} // namespace apache::configuration
