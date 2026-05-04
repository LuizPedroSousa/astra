#pragma once

#include "base-manager.hpp"
#include "types.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>

namespace astralix::editor {

enum class EditorViewportNavigationAction : uint8_t {
  Front = 0,
  Back = 1,
  Right = 2,
  Left = 3,
  Top = 4,
  Bottom = 5,
  ToggleProjection = 6,
};

class EditorViewportNavigationStore
    : public BaseManager<EditorViewportNavigationStore> {
public:
  void request_action(EditorViewportNavigationAction action) {
    m_pending_action = action;
    ++m_revision;
  }

  std::optional<EditorViewportNavigationAction> consume_action_request() {
    auto request = m_pending_action;
    m_pending_action.reset();
    return request;
  }

  std::optional<ui::UIRect> draw_rect() const { return m_draw_rect; }

  void set_draw_rect(std::optional<ui::UIRect> rect) {
    rect = sanitize_rect(rect);
    if (same_rect(m_draw_rect, rect)) {
      return;
    }

    m_draw_rect = rect;
    ++m_revision;
  }

  uint64_t revision() const { return m_revision; }

private:
  static std::optional<ui::UIRect> sanitize_rect(
      std::optional<ui::UIRect> rect
  ) {
    if (!rect.has_value()) {
      return std::nullopt;
    }

    rect->x = std::max(0.0f, rect->x);
    rect->y = std::max(0.0f, rect->y);
    rect->width = std::max(0.0f, rect->width);
    rect->height = std::max(0.0f, rect->height);
    if (rect->width <= 1.0f || rect->height <= 1.0f) {
      return std::nullopt;
    }

    return rect;
  }

  static bool same_rect(
      const std::optional<ui::UIRect> &lhs,
      const std::optional<ui::UIRect> &rhs
  ) {
    if (lhs.has_value() != rhs.has_value()) {
      return false;
    }

    if (!lhs.has_value()) {
      return true;
    }

    return lhs->x == rhs->x && lhs->y == rhs->y &&
           lhs->width == rhs->width && lhs->height == rhs->height;
  }

  std::optional<EditorViewportNavigationAction> m_pending_action;
  std::optional<ui::UIRect> m_draw_rect;
  uint64_t m_revision = 1u;
};

inline Ref<EditorViewportNavigationStore> editor_viewport_navigation_store() {
  if (EditorViewportNavigationStore::get() == nullptr) {
    EditorViewportNavigationStore::init();
  }

  return EditorViewportNavigationStore::get();
}

} // namespace astralix::editor
