#include "mouse.hpp"
#include "guid.hpp"
#include "managers/window-manager.hpp"

namespace astralix::input {

Mouse::Mouse(WindowID &window_id, double initial_mouse_x,
             double initial_mouse_y)
    : m_window_id(window_id) {
  m_last.x = initial_mouse_x;
  m_last.y = initial_mouse_y;
  m_position = {.x = initial_mouse_x, .y = initial_mouse_y};
}

void Mouse::set_button_state(MouseButton button, bool down) {
  const size_t index = static_cast<size_t>(button);
  auto &state = m_button_states[index];

  if (state.down == down) {
    return;
  }

  state.down = down;
  state.pressed = down;
  state.released = !down;
}

bool Mouse::is_button_down(MouseButton button) const {
  return m_button_states[static_cast<size_t>(button)].down;
}

bool Mouse::is_button_pressed(MouseButton button) const {
  return m_button_states[static_cast<size_t>(button)].pressed;
}

bool Mouse::is_button_released(MouseButton button) const {
  return m_button_states[static_cast<size_t>(button)].released;
}

Mouse::~Mouse() {}

} // namespace astralix::input
