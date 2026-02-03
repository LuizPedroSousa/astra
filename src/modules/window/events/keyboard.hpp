#pragma once

#include "events/key-codes.hpp"
#include "guid.hpp"
#include <unordered_map>

namespace astralix::input {

class Keyboard {
public:
  enum KeyEvent {
    KeyPressed = 0,
    KeyDown = 1,
    KeyReleased = 2,
  };

  struct KeyState {
    KeyEvent event;
    SchedulerID scheduler_id;
  };

  Keyboard(WindowID &window_id);
  ~Keyboard();

  bool is_key_down(KeyCode key_code);

  bool is_key_released(KeyCode key_code);

  void release_keys();
  void release_key(KeyCode key, WindowID window_id);

  void attach_key(KeyCode keycode, KeyState key_state);

  void destroy_key(KeyCode keycode);

  void destroy_release_keys();

private:
  std::unordered_map<KeyCode, KeyState> m_key_events;
  WindowID m_window_id;
};
} // namespace astralix::input
