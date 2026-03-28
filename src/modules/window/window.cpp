#include "window.hpp"

#include "clipboard.hpp"

#include "events/keyboard.hpp"
#include "events/mouse.hpp"

#include "assert.hpp"

#include "events/key-codes.hpp"

#include "event-dispatcher.hpp"
#include "events/key-event.hpp"
#include "events/mouse-event.hpp"
#include "guid.hpp"
#include "iostream"
#include "managers/window-manager.hpp"
#include "stdio.h"

namespace astralix {

void Window::handle_errors(int, const char *description) {
  std::cout << description << std::endl;
}

GLFWwindow *Window::handle() { return m_value; }

Window::Window(WindowID &id, std::string &title, int &width, int &height,
               bool headless)
    : m_id(id), m_title(title), m_width(width), m_height(height),
      m_headless(headless) {
  glfwSetErrorCallback(handle_errors);
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  auto initial_mouse_x = static_cast<double>(height) / 2;
  auto initial_mouse_y = static_cast<double>(width) / 2;

  m_keyboard = create_ref<input::Keyboard>(id);
  m_mouse = create_ref<input::Mouse>(id, initial_mouse_x, initial_mouse_y);

  // auto engine = Engine::get();
  // if (engine->has_msaa_enabled())
  //   glfwWindowHint(GLFW_SAMPLES, engine->msaa.samples);
}

Window::~Window() {
  if (m_horizontal_resize_cursor != nullptr) {
    glfwDestroyCursor(m_horizontal_resize_cursor);
  }
  if (m_vertical_resize_cursor != nullptr) {
    glfwDestroyCursor(m_vertical_resize_cursor);
  }
  if (m_move_cursor != nullptr) {
    glfwDestroyCursor(m_move_cursor);
  }
  if (m_diagonal_nwse_resize_cursor != nullptr) {
    glfwDestroyCursor(m_diagonal_nwse_resize_cursor);
  }
  if (m_diagonal_nesw_resize_cursor != nullptr) {
    glfwDestroyCursor(m_diagonal_nesw_resize_cursor);
  }
}

Ref<Window> Window::create(WindowID &id, std::string &title, int &width,
                           int &height, bool headless) {
  return create_ref<Window>(id, title, width, height, headless);
}

void Window::resizing(GLFWwindow *window, int width, int height) {
  auto self = static_cast<Window *>(glfwGetWindowUserPointer(window));

  self->m_width = width;
  self->m_height = height;

  glViewport(0, 0, width, height);

  self->was_resized = true;
}

void Window::start() {
  if (m_headless) {
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
  }

  GLFWwindow *window =
      glfwCreateWindow(m_width, m_height, m_title.c_str(), NULL, NULL);

  m_value = window;

  if (window == NULL) {
    glfwTerminate();

    const char *description;
    int errorCode = glfwGetError(&description);

    ASTRA_EXCEPTION(std::string("Couldn't create window for OpenGL. ") +
                    description);
  }

  glfwMakeContextCurrent(m_value);

  ASTRA_ENSURE(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress),
               "Couldn't load GLAD");

  glViewport(0, 0, m_width, m_height);

  glfwSetWindowUserPointer(window, this);

  if (!m_headless) {
    glfwSetFramebufferSizeCallback(m_value, resizing);
    EventDispatcher::get()
        ->attach<input::KeyboardListener, input::KeyReleasedEvent>(
            ASTRA_BIND_EVENT_FN(Window::toggle_view_mouse));

    glfwSetCursorPosCallback(m_value, mouse_callback);
    glfwSetMouseButtonCallback(m_value, mouse_button_callback);
    glfwSetCharCallback(m_value, char_callback);
    glfwSetScrollCallback(m_value, scroll_callback);

    glfwSetKeyCallback(m_value, key_callback);
    m_horizontal_resize_cursor = glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR);
    m_vertical_resize_cursor = glfwCreateStandardCursor(GLFW_VRESIZE_CURSOR);
#ifdef GLFW_RESIZE_ALL_CURSOR
    m_move_cursor = glfwCreateStandardCursor(GLFW_RESIZE_ALL_CURSOR);
#endif
    m_diagonal_nwse_resize_cursor =
        glfwCreateStandardCursor(GLFW_RESIZE_NWSE_CURSOR);
    m_diagonal_nesw_resize_cursor =
        glfwCreateStandardCursor(GLFW_RESIZE_NESW_CURSOR);
    set_cursor_captured(true);
  }
}

