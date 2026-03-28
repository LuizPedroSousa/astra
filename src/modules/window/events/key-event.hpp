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

struct KeyModifiers {
  bool shift = false;
  bool control = false;
  bool alt = false;
  bool super = false;

  static KeyModifiers from_glfw(int mods) {
    return KeyModifiers{
        .shift = (mods & 0x0001) != 0,
        .control = (mods & 0x0002) != 0,
        .alt = (mods & 0x0004) != 0,
        .super = (mods & 0x0008) != 0,
    };
  }

  bool primary_shortcut() const {
#if defined(__APPLE__)
    return super;
#else
    return control;
#endif
  }
};

class KeyPressedEvent : public Event {
public:
  KeyPressedEvent(KeyCode key_code, WindowID window_id,
                  KeyModifiers modifiers = {}, bool repeat = false)
      : key_code(key_code), window_id(window_id), modifiers(modifiers),
        repeat(repeat), Event() {}

  BASE_FIELDS
  KeyModifiers modifiers;
  bool repeat = false;
  EVENT_TYPE(KeyPressed)
};

class KeyReleasedEvent : public Event {
public:
  KeyReleasedEvent(KeyCode key_code, WindowID window_id,
                   KeyModifiers modifiers = {})
      : key_code(key_code), window_id(window_id), modifiers(modifiers) {}

  BASE_FIELDS;
  KeyModifiers modifiers;
  EVENT_TYPE(KeyReleased)
};

class CharacterInputEvent : public Event {
public:
  CharacterInputEvent(uint32_t codepoint, WindowID window_id,
                      KeyModifiers modifiers = {})
      : codepoint(codepoint), window_id(window_id), modifiers(modifiers) {}

  uint32_t codepoint;
  WindowID window_id;
  KeyModifiers modifiers;
  EVENT_TYPE(CharacterInput)
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
