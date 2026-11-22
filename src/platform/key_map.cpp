#include "key_map.h"
#include <array>
#include <GLFW/glfw3.h>
namespace apache::platform {
std::optional<uint16_t> scan_code(int const key, int const modifiers) {
  uint16_t scan{0};
  if(key >= GLFW_KEY_A && key <= GLFW_KEY_Z) {
    static std::array<uint16_t, 26> constexpr letters{0x1e, 0x30, 0x2e, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17, 0x24, 0x25, 0x26, 0x32, 0x31, 0x18, 0x19, 0x10, 0x13, 0x1f, 0x14, 0x16, 0x2f, 0x11, 0x2d, 0x15, 0x2c};
    scan = letters[static_cast<size_t>(key - GLFW_KEY_A)];
  } else if(key >= GLFW_KEY_1 && key <= GLFW_KEY_9) {
    scan = static_cast<uint16_t>(key - GLFW_KEY_1 + 2);
  } else if(key >= GLFW_KEY_F1 && key <= GLFW_KEY_F10) {
    scan = static_cast<uint16_t>(key - GLFW_KEY_F1 + 0x3b);
  } else {
    switch(key) {
    case GLFW_KEY_0:
      scan = 0xb;
      break;
    case GLFW_KEY_ESCAPE:
      scan = 1;
      break;
    case GLFW_KEY_MINUS:
      scan = 0xc;
      break;
    case GLFW_KEY_EQUAL:
      scan = 0xd;
      break;
    case GLFW_KEY_BACKSPACE:
      scan = 0xe;
      break;
    case GLFW_KEY_TAB:
      scan = 0xf;
      break;
    case GLFW_KEY_LEFT_BRACKET:
      scan = 0x1a;
      break;
    case GLFW_KEY_RIGHT_BRACKET:
      scan = 0x1b;
      break;
    case GLFW_KEY_ENTER:
    case GLFW_KEY_KP_ENTER:
      scan = 0x1c;
      break;
    case GLFW_KEY_LEFT_CONTROL:
    case GLFW_KEY_RIGHT_CONTROL:
      scan = 0x1d;
      break;
    case GLFW_KEY_SEMICOLON:
      scan = 0x27;
      break;
    case GLFW_KEY_APOSTROPHE:
      scan = 0x28;
      break;
    case GLFW_KEY_GRAVE_ACCENT:
      scan = 0x29;
      break;
    case GLFW_KEY_LEFT_SHIFT:
      scan = 0x2a;
      break;
    case GLFW_KEY_BACKSLASH:
      scan = 0x2b;
      break;
    case GLFW_KEY_COMMA:
      scan = 0x33;
      break;
    case GLFW_KEY_PERIOD:
      scan = 0x34;
      break;
    case GLFW_KEY_SLASH:
      scan = 0x35;
      break;
    case GLFW_KEY_RIGHT_SHIFT:
      scan = 0x36;
      break;
    case GLFW_KEY_KP_MULTIPLY:
      scan = 0x37;
      break;
    case GLFW_KEY_LEFT_ALT:
    case GLFW_KEY_RIGHT_ALT:
      scan = 0x38;
      break;
    case GLFW_KEY_SPACE:
      scan = 0x39;
      break;
    case GLFW_KEY_HOME:
    case GLFW_KEY_KP_7:
      scan = 0x47;
      break;
    case GLFW_KEY_UP:
    case GLFW_KEY_KP_8:
      scan = 0x48;
      break;
    case GLFW_KEY_PAGE_UP:
    case GLFW_KEY_KP_9:
      scan = 0x49;
      break;
    case GLFW_KEY_KP_SUBTRACT:
      scan = 0x4a;
      break;
    case GLFW_KEY_LEFT:
    case GLFW_KEY_KP_4:
      scan = 0x4b;
      break;
    case GLFW_KEY_RIGHT:
    case GLFW_KEY_KP_6:
      scan = 0x4d;
      break;
    case GLFW_KEY_KP_ADD:
      scan = 0x4e;
      break;
    case GLFW_KEY_END:
    case GLFW_KEY_KP_1:
      scan = 0x4f;
      break;
    case GLFW_KEY_DOWN:
    case GLFW_KEY_KP_2:
      scan = 0x50;
      break;
    case GLFW_KEY_PAGE_DOWN:
    case GLFW_KEY_KP_3:
      scan = 0x51;
      break;
    case GLFW_KEY_INSERT:
    case GLFW_KEY_KP_0:
      scan = 0x52;
      break;
    case GLFW_KEY_DELETE:
    case GLFW_KEY_KP_DECIMAL:
      scan = 0x53;
      break;
    case GLFW_KEY_F11:
      scan = 0x57;
      break;
    case GLFW_KEY_F12:
      scan = 0x58;
      break;
    case GLFW_KEY_PAUSE:
      scan = 0x19;
      break;
    default:
      return {};
    }
  }
  if(modifiers & GLFW_MOD_ALT) scan |= 0x1000;
  if(modifiers & GLFW_MOD_CONTROL) scan |= 0x2000;
  if(modifiers & GLFW_MOD_SHIFT) scan |= 0x4000;
  return scan;
}
} // namespace apache::platform
