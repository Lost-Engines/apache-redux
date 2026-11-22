#pragma once
#include <iosfwd>
void inspect_flight_systems(std::istream &input, std::ostream &output);
void inspect_flight_dynamics(std::istream &input, std::ostream &output);
namespace apache::resources {
class data_store;
}
void inspect_flight_trace(std::istream &input, std::ostream &output, apache::resources::data_store const &data);
