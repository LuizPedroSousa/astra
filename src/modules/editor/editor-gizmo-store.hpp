#pragma once

#include "base-manager.hpp"
#include "types.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace astralix::editor {

enum class EditorGizmoMode : uint8_t {
  Translate = 0,
  Rotate = 1,
  Scale = 2,
};

enum class EditorGizmoHandle : uint8_t {
  None = 0,
  TranslateX,
  TranslateY,
  TranslateZ,
  RotateX,
  RotateY,
  RotateZ,
  ScaleX,
  ScaleY,
  ScaleZ,
};

class EditorGizmoStore : public BaseManager<EditorGizmoStore> {
public:
  EditorGizmoMode mode() const { return m_mode; }

  void set_mode(EditorGizmoMode mode) {
    if (m_mode == mode) {
      return;
    }

    m_mode = mode;
    ++m_revision;
  }

  std::optional<EditorGizmoHandle> hovered_handle() const {
    return m_hovered_handle;
  }

  void set_hovered_handle(std::optional<EditorGizmoHandle> handle) {
    if (same_handle(m_hovered_handle, handle)) {
      return;
    }

    m_hovered_handle = sanitize_handle(handle);
    ++m_revision;
  }

  std::optional<EditorGizmoHandle> active_handle() const {
    return m_active_handle;
  }

  void set_active_handle(std::optional<EditorGizmoHandle> handle) {
    if (same_handle(m_active_handle, handle)) {
      return;
    }

    m_active_handle = sanitize_handle(handle);
    ++m_revision;
  }

  std::optional<ui::UIRect> panel_rect() const { return m_panel_rect; }

  void set_panel_rect(std::optional<ui::UIRect> rect) {
    rect = sanitize_rect(rect);
    if (same_rect(m_panel_rect, rect)) {
      return;
    }

    m_panel_rect = rect;
    ++m_revision;
  }

  std::optional<ui::UIRect> window_rect() const { return m_window_rect; }

  void set_window_rect(std::optional<ui::UIRect> rect) {
    rect = sanitize_rect(rect);
    if (same_rect(m_window_rect, rect)) {
      return;
    }

    m_window_rect = rect;
    ++m_revision;
  }

  bool window_capture_enabled() const { return m_window_capture_enabled; }

  void set_window_capture_enabled(bool enabled) {
    if (m_window_capture_enabled == enabled) {
      return;
    }

    m_window_capture_enabled = enabled;
    ++m_revision;
  }

  const std::vector<ui::UIRect> &blocked_rects() const {
    return m_blocked_rects;
  }

  void set_blocked_rects(std::vector<ui::UIRect> rects) {
    sanitize_rects(rects);
    if (same_rects(m_blocked_rects, rects)) {
      return;
    }

    m_blocked_rects = std::move(rects);
    ++m_revision;
  }

  std::optional<ui::UIRect> interaction_rect() const {
    return m_window_capture_enabled ? m_window_rect : m_panel_rect;
  }

  const std::vector<ui::UIRect> &interaction_blocked_rects() const {
    static const std::vector<ui::UIRect> k_empty_rects;
    return m_window_capture_enabled ? m_blocked_rects : k_empty_rects;
  }

  bool point_blocked(glm::vec2 point) const {
    if (!m_window_capture_enabled) {
      return false;
    }

    for (const auto &rect : m_blocked_rects) {
      if (rect.contains(point)) {
        return true;
      }
    }

    return false;
  }

  bool point_in_interaction_region(glm::vec2 point) const {
    const auto rect = interaction_rect();
    return rect.has_value() && rect->contains(point) && !point_blocked(point);
  }

  void clear_hover_and_active_handles() {
    const auto previous_revision = m_revision;
    set_hovered_handle(std::nullopt);
    set_active_handle(std::nullopt);
    if (m_revision == previous_revision) {
      return;
    }
  }

  void clear_capture_regions() {
    const auto previous_revision = m_revision;
    set_panel_rect(std::nullopt);
    set_window_rect(std::nullopt);
    set_window_capture_enabled(false);
    set_blocked_rects({});
    if (m_revision == previous_revision) {
      return;
    }
  }

  void clear_interaction() {
    const auto previous_revision = m_revision;
    clear_hover_and_active_handles();
    clear_capture_regions();
    if (m_revision == previous_revision) {
      return;
    }
  }

  uint64_t revision() const { return m_revision; }

private:
  static bool same_handle(
      const std::optional<EditorGizmoHandle> &lhs,
      const std::optional<EditorGizmoHandle> &rhs
  ) {
    if (lhs.has_value() != rhs.has_value()) {
      return false;
    }

    if (!lhs.has_value()) {
      return true;
    }

    return *lhs == *rhs;
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

  static bool same_rects(
      const std::vector<ui::UIRect> &lhs,
      const std::vector<ui::UIRect> &rhs
  ) {
    if (lhs.size() != rhs.size()) {
      return false;
    }

    for (size_t index = 0; index < lhs.size(); ++index) {
      if (lhs[index].x != rhs[index].x || lhs[index].y != rhs[index].y ||
          lhs[index].width != rhs[index].width ||
          lhs[index].height != rhs[index].height) {
        return false;
      }
    }

    return true;
  }

  static std::optional<EditorGizmoHandle>
  sanitize_handle(std::optional<EditorGizmoHandle> handle) {
    if (handle.has_value() && *handle == EditorGizmoHandle::None) {
      return std::nullopt;
    }

    return handle;
  }

  static std::optional<ui::UIRect> sanitize_rect(std::optional<ui::UIRect> rect) {
    if (!rect.has_value() || rect->width <= 0.0f || rect->height <= 0.0f) {
      return std::nullopt;
    }

    return rect;
  }

  static void sanitize_rects(std::vector<ui::UIRect> &rects) {
    rects.erase(
        std::remove_if(
            rects.begin(),
            rects.end(),
            [](const ui::UIRect &rect) {
              return rect.width <= 0.0f || rect.height <= 0.0f;
            }
        ),
        rects.end()
    );
  }

  EditorGizmoMode m_mode = EditorGizmoMode::Translate;
  std::optional<EditorGizmoHandle> m_hovered_handle;
  std::optional<EditorGizmoHandle> m_active_handle;
  std::optional<ui::UIRect> m_panel_rect;
  std::optional<ui::UIRect> m_window_rect;
  std::vector<ui::UIRect> m_blocked_rects;
  bool m_window_capture_enabled = false;
  uint64_t m_revision = 0u;
};

inline Ref<EditorGizmoStore> editor_gizmo_store() {
  if (EditorGizmoStore::get() == nullptr) {
    EditorGizmoStore::init();
  }

  return EditorGizmoStore::get();
}

} // namespace astralix::editor
