#include "layout/widgets/layout/scroll-view.hpp"

#include "layout/common.hpp"

#include <algorithm>

namespace astralix::ui {

void reset_scrollbar_geometry(UIScrollState &scroll) {
  scroll.vertical_scrollbar_visible = false;
  scroll.horizontal_scrollbar_visible = false;
  scroll.vertical_track_rect = UIRect{};
  scroll.vertical_thumb_rect = UIRect{};
  scroll.horizontal_track_rect = UIRect{};
  scroll.horizontal_thumb_rect = UIRect{};
  scroll.vertical_thumb_hovered = false;
  scroll.vertical_thumb_active = false;
  scroll.horizontal_thumb_hovered = false;
  scroll.horizontal_thumb_active = false;
}

void update_scrollbar_geometry(UIDocument::UINode &node) {
  auto &scroll = node.layout.scroll;
  reset_scrollbar_geometry(scroll);

  if (node.type != NodeType::ScrollView ||
      node.style.scrollbar_visibility == ScrollbarVisibility::Hidden) {
    return;
  }

  const float thickness = std::max(0.0f, node.style.scrollbar_thickness);
  if (thickness <= 0.0f) {
    return;
  }

  const bool wants_vertical = scrolls_vertically(node.style.scroll_mode);
  const bool wants_horizontal = scrolls_horizontally(node.style.scroll_mode);

  const bool show_vertical =
      wants_vertical &&
      (node.style.scrollbar_visibility == ScrollbarVisibility::Always ||
       scroll.max_offset.y > 0.0f);
  const bool show_horizontal =
      wants_horizontal &&
      (node.style.scrollbar_visibility == ScrollbarVisibility::Always ||
       scroll.max_offset.x > 0.0f);

  const UIRect viewport = node.layout.content_bounds;

  if (show_vertical) {
    const float track_height =
        std::max(0.0f, viewport.height - (show_horizontal ? thickness : 0.0f));
    if (track_height > 0.0f) {
      scroll.vertical_scrollbar_visible = true;
      scroll.vertical_track_rect = UIRect{
          .x = viewport.right() - thickness,
          .y = viewport.y,
          .width = thickness,
          .height = track_height,
      };

      const float content_height =
          std::max(scroll.content_size.y, scroll.viewport_size.y);
      const float thumb_height = std::clamp(
          content_height > 0.0f
              ? (scroll.viewport_size.y / content_height) * track_height
              : track_height,
          std::min(track_height, node.style.scrollbar_min_thumb_length),
          track_height
      );
      const float travel = std::max(0.0f, track_height - thumb_height);
      const float ratio = scroll.max_offset.y > 0.0f
                              ? scroll.offset.y / scroll.max_offset.y
                              : 0.0f;
      scroll.vertical_thumb_rect = UIRect{
          .x = scroll.vertical_track_rect.x,
          .y = scroll.vertical_track_rect.y + ratio * travel,
          .width = thickness,
          .height = thumb_height,
      };
    }
  }

  if (show_horizontal) {
    const float track_width =
        std::max(0.0f, viewport.width - (show_vertical ? thickness : 0.0f));
    if (track_width > 0.0f) {
      scroll.horizontal_scrollbar_visible = true;
      scroll.horizontal_track_rect = UIRect{
          .x = viewport.x,
          .y = viewport.bottom() - thickness,
          .width = track_width,
          .height = thickness,
      };

      const float content_width =
          std::max(scroll.content_size.x, scroll.viewport_size.x);
      const float thumb_width = std::clamp(
          content_width > 0.0f
              ? (scroll.viewport_size.x / content_width) * track_width
              : track_width,
          std::min(track_width, node.style.scrollbar_min_thumb_length),
          track_width
      );
      const float travel = std::max(0.0f, track_width - thumb_width);
      const float ratio = scroll.max_offset.x > 0.0f
                              ? scroll.offset.x / scroll.max_offset.x
                              : 0.0f;
      scroll.horizontal_thumb_rect = UIRect{
          .x = scroll.horizontal_track_rect.x + ratio * travel,
          .y = scroll.horizontal_track_rect.y,
          .width = thumb_width,
          .height = thickness,
      };
    }
  }
}

void append_scrollbar_commands(
    UIDocument &document,
    UINodeId node_id,
    const UIDocument::UINode &node,
    const UIResolvedStyle &resolved
) {
  const float thickness = std::max(0.0f, node.style.scrollbar_thickness);
  if (node.type != NodeType::ScrollView || thickness <= 0.0f) {
    return;
  }

  auto append_rect = [&](const UIRect &rect, glm::vec4 color) {
    if (rect.width <= 0.0f || rect.height <= 0.0f || color.a <= 0.0f) {
      return;
    }

    UIDrawCommand command;
    command.type = DrawCommandType::Rect;
    command.node_id = node_id;
    command.rect = rect;
    apply_content_clip(command, node);
    command.color = color * glm::vec4(1.0f, 1.0f, 1.0f, resolved.opacity);
    command.border_radius = thickness * 0.5f;
    document.draw_list().commands.push_back(std::move(command));
  };

  if (node.layout.scroll.vertical_scrollbar_visible) {
    append_rect(
        node.layout.scroll.vertical_track_rect,
        node.style.scrollbar_track_color
    );
    append_rect(
        node.layout.scroll.vertical_thumb_rect,
        node.layout.scroll.vertical_thumb_active
            ? node.style.scrollbar_thumb_active_color
        : node.layout.scroll.vertical_thumb_hovered
            ? node.style.scrollbar_thumb_hovered_color
            : node.style.scrollbar_thumb_color
    );
  }

  if (node.layout.scroll.horizontal_scrollbar_visible) {
    append_rect(
        node.layout.scroll.horizontal_track_rect,
        node.style.scrollbar_track_color
    );
    append_rect(
        node.layout.scroll.horizontal_thumb_rect,
        node.layout.scroll.horizontal_thumb_active
            ? node.style.scrollbar_thumb_active_color
        : node.layout.scroll.horizontal_thumb_hovered
            ? node.style.scrollbar_thumb_hovered_color
            : node.style.scrollbar_thumb_color
    );
  }
}

std::optional<UIHitResult>
hit_test_scroll_view(const UIDocument::UINode &node, UINodeId node_id, glm::vec2 point) {
  if (node.type != NodeType::ScrollView) {
    return std::nullopt;
  }

  if (node.layout.scroll.vertical_scrollbar_visible) {
    if (node.layout.scroll.vertical_thumb_rect.contains(point)) {
      return UIHitResult{
          .node_id = node_id,
          .part = UIHitPart::VerticalScrollbarThumb,
      };
    }
    if (node.layout.scroll.vertical_track_rect.contains(point)) {
      return UIHitResult{
          .node_id = node_id,
          .part = UIHitPart::VerticalScrollbarTrack,
      };
    }
  }

  if (node.layout.scroll.horizontal_scrollbar_visible) {
    if (node.layout.scroll.horizontal_thumb_rect.contains(point)) {
      return UIHitResult{
          .node_id = node_id,
          .part = UIHitPart::HorizontalScrollbarThumb,
      };
    }
    if (node.layout.scroll.horizontal_track_rect.contains(point)) {
      return UIHitResult{
          .node_id = node_id,
          .part = UIHitPart::HorizontalScrollbarTrack,
      };
    }
  }

  return std::nullopt;
}

} // namespace astralix::ui
