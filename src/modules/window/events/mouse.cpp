#include "mouse.hpp"
#include "guid.hpp"
#include "managers/window-manager.hpp"

namespace astralix::input {

Mouse::Mouse(WindowID &window_id, double initial_mouse_x,
             double initial_mouse_y)
    : m_window_id(window_id) {
  m_last.x = initial_mouse_x;
  m_last.y = initial_mouse_y;
}

Mouse::~Mouse() {}

} // namespace astralix::input
