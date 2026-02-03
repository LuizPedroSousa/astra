#pragma once
#include "event.hpp"
#include "functional"
#include "guid.hpp"
#include "key-codes.hpp"
#include "listener.hpp"

#define BASE_FIELDS                                                            \
  KeyCode key_code;                                                            \
  WindowID window_id;

namespace astralix::input {

class KeyPressedEvent : public Event {
public:
  KeyPressedEvent(KeyCode key_code, WindowID window_id)
      : key_code(key_code), window_id(window_id), Event() {}

  BASE_FIELDS
  EVENT_TYPE(KeyPressed)
};

class KeyReleasedEvent : public Event {
public:
  KeyReleasedEvent(KeyCode key_code, WindowID window_id)
      : key_code(key_code), window_id(window_id) {}

  BASE_FIELDS;
  EVENT_TYPE(KeyReleased)
};

class KeyboardListener : public BaseListener {
public:
  KeyboardListener(const std::function<void(Event *)> &callback)
      : m_callback(callback) {}

  void dispatch(Event *event) override { m_callback(event); }

  LISTENER_CLASS_TYPE(Keyboard)

private:
  std::function<void(Event *)> m_callback;
};
} // namespace astralix::input
