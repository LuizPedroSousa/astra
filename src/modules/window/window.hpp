#pragma once

#include "base.hpp"
#include "events/key-event.hpp"
#include "guid.hpp"

#include "events/keyboard.hpp"
#include "events/mouse.hpp"

#include "glad/glad.h"

#include "GLFW/glfw3.h"

namespace astralix {

class Window {
public:
  Window(WindowID &id, std::string &title, int &width, int &height,
         bool headless);
  ~Window();

  static Ref<Window> create(WindowID &id, std::string &title, int &width,
                            int &height, bool headless);

  void start();

  WindowID id() const noexcept { return m_id; }
  int height() const noexcept { return m_height; }
  int width() const noexcept { return m_width; }
  GLFWwindow *value() const noexcept { return m_value; }
  std::string title() const noexcept { return m_title; }

  void update();
  void swap();

  GLFWwindow *value();
  bool is_open();
  void close();

  Ref<input::Mouse> const mouse() const { return m_mouse; }
  Ref<input::Keyboard> const keyboard() const { return m_keyboard; }

private:
  static void resizing(GLFWwindow *window, int width, int height);
  static void handle_errors(int, const char *description);
  static void mouse_callback(GLFWwindow *window, double xpos, double ypos);
  static void key_callback(GLFWwindow *window, int key, int scancode,
                           int action, int mods);
  static void toggle_view_mouse(input::KeyReleasedEvent *event);

  GLFWwindow *m_value;

  int m_height = 0;
  int m_width = 0;
  std::string m_title;

  WindowID m_id;

  Ref<input::Keyboard> m_keyboard;
  Ref<input::Mouse> m_mouse;

  bool m_headless = false;
};
} // namespace astralix
