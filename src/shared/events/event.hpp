#pragma once
namespace astralix {
enum EventType {
  /* KEYBOARD */
  KeyPressed = 0,
  KeyReleased = 1,
  CharacterInput = 2,
  /* MOUSE */
  MouseMovement = 3,
  MouseButtonPressed = 4,
  MouseButtonReleased = 5,
  MouseWheel = 6,

  /* ENTITY */
  EntityCreated = 7,
  Viewport = 8,
  Logs = 9
};

class Event {
public:
  Event() = default;
  virtual EventType get_event_type() const = 0;
};

#define EVENT_TYPE(t)                                                          \
  static EventType get_static_type() { return EventType::t; }                  \
  EventType get_event_type() const override { return get_static_type(); }

} // namespace astralix
