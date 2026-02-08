#include "keyboard.hpp"

#include "event-scheduler.hpp"
#include "events/key-event.hpp"
#include "guid.hpp"
#include "key-codes.hpp"
#include "log.hpp"
#include "managers/window-manager.hpp"
#include <GLFW/glfw3.h>

namespace astralix::input {
Keyboard::Keyboard(WindowID &window_id) : m_window_id(window_id) {};

Keyboard::~Keyboard() {}

void Keyboard::release_keys() {
  for (auto [key, value] : m_key_events) {
    auto window = window_manager()->get_window_by_id(m_window_id);

    auto state = glfwGetKey(window->value(), (int)key);

    if (state == GLFW_RELEASE) {
      auto keycode = KeyReleasedEvent(KeyCode(key), window->id());

      EventDispatcher::get()->dispatch(&keycode);

      m_key_events[key].event = KeyEvent::KeyReleased;
    }
  }
}

void Keyboard::release_key(KeyCode key, WindowID window_id) {
  LOG_INFO("KeyReleased");

  auto key_exists = m_key_events.find(key);

  if (key_exists == m_key_events.end())
    return;

  auto event = KeyReleasedEvent(key, window_id);

  EventDispatcher::get()->dispatch(&event);

  m_key_events[key].event = KeyEvent::KeyReleased;
}

void Keyboard::destroy_release_keys() {
  auto it = m_key_events.begin();
  auto scheduler = EventScheduler::get();

  for (; it != m_key_events.end();) {
    if (it->second.event == KeyEvent::KeyReleased) {
      if (scheduler->has_schedulers()) {
        scheduler->destroy(it->second.scheduler_id);
      }

      it = m_key_events.erase(it);
    } else {
      it++;
    }
  }
}

bool Keyboard::is_key_down(KeyCode key_code) {
  auto at = m_key_events.find(key_code);

  return at != nullptr && at->second.event == KeyEvent::KeyDown;
}

bool Keyboard::is_key_released(KeyCode key_code) {
  auto at = m_key_events.find(key_code);

  return at != nullptr && at->second.event == KeyEvent::KeyReleased;
}

void Keyboard::attach_key(KeyCode keycode, KeyState key_state) {
  m_key_events.emplace(keycode, key_state);
};

void Keyboard::destroy_key(KeyCode keycode) { m_key_events.erase(keycode); }

} // namespace astralix::input
