#pragma once
#include <iosfwd>
#include "resources/data_store.h"
namespace apache {
void inspect_chat(resources::data_store const &data, std::istream &input, std::ostream &out);
void inspect_network(std::istream &input, std::ostream &output);
void inspect_network_live(resources::data_store const &data, unsigned int mode, std::ostream &output, bool expect_rejection = false);
void inspect_network_group(resources::data_store const &data, unsigned int players, std::ostream &out);
}
