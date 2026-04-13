#pragma once
#include "base-manager.hpp"
#include "events/key-codes.hpp"
#include "events/mouse.hpp"
#include "guid.hpp"
#include "window.hpp"
#include <string>
#include <unordered_map>

namespace astralix {

struct ProjectConfig;
class Window;

class WindowManager : public BaseManager<WindowManager> {
public:
  WindowManager() = default;
  ~WindowManager() = default;

  void start();
  void update();
  void swap();
  void end();
  bool is_open();

  void resize(int width, int height);

  Ref<Window> get_window_by_id(WindowID id);

  Ref<Window> active_window() const noexcept;

  void set_active_window_by_id(WindowID window_id);

  void load_windows(std::initializer_list<Ref<Window>> windows);
  Ref<Window> load_window(Ref<Window> window);

  static WindowGraphicsAPI resolve_graphics_api(
      const ProjectConfig &config, const WindowID &window_id);

private:
  std::unordered_map<WindowID, Ref<Window>> m_window_table;
  Ref<Window> m_active_window = nullptr;
};

inline Ref<WindowManager> window_manager() { return WindowManager::get(); }

#define ACTIVE_WINDOW_WIDTH() window_manager()->active_window()->width()
#define ACTIVE_WINDOW_HEIGHT() window_manager()->active_window()->height()

namespace input {
[[nodiscard]] static inline bool IS_KEY_DOWN(input::KeyCode keycode) {
  return window_manager()->active_window()->keyboard()->is_key_down(keycode);
}

[[nodiscard]] static inline bool IS_KEY_RELEASED(input::KeyCode keycode) {
  return window_manager()->active_window()->keyboard()->is_key_released(
      keycode
  );
}

[[nodiscard]] static inline const input::Mouse::Position MOUSE_DELTA() {
  return window_manager()->active_window()->mouse()->delta();
}

[[nodiscard]] static inline const input::Mouse::Position CURSOR_POSITION() {
  return window_manager()->active_window()->mouse()->position();
}

[[nodiscard]] static inline const bool HAS_MOUSE_MOVED() {
  return window_manager()->active_window()->mouse()->has_moved();
}

[[nodiscard]] static inline bool IS_MOUSE_BUTTON_DOWN(input::MouseButton button) {
  return window_manager()->active_window()->mouse()->is_button_down(button);
}

[[nodiscard]] static inline bool
IS_MOUSE_BUTTON_PRESSED(input::MouseButton button) {
  return window_manager()->active_window()->mouse()->is_button_pressed(button);
}

[[nodiscard]] static inline bool
IS_MOUSE_BUTTON_RELEASED(input::MouseButton button) {
  return window_manager()->active_window()->mouse()->is_button_released(button);
}

[[nodiscard]] static inline bool IS_CURSOR_CAPTURED() {
  return window_manager()->active_window()->cursor_captured();
}

[[nodiscard]] static inline std::string CLIPBOARD_TEXT() {
  return window_manager()->active_window()->clipboard_text();
}

static inline void SET_CLIPBOARD_TEXT(const std::string &text) {
  window_manager()->active_window()->set_clipboard_text(text);
}

inline void SET_MOUSE_POSITION(input::Mouse::Position &position) {
  return window_manager()->active_window()->mouse()->set_position(position);
}

} // namespace input

} // namespace astralix
