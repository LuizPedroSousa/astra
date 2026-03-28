#pragma once

#include "document.hpp"
#include <algorithm>
#include <cmath>
#include <string_view>

namespace astralix::ui {

inline bool scrolls_horizontally(ScrollMode mode) {
  return mode == ScrollMode::Horizontal || mode == ScrollMode::Both;
}

inline bool scrolls_vertically(ScrollMode mode) {
  return mode == ScrollMode::Vertical || mode == ScrollMode::Both;
}

inline bool resize_allows_horizontal(ResizeMode mode) {
  return mode == ResizeMode::Horizontal || mode == ResizeMode::Both;
}

inline bool resize_allows_vertical(ResizeMode mode) {
  return mode == ResizeMode::Vertical || mode == ResizeMode::Both;
}

inline bool has_resize_edge(uint8_t mask, uint8_t edge) {
  return (mask & edge) != 0u;
}

inline glm::vec2 clamp_scroll_offset(glm::vec2 offset, glm::vec2 max_offset,
                                     ScrollMode mode) {
  glm::vec2 clamped = offset;

  if (scrolls_horizontally(mode)) {
    clamped.x = std::clamp(clamped.x, 0.0f, std::max(0.0f, max_offset.x));
  } else {
    clamped.x = 0.0f;
  }

  if (scrolls_vertically(mode)) {
    clamped.y = std::clamp(clamped.y, 0.0f, std::max(0.0f, max_offset.y));
  } else {
    clamped.y = 0.0f;
  }

  return clamped;
}

inline void apply_state_style(UIResolvedStyle &resolved,
                              const UiStateStyle &state_style) {
  if (state_style.background_color.has_value()) {
    resolved.background_color = *state_style.background_color;
  }

  if (state_style.border_color.has_value()) {
    resolved.border_color = *state_style.border_color;
  }

  if (state_style.border_width.has_value()) {
    resolved.border_width = *state_style.border_width;
  }

  if (state_style.border_radius.has_value()) {
    resolved.border_radius = *state_style.border_radius;
  }

  if (state_style.opacity.has_value()) {
    resolved.opacity = *state_style.opacity;
  }

  if (state_style.tint.has_value()) {
    resolved.tint = *state_style.tint;
  }

  if (state_style.text_color.has_value()) {
    resolved.text_color = *state_style.text_color;
  }
}

inline UIResolvedStyle resolve_style(const UIStyle &style,
                                     const UIPaintState &paint_state,
                                     bool enabled) {
  UIResolvedStyle resolved{
      .background_color = style.background_color,
      .border_color = style.border_color,
      .border_width = style.border_width,
      .border_radius = style.border_radius,
      .opacity = style.opacity,
      .tint = style.tint,
      .text_color = style.text_color,
      .placeholder_text_color = style.placeholder_text_color,
      .selection_color = style.selection_color,
      .caret_color = style.caret_color,
  };

  if (paint_state.focused) {
    apply_state_style(resolved, style.focused_style);
  }

  if (paint_state.hovered) {
    apply_state_style(resolved, style.hovered_style);
  }

  if (paint_state.pressed) {
    apply_state_style(resolved, style.pressed_style);
  }

  if (!enabled) {
    apply_state_style(resolved, style.disabled_style);
  }

  return resolved;
}

template <typename AdvanceFn>
inline float measure_text_prefix_advance(std::string_view text, size_t index,
                                         AdvanceFn &&advance_fn) {
  const size_t clamped_index = std::min(index, text.size());

  float width = 0.0f;
  for (size_t i = 0; i < clamped_index; ++i) {
    width += advance_fn(text[i]);
  }

  return width;
}

template <typename AdvanceFn>
inline size_t nearest_text_index(std::string_view text, float x,
                                 AdvanceFn &&advance_fn) {
  if (x <= 0.0f) {
    return 0u;
  }

  float cursor = 0.0f;
  for (size_t i = 0; i < text.size(); ++i) {
    const float advance = advance_fn(text[i]);
    const float midpoint = cursor + advance * 0.5f;
    if (x < midpoint) {
      return i;
    }

    cursor += advance;
  }

  return text.size();
}

inline bool is_supported_text_codepoint(uint32_t codepoint) {
  return codepoint < 128u && codepoint >= 32u;
}

inline size_t clamp_text_index(std::string_view text, size_t index) {
  return std::min(index, text.size());
}

inline UITextSelection clamp_text_selection(std::string_view text,
                                            UITextSelection selection) {
  selection.anchor = clamp_text_index(text, selection.anchor);
  selection.focus = clamp_text_index(text, selection.focus);
  return selection;
}

inline std::string sanitize_ascii_text(std::string_view text) {
  std::string sanitized;
  sanitized.reserve(text.size());

  for (const unsigned char byte : text) {
    if (byte >= 32u && byte < 128u) {
      sanitized.push_back(static_cast<char>(byte));
    }
  }

  return sanitized;
}

inline bool is_scrollbar_thumb_part(UIHitPart part) {
  return part == UIHitPart::VerticalScrollbarThumb ||
         part == UIHitPart::HorizontalScrollbarThumb;
}

inline bool is_scrollbar_track_part(UIHitPart part) {
  return part == UIHitPart::VerticalScrollbarTrack ||
         part == UIHitPart::HorizontalScrollbarTrack;
}

inline bool is_scrollbar_part(UIHitPart part) {
  return is_scrollbar_thumb_part(part) || is_scrollbar_track_part(part);
}

inline bool is_slider_part(UIHitPart part) {
  return part == UIHitPart::SliderTrack || part == UIHitPart::SliderThumb;
}

inline bool is_select_part(UIHitPart part) {
  return part == UIHitPart::SelectField || part == UIHitPart::SelectOption;
}

inline bool resize_part_moves_left_edge(UIHitPart part) {
  return part == UIHitPart::ResizeLeft ||
         part == UIHitPart::ResizeTopLeft ||
         part == UIHitPart::ResizeBottomLeft;
}

inline bool resize_part_moves_right_edge(UIHitPart part) {
  return part == UIHitPart::ResizeRight ||
         part == UIHitPart::ResizeTopRight ||
         part == UIHitPart::ResizeBottomRight;
}

inline bool resize_part_moves_top_edge(UIHitPart part) {
  return part == UIHitPart::ResizeTop ||
         part == UIHitPart::ResizeTopLeft ||
         part == UIHitPart::ResizeTopRight;
}

inline bool resize_part_moves_bottom_edge(UIHitPart part) {
  return part == UIHitPart::ResizeBottom ||
         part == UIHitPart::ResizeBottomLeft ||
         part == UIHitPart::ResizeBottomRight;
}

inline bool is_panel_resize_part(UIHitPart part) {
  switch (part) {
    case UIHitPart::ResizeLeft:
    case UIHitPart::ResizeTop:
    case UIHitPart::ResizeRight:
    case UIHitPart::ResizeBottom:
    case UIHitPart::ResizeTopLeft:
    case UIHitPart::ResizeTopRight:
    case UIHitPart::ResizeBottomLeft:
    case UIHitPart::ResizeBottomRight:
      return true;
    default:
      return false;
  }
}

inline bool is_corner_resize_part(UIHitPart part) {
  return part == UIHitPart::ResizeTopLeft ||
         part == UIHitPart::ResizeTopRight ||
         part == UIHitPart::ResizeBottomLeft ||
         part == UIHitPart::ResizeBottomRight;
}

inline bool is_splitter_part(UIHitPart part) {
  return part == UIHitPart::SplitterBar;
}

inline bool is_resize_part(UIHitPart part) {
  return is_panel_resize_part(part) || is_splitter_part(part);
}

inline float clamp_resize_extent(float value, float min_value, float max_value,
                                 float available_space) {
  const float clamped_max =
      std::max(0.0f, std::min(max_value, available_space));
  if (clamped_max < min_value) {
    return clamped_max;
  }

  return std::clamp(value, min_value, clamped_max);
}

inline UiRect clamp_panel_resize_bounds(const UiRect &start_bounds,
                                        UiRect next_bounds,
                                        const UiRect &parent_bounds,
                                        UIHitPart part, float min_width,
                                        float max_width, float min_height,
                                        float max_height) {
  if (resize_part_moves_left_edge(part)) {
    const float anchored_right = std::clamp(
        start_bounds.right(), parent_bounds.x, parent_bounds.right()
    );
    const float available_width =
        std::max(0.0f, anchored_right - parent_bounds.x);
    next_bounds.width = clamp_resize_extent(
        next_bounds.width, min_width, max_width, available_width
    );
    next_bounds.x = anchored_right - next_bounds.width;
  } else if (resize_part_moves_right_edge(part)) {
    const float anchored_left = std::clamp(
        start_bounds.x, parent_bounds.x, parent_bounds.right()
    );
    const float available_width =
        std::max(0.0f, parent_bounds.right() - anchored_left);
    next_bounds.width = clamp_resize_extent(
        next_bounds.width, min_width, max_width, available_width
    );
    next_bounds.x = anchored_left;
  }

  if (resize_part_moves_top_edge(part)) {
    const float anchored_bottom = std::clamp(
        start_bounds.bottom(), parent_bounds.y, parent_bounds.bottom()
    );
    const float available_height =
        std::max(0.0f, anchored_bottom - parent_bounds.y);
    next_bounds.height = clamp_resize_extent(
        next_bounds.height, min_height, max_height, available_height
    );
    next_bounds.y = anchored_bottom - next_bounds.height;
  } else if (resize_part_moves_bottom_edge(part)) {
    const float anchored_top = std::clamp(
        start_bounds.y, parent_bounds.y, parent_bounds.bottom()
    );
    const float available_height =
        std::max(0.0f, parent_bounds.bottom() - anchored_top);
    next_bounds.height = clamp_resize_extent(
        next_bounds.height, min_height, max_height, available_height
    );
    next_bounds.y = anchored_top;
  }

  return next_bounds;
}

inline UiRect clamp_rect_to_bounds(UiRect rect, const UiRect &bounds) {
  rect.width = std::clamp(rect.width, 0.0f, std::max(0.0f, bounds.width));
  rect.height = std::clamp(rect.height, 0.0f, std::max(0.0f, bounds.height));

  const float max_x = std::max(bounds.x, bounds.right() - rect.width);
  const float max_y = std::max(bounds.y, bounds.bottom() - rect.height);
  rect.x = std::clamp(rect.x, bounds.x, max_x);
  rect.y = std::clamp(rect.y, bounds.y, max_y);
  return rect;
}

inline bool node_supports_panel_drag(const UIDocument::UINode &node) {
  return (node.type == NodeType::View || node.type == NodeType::ScrollView) &&
         node.style.position_type == PositionType::Absolute &&
         node.style.draggable;
}

inline bool node_is_drag_handle(const UIDocument::UINode &node) {
  return node.style.drag_handle;
}

inline bool node_supports_panel_resize(const UIDocument::UINode &node) {
  return (node.type == NodeType::View || node.type == NodeType::ScrollView) &&
         node.style.position_type == PositionType::Absolute &&
         node.style.resize_mode != ResizeMode::None &&
         node.style.resize_edges != k_resize_edge_none;
}

inline float clamp_text_scroll_x(float scroll_x, float content_width,
                                 float viewport_width) {
  return std::clamp(scroll_x, 0.0f,
                    std::max(0.0f, content_width - viewport_width));
}

inline float scroll_x_to_keep_range_visible(float current_scroll_x,
                                            float range_start_x,
                                            float range_end_x,
                                            float content_width,
                                            float viewport_width) {
  float next_scroll_x = current_scroll_x;
  if (range_start_x < next_scroll_x) {
    next_scroll_x = range_start_x;
  } else if (range_end_x > next_scroll_x + viewport_width) {
    next_scroll_x = range_end_x - viewport_width;
  }

  return clamp_text_scroll_x(next_scroll_x, content_width, viewport_width);
}

inline bool node_chain_allows_interaction(const UIDocument &document,
                                          UINodeId node_id) {
  UINodeId current = node_id;

  while (current != k_invalid_node_id) {
    const auto *node = document.node(current);
    if (node == nullptr || !node->visible || !node->enabled) {
      return false;
    }

    current = node->parent;
  }

  return true;
}

inline std::optional<UINodeId>
find_nearest_focusable_ancestor(const UIDocument &document, UINodeId node_id) {
  UINodeId current = node_id;

  while (current != k_invalid_node_id) {
    const auto *node = document.node(current);
    if (node == nullptr) {
      return std::nullopt;
    }

    if (node->focusable && node_chain_allows_interaction(document, current)) {
      return current;
    }

    current = node->parent;
  }

  return std::nullopt;
}

inline std::optional<UINodeId>
find_nearest_pressable_ancestor(const UIDocument &document, UINodeId node_id) {
  UINodeId current = node_id;

  while (current != k_invalid_node_id) {
    const auto *node = document.node(current);
    if (node == nullptr) {
      return std::nullopt;
    }

    if (node->type == NodeType::Pressable &&
        node_chain_allows_interaction(document, current)) {
      return current;
    }

    current = node->parent;
  }

  return std::nullopt;
}

inline std::optional<UINodeId>
find_nearest_scrollable_ancestor(const UIDocument &document, UINodeId node_id) {
  UINodeId current = node_id;

  while (current != k_invalid_node_id) {
    const auto *node = document.node(current);
    if (node == nullptr) {
      return std::nullopt;
    }

    if (node->style.scroll_mode != ScrollMode::None &&
        node_chain_allows_interaction(document, current)) {
      return current;
    }

    current = node->parent;
  }

  return std::nullopt;
}

inline std::vector<UINodeId>
collect_focusable_order(const UIDocument &document) {
  std::vector<UINodeId> order;

  for (UINodeId node_id : document.root_to_leaf_order()) {
    const auto *node = document.node(node_id);
    if (node == nullptr || !node->focusable) {
      continue;
    }

    if (!node_chain_allows_interaction(document, node_id)) {
      continue;
    }

    order.push_back(node_id);
  }

  return order;
}

} // namespace astralix::ui
