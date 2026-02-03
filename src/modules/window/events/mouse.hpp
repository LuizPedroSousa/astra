#pragma once

#include "guid.hpp"

namespace astralix::input {
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

  Position delta() {
    return Position{
        m_current.x,
        m_current.y,
    };
  };

  void set_position(Position position) {
    if (m_is_first_recalculation) {
      m_last = position;
      m_is_first_recalculation = false;
    }

    double dx = position.x - m_last.x;
    double dy = position.y - m_last.y;

    m_current.x += dx;
    m_current.y += dy;

    m_last = position;

    m_changed = true;
  }

  void apply_delta(Position &position) {
    m_current.x += position.x;
    m_current.y += position.y;

    m_changed = true;
  }

  void reset_delta() {
    m_current = {.x = 0, .y = 0};
    m_changed = false;
  }

  bool has_moved() { return m_changed; }

  Mouse(WindowID &window_id, double initial_mouse_x, double initial_mouse_y);
  ~Mouse();

private:
  Position m_current;
  Position m_last;

  bool m_is_first_recalculation = true;
  bool m_changed = false;

  WindowID m_window_id;
};
} // namespace astralix::input
