#pragma once
#include "event.hpp"
#include "events/key-event.hpp"
#include "events/mouse.hpp"
#include "guid.hpp"

namespace astralix {
struct MouseEvent : public Event {
public:
  MouseEvent(double x, double y, WindowID window_id)
      : x(x), y(y), window_id(std::move(window_id)) {}
  double x;
  double y;
  WindowID window_id;
  EVENT_TYPE(MouseMovement)
};

struct MouseButtonEvent : public Event {
  MouseButtonEvent(input::MouseButton button, WindowID window_id,
                   input::KeyModifiers modifiers = {})
      : button(button), window_id(std::move(window_id)), modifiers(modifiers) {}

  input::MouseButton button;
  WindowID window_id;
  input::KeyModifiers modifiers;
};

struct MouseButtonPressedEvent : public MouseButtonEvent {
  MouseButtonPressedEvent(input::MouseButton button, WindowID window_id,
                          input::KeyModifiers modifiers = {})
      : MouseButtonEvent(button, std::move(window_id), modifiers) {}

  EVENT_TYPE(MouseButtonPressed)
};

struct MouseButtonReleasedEvent : public MouseButtonEvent {
  MouseButtonReleasedEvent(input::MouseButton button, WindowID window_id,
                           input::KeyModifiers modifiers = {})
      : MouseButtonEvent(button, std::move(window_id), modifiers) {}

  EVENT_TYPE(MouseButtonReleased)
};

struct MouseWheelEvent : public Event {
public:
  MouseWheelEvent(double xoffset, double yoffset, WindowID window_id,
                  input::KeyModifiers modifiers = {})
      : xoffset(xoffset), yoffset(yoffset), window_id(std::move(window_id)),
        modifiers(modifiers) {}

  double xoffset;
  double yoffset;
  WindowID window_id;
  input::KeyModifiers modifiers;
  EVENT_TYPE(MouseWheel)
};
} // namespace astralix