void Window::toggle_view_mouse(input::KeyReleasedEvent *event) {
  if (event->key_code == input::KeyCode::Escape) {
    if (auto window = window_manager()->get_window_by_id(event->window_id)) {
      window->set_cursor_captured(!window->cursor_captured());
    }
  }
}

void Window::mouse_callback(GLFWwindow *window, double x, double y) {
  auto self = static_cast<Window *>(glfwGetWindowUserPointer(window));
  self->m_mouse->set_position(input::Mouse::Position{.x = x, .y = y});

  auto event = MouseEvent(x, y, self->id());
  EventDispatcher::get()->dispatch(&event);
};

void Window::mouse_button_callback(GLFWwindow *window, int button, int action,
                                   int mods) {
  auto self = static_cast<Window *>(glfwGetWindowUserPointer(window));
  if (self == nullptr) {
    return;
  }

  const auto modifiers = input::KeyModifiers::from_glfw(mods);

  input::MouseButton mapped_button;
  switch (button) {
  case GLFW_MOUSE_BUTTON_LEFT:
    mapped_button = input::MouseButton::Left;
    break;
  case GLFW_MOUSE_BUTTON_RIGHT:
    mapped_button = input::MouseButton::Right;
    break;
  case GLFW_MOUSE_BUTTON_MIDDLE:
    mapped_button = input::MouseButton::Middle;
    break;
  default:
    return;
  }

  if (action == GLFW_PRESS) {
    self->m_mouse->set_button_state(mapped_button, true);
    auto event = MouseButtonPressedEvent(mapped_button, self->id(), modifiers);
    EventDispatcher::get()->dispatch(&event);
  } else if (action == GLFW_RELEASE) {
    self->m_mouse->set_button_state(mapped_button, false);
    auto event =
        MouseButtonReleasedEvent(mapped_button, self->id(), modifiers);
    EventDispatcher::get()->dispatch(&event);
  }
}

void Window::char_callback(GLFWwindow *window, unsigned int codepoint) {
  auto self = static_cast<Window *>(glfwGetWindowUserPointer(window));
  if (self == nullptr) {
    return;
  }

  int mods = 0;
  if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
      glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS) {
    mods |= GLFW_MOD_SHIFT;
  }
  if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
      glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS) {
    mods |= GLFW_MOD_CONTROL;
  }
  if (glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS ||
      glfwGetKey(window, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS) {
    mods |= GLFW_MOD_ALT;
  }
  if (glfwGetKey(window, GLFW_KEY_LEFT_SUPER) == GLFW_PRESS ||
      glfwGetKey(window, GLFW_KEY_RIGHT_SUPER) == GLFW_PRESS) {
    mods |= GLFW_MOD_SUPER;
  }

  auto event = input::CharacterInputEvent(
      codepoint, self->id(), input::KeyModifiers::from_glfw(mods));
  EventDispatcher::get()->dispatch(&event);
}

