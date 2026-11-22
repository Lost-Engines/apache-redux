#pragma once
#include <iosfwd>
#include "resources/data_store.h"
void inspect_blasts(std::istream &input, std::ostream &output, apache::resources::data_store const &data);
