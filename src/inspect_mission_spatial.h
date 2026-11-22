#pragma once
#include <iosfwd>
#include "resources/data_store.h"
void inspect_mission_spatial(std::istream &input, std::ostream &output, apache::resources::data_store const &data, std::string const &zone, std::string const &path);