void Window::scroll_callback(GLFWwindow *window, double xoffset,
                             double yoffset) {
  auto self = static_cast<Window *>(glfwGetWindowUserPointer(window));
  if (self == nullptr) {
    return;
  }

  int mods = 0;
  if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
      glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS) {
    mods |= GLFW_MOD_SHIFT;
  }
  if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
      glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS) {
    mods |= GLFW_MOD_CONTROL;
  }
  if (glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS ||
      glfwGetKey(window, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS) {
    mods |= GLFW_MOD_ALT;
  }
  if (glfwGetKey(window, GLFW_KEY_LEFT_SUPER) == GLFW_PRESS ||
      glfwGetKey(window, GLFW_KEY_RIGHT_SUPER) == GLFW_PRESS) {
    mods |= GLFW_MOD_SUPER;
  }

  auto event = MouseWheelEvent(
      xoffset, yoffset, self->id(), input::KeyModifiers::from_glfw(mods));
  EventDispatcher::get()->dispatch(&event);
}

void Window::key_callback(GLFWwindow *window, int key, int scancode, int action,
                          int mods) {
  (void)scancode;
  auto self = static_cast<Window *>(glfwGetWindowUserPointer(window));
  if (self == nullptr) {
    return;
  }

  const auto key_code = input::KeyCode(key);
  const auto modifiers = input::KeyModifiers::from_glfw(mods);

  switch (action) {
    case GLFW_PRESS:
      self->m_keyboard->attach_key(
          key_code,
          input::Keyboard::KeyState{.event = input::Keyboard::KeyEvent::KeyDown});
      {
        auto event =
            input::KeyPressedEvent(key_code, self->id(), modifiers, false);
        EventDispatcher::get()->dispatch(&event);
      }
      break;
    case GLFW_REPEAT: {
      auto event = input::KeyPressedEvent(key_code, self->id(), modifiers, true);
      EventDispatcher::get()->dispatch(&event);
      break;
    }
    default:
      break;
  }
}

std::string Window::clipboard_text() const {
  return clipboard_backend()->get(m_value);
}

void Window::set_clipboard_text(const std::string &text) const {
  clipboard_backend()->set(m_value, text);
}

void Window::set_cursor_icon(CursorIcon icon) {
  m_cursor_icon = icon;

  if (m_value == nullptr || m_headless || m_cursor_captured) {
    return;
  }

  GLFWcursor *cursor = nullptr;
  switch (icon) {
  case CursorIcon::Move:
    cursor = m_move_cursor;
    break;
  case CursorIcon::ResizeHorizontal:
    cursor = m_horizontal_resize_cursor;
    break;
  case CursorIcon::ResizeVertical:
    cursor = m_vertical_resize_cursor;
    break;
  case CursorIcon::ResizeDiagonalNwSe:
    cursor = m_diagonal_nwse_resize_cursor;
    break;
  case CursorIcon::ResizeDiagonalNeSw:
    cursor = m_diagonal_nesw_resize_cursor;
    break;
  case CursorIcon::Default:
  default:
    cursor = nullptr;
    break;
  }

  glfwSetCursor(m_value, cursor);
}

void Window::capture_cursor(bool captured) {
  if (m_value == nullptr) {
    m_cursor_captured = captured;
    return;
  }

  if (m_cursor_captured == captured) {
    return;
  }

  set_cursor_captured(captured);
}

void Window::set_cursor_captured(bool captured) {
  m_cursor_captured = captured;

  if (captured) {
    glfwSetCursorPos(m_value, m_width / 2.0, m_height / 2.0);
    glfwSetCursor(m_value, nullptr);
  }

  glfwSetInputMode(m_value, GLFW_CURSOR,
                   captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);

  if (!captured) {
    set_cursor_icon(m_cursor_icon);
  }
}

void Window::update() {
  glfwPollEvents();

  if (!m_headless)
    m_keyboard->release_keys();
}

void Window::swap() {
  m_mouse->reset_delta();
  m_keyboard->destroy_release_keys();

  glfwSwapBuffers(m_value);

  if (was_resized) {
    was_resized = false;
  }
}

void Window::close() { glfwTerminate(); }

bool Window::is_open() { return !glfwWindowShouldClose(m_value); }

} // namespace astralix
//
