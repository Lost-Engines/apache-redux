#pragma once
#include <iosfwd>
#include "resources/data_store.h"
void inspect_mission_combat(std::istream &, std::ostream &, apache::resources::data_store const &, std::string const &, std::string const &, bool armed = false, bool hurt = false);
