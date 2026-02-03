#include "window.hpp"

#include "events/keyboard.hpp"
#include "events/mouse.hpp"

#include "assert.hpp"

#include "events/event-scheduler.hpp"

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

GLFWwindow *Window::value() { return m_value; }

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

Window::~Window() {}

Ref<Window> Window::create(WindowID &id, std::string &title, int &width,
                           int &height, bool headless) {
  return create_ref<Window>(id, title, width, height, headless);
}

void Window::resizing(GLFWwindow *window, int width, int height) {
  auto self = static_cast<Window *>(glfwGetWindowUserPointer(window));

  self->m_width = width;
  self->m_height = height;

  glViewport(0, 0, width, height);
}

static bool has_pressed = false;

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

    glfwSetKeyCallback(m_value, key_callback);
  }
}

void Window::toggle_view_mouse(input::KeyReleasedEvent *event) {
  if (event->key_code == input::KeyCode::Escape) {
    has_pressed = !has_pressed;

    if (auto window = window_manager()->get_window_by_id(event->window_id)) {
      glfwSetCursorPos(window->value(), window->width() / 2.0f,
                       window->height() / 2.0f);

      glfwSetInputMode(window->value(), GLFW_CURSOR,
                       has_pressed ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
    }
  }
}

void Window::mouse_callback(GLFWwindow *window, double x, double y) {
  if (!has_pressed) {
    auto self = static_cast<Window *>(glfwGetWindowUserPointer(window));

    self->m_mouse->set_position(input::Mouse::Position{.x = x, .y = y});

    EventDispatcher::get()->dispatch(new MouseEvent(x, y));
  }
};

void Window::key_callback(GLFWwindow *window, int key, int scancode, int action,
                          int mods) {
  auto scheduler = EventScheduler::get();

  switch (action) {
  case GLFW_PRESS: {
    auto event = input::KeyCode(key);

    auto self = static_cast<Window *>(glfwGetWindowUserPointer(window));

    auto scheduler_id = scheduler->schedule<input::KeyPressedEvent>(
        SchedulerType::POST_FRAME, event, self->id());

    self->m_keyboard->attach_key(
        event,
        input::Keyboard::KeyState{.event = input::Keyboard::KeyEvent::KeyDown,
                                  .scheduler_id = scheduler_id});
    break;
  }
  default:
    break;
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
}

void Window::close() { glfwTerminate(); }

bool Window::is_open() { return !glfwWindowShouldClose(m_value); }

} // namespace astralix
//
