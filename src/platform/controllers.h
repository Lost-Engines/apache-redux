#pragma once
#include "input/controllers.h"
namespace apache::platform {
std::vector<input::device_state> poll_controllers();
}
