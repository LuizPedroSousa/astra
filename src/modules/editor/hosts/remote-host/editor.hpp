#pragma once
#include "arena.hpp"
#include "base.hpp"
#include "layouts/layout.hpp"

namespace astralix {
class Editor {
public:
  static Editor *init();
  void end();

  static Editor *get() { return m_instance; }

  inline const Ref<Layout> get_current_layout() { return m_current_layout; }

  void start();
  void run();

private:
  Editor();
  ~Editor() = default;
  Ref<Layout> m_current_layout = nullptr;
  static Editor *m_instance;

  ElasticArena m_arena;
};
} // namespace astralix
