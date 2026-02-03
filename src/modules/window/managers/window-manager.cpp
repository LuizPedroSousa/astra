#include "managers/project-manager.hpp"
#include "window.hpp"

#include "assert.hpp"
#include "log.hpp"
#include "window-manager.hpp"

namespace astralix {
static int count = 0;

#define WINDOW_MANAGER_SUGGESTION_NAME "WindowManager"

Ref<Window> WindowManager::load_window(Ref<Window> window) {
  auto window_id = window->id();

  auto inserted_window = m_window_table.emplace(window_id, std::move(window));

  ASTRA_ENSURE(!inserted_window.second, "can't insert window");

  return m_window_table[window_id];
}

Ref<Window> WindowManager::active_window() const noexcept {
  return m_active_window;
}

void WindowManager::load_windows(std::initializer_list<Ref<Window>> windows) {
  for (auto window : windows) {
    load_window(window);
  }
}

Ref<Window> WindowManager::get_window_by_id(WindowID id) {
  auto it = m_window_table.find(id);

  ASTRA_ENSURE_WITH_SUGGESTIONS(it == m_window_table.end(), m_window_table, id,
                                "Window", WINDOW_MANAGER_SUGGESTION_NAME);

  return it->second;
}

void WindowManager::set_active_window_by_id(WindowID id) {
  auto window = get_window_by_id(id);

  m_active_window = window;
}

void WindowManager::start() {
  auto project_config = active_project()->get_config();


  for (auto window : project_config.windows) {
  auto created_win  =  load_window(Window::create(window.id, window.title, window.width,
                                   window.height, window.headless));
    if(m_active_window == nullptr){
      m_active_window = created_win;
    }

  }

  for (auto &[_, window] : m_window_table) {
    window->start();
  }
};

void WindowManager::update() {
  for (auto &[_, window] : m_window_table) {
    window->update();
  }
};

void WindowManager::swap() {
  for (auto &[_, window] : m_window_table) {
    window->swap();
  }
};

void WindowManager::end() {};

void WindowManager::resize(int width, int height) {};

bool WindowManager::is_open() {
  for (auto &[_, window] : m_window_table) {
    if (window->is_open()) {
      return true;
    }
  }

  return false;
};
} // namespace astralix
