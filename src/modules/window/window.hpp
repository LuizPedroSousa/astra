#pragma once

#include "base.hpp"
#include "events/key-event.hpp"
#include "guid.hpp"

#include "events/keyboard.hpp"
#include "events/mouse.hpp"

#include "glad/glad.h"

#include "GLFW/glfw3.h"
#include <string>

namespace astralix {

enum class CursorIcon : uint8_t {
  Default,
  Move,
  ResizeHorizontal,
  ResizeVertical,
  ResizeDiagonalNwSe,
  ResizeDiagonalNeSw,
};

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
  GLFWwindow *handle() const noexcept { return m_value; }
  std::string title() const noexcept { return m_title; }
  bool cursor_captured() const noexcept { return m_cursor_captured; }

  void update();
  void swap();

  GLFWwindow *handle();
  bool is_open();
  void close();
  void capture_cursor(bool captured);
  void set_cursor_icon(CursorIcon icon);
  std::string clipboard_text() const;
  void set_clipboard_text(const std::string &text) const;

  Ref<input::Mouse> const mouse() const { return m_mouse; }
  Ref<input::Keyboard> const keyboard() const { return m_keyboard; }

  bool was_resized = false;

private:
  static void resizing(GLFWwindow *window, int width, int height);
  static void handle_errors(int, const char *description);
  static void mouse_callback(GLFWwindow *window, double xpos, double ypos);
  static void mouse_button_callback(GLFWwindow *window, int button, int action,
                                    int mods);
  static void char_callback(GLFWwindow *window, unsigned int codepoint);
  static void scroll_callback(GLFWwindow *window, double xoffset,
                              double yoffset);
  static void key_callback(GLFWwindow *window, int key, int scancode,
                           int action, int mods);
  static void toggle_view_mouse(input::KeyReleasedEvent *event);
  void set_cursor_captured(bool captured);

  GLFWwindow *m_value;

  int m_height = 0;
  int m_width = 0;
  std::string m_title;

  WindowID m_id;

  Ref<input::Keyboard> m_keyboard;
  Ref<input::Mouse> m_mouse;

  bool m_headless = false;
  bool m_cursor_captured = true;
  CursorIcon m_cursor_icon = CursorIcon::Default;
  GLFWcursor *m_horizontal_resize_cursor = nullptr;
  GLFWcursor *m_vertical_resize_cursor = nullptr;
  GLFWcursor *m_move_cursor = nullptr;
  GLFWcursor *m_diagonal_nwse_resize_cursor = nullptr;
  GLFWcursor *m_diagonal_nesw_resize_cursor = nullptr;
};
} // namespace astralix
