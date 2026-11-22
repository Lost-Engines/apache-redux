#pragma once
#include <iosfwd>
#include "resources/data_store.h"
void inspect_mission_targets(std::istream &, std::ostream &, apache::resources::data_store const &, std::string const &, std::string const &);
