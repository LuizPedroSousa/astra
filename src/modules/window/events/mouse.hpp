#pragma once

#include "guid.hpp"

namespace astralix::input {

enum class MouseButton : uint8_t {
  Left = 0,
  Right = 1,
  Middle = 2,
  Count = 3,
};

class Mouse {
public:
  struct Position {
    double x;
    double y;

    void operator*=(float factor) {
      x *= factor;
      y *= factor;
    }
  };

  struct ButtonState {
    bool down = false;
    bool pressed = false;
    bool released = false;
  };

  Position delta() {
    return Position{
        m_delta.x,
        m_delta.y,
    };
  };

  Position wheel_delta() const {
    return Position{
        m_wheel_delta.x,
        m_wheel_delta.y,
    };
  }

  Position position() const { return m_position; }

  void set_position(Position position) {
    if (m_is_first_recalculation) {
      m_last = position;
      m_position = position;
      m_is_first_recalculation = false;
    }

    double dx = position.x - m_last.x;
    double dy = position.y - m_last.y;

    m_delta.x += dx;
    m_delta.y += dy;
    m_position = position;

    m_last = position;

    m_changed = true;
  }

  void apply_delta(Position &position) {
    m_delta.x += position.x;
    m_delta.y += position.y;

    m_changed = true;
  }

  void apply_wheel(Position position) {
    m_wheel_delta.x += position.x;
    m_wheel_delta.y += position.y;
  }

  void set_button_state(MouseButton button, bool down);

  bool is_button_down(MouseButton button) const;
  bool is_button_pressed(MouseButton button) const;
  bool is_button_released(MouseButton button) const;

  void reset_delta() {
    m_delta = {.x = 0, .y = 0};
    m_wheel_delta = {.x = 0, .y = 0};
    m_changed = false;

    for (auto &state : m_button_states) {
      state.pressed = false;
      state.released = false;
    }
  }

  bool has_moved() { return m_changed; }

  Mouse(WindowID &window_id, double initial_mouse_x, double initial_mouse_y);
  ~Mouse();

private:
  Position m_delta;
  Position m_wheel_delta{.x = 0.0, .y = 0.0};
  Position m_position;
  Position m_last;
  ButtonState m_button_states[static_cast<size_t>(MouseButton::Count)];

  bool m_is_first_recalculation = true;
  bool m_changed = false;

  WindowID m_window_id;
};
} // namespace astralix::input
