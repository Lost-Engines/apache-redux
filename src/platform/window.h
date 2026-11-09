#pragma once
#include <array>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <GLFW/glfw3.h>
#include "render/framebuffer.h"
#include "configuration/graphics.h"

namespace apache::platform {
struct cursor {
  double x{0};
  double y{0};
};
class glfw_runtime {
public:
  glfw_runtime();
  ~glfw_runtime();
  glfw_runtime(glfw_runtime const&) = delete;
  glfw_runtime &operator=(glfw_runtime const&) = delete;
};
class window {
  glfw_runtime runtime;
  std::unique_ptr<GLFWwindow, decltype(&glfwDestroyWindow)> handle;
  unsigned int texture{0};
  std::vector<render::rgba> pixels;
  int logical_width{640};
  int logical_height{480};
  int windowed_x{0}, windowed_y{0}, windowed_width{960}, windowed_height{720};
  bool windowed_maximized{false}, fullscreen_pending{false};
  std::array<bool, 2> fullscreen_enter{};
  void toggle_fullscreen();

public:
  window(unsigned int scale, std::optional<configuration::resolution> resolution = {}, bool fullscreen = false);
  ~window();
  window(window const&) = delete;
  window &operator=(window const&) = delete;
  GLFWwindow *get() const;
  bool poll();
  // Consume the fullscreen chord in every screen; apply it after event dispatch.
  bool fullscreen_key(int key, int action, int modifiers);
  bool focused() const;
  configuration::resolution framebuffer_size() const;
  std::optional<cursor> cursor_position() const;
  void title(std::string const &text);
  void present(render::framebuffer const &frame);
};
} // namespace apache::platform
