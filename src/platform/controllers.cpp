#include "controllers.h"
#include <iterator>
#include <map>
#include <GLFW/glfw3.h>

namespace apache::platform {
std::vector<input::device_state> poll_controllers() {
  std::vector<input::device_state> result;
  std::map<std::string, unsigned int> instances;
  for(int id{GLFW_JOYSTICK_1}; id <= GLFW_JOYSTICK_LAST; ++id) {
    if(!glfwJoystickPresent(id)) continue;
    input::device_state d;
    if(auto const *name{glfwGetJoystickName(id)}) d.id.name = name;
    if(auto const *guid{glfwGetJoystickGUID(id)}) d.id.guid = guid;
    d.id.instance = instances[d.id.guid.empty() ? d.id.name : d.id.guid]++;
    int count{0};
    if(auto const *axes{glfwGetJoystickAxes(id, &count)}; axes && count > 0) d.axes.assign(axes, axes + count);
    if(auto const *buttons{glfwGetJoystickButtons(id, &count)}; buttons && count > 0) d.buttons.assign(buttons, buttons + count);
    if(auto const *hats{glfwGetJoystickHats(id, &count)}; hats && count > 0) d.hats.assign(hats, hats + count);
    GLFWgamepadstate state;
    if(glfwGetGamepadState(id, &state)) {
      d.gamepad_axes.assign(std::begin(state.axes), std::end(state.axes));
      d.gamepad_buttons.assign(std::begin(state.buttons), std::end(state.buttons));
    }
    result.push_back(std::move(d));
  }
  return result;
}
} // namespace apache::platform
