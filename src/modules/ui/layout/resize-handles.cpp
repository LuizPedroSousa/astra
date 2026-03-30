#include "layout/resize-handles.hpp"

#include "layout/common.hpp"

#include <algorithm>

namespace astralix::ui {

UIRect resize_handle_rect(const UIDocument::UINode &node, UIHitPart part) {
  const UIRect bounds = node.layout.bounds;
  const float thickness = std::max(1.0f, node.style.resize_handle_thickness);
  const float corner_extent =
      std::max(thickness, node.style.resize_corner_extent);

  switch (part) {
    case UIHitPart::ResizeLeft:
      return UIRect{
          .x = bounds.x,
          .y = bounds.y,
          .width = thickness,
          .height = bounds.height,
      };
    case UIHitPart::ResizeTop:
      return UIRect{
          .x = bounds.x,
          .y = bounds.y,
          .width = bounds.width,
          .height = thickness,
      };
    case UIHitPart::ResizeRight:
      return UIRect{
          .x = bounds.right() - thickness,
          .y = bounds.y,
          .width = thickness,
          .height = bounds.height,
      };
    case UIHitPart::ResizeBottom:
      return UIRect{
          .x = bounds.x,
          .y = bounds.bottom() - thickness,
          .width = bounds.width,
          .height = thickness,
      };
    case UIHitPart::ResizeTopLeft:
      return UIRect{
          .x = bounds.x,
          .y = bounds.y,
          .width = corner_extent,
          .height = corner_extent,
      };
    case UIHitPart::ResizeTopRight:
      return UIRect{
          .x = bounds.right() - corner_extent,
          .y = bounds.y,
          .width = corner_extent,
          .height = corner_extent,
      };
    case UIHitPart::ResizeBottomLeft:
      return UIRect{
          .x = bounds.x,
          .y = bounds.bottom() - corner_extent,
          .width = corner_extent,
          .height = corner_extent,
      };
    case UIHitPart::ResizeBottomRight:
      return UIRect{
          .x = bounds.right() - corner_extent,
          .y = bounds.bottom() - corner_extent,
          .width = corner_extent,
          .height = corner_extent,
      };
    default:
      return UIRect{};
  }
}

std::optional<UIHitPart>
hit_test_resize_handles(const UIDocument::UINode &node, glm::vec2 point) {
  if (!node_supports_panel_resize(node)) {
    return std::nullopt;
  }

  const bool allow_horizontal =
      resize_allows_horizontal(node.style.resize_mode);
  const bool allow_vertical = resize_allows_vertical(node.style.resize_mode);
  const bool left_enabled =
      allow_horizontal &&
      has_resize_edge(node.style.resize_edges, k_resize_edge_left);
  const bool top_enabled =
      allow_vertical &&
      has_resize_edge(node.style.resize_edges, k_resize_edge_top);
  const bool right_enabled =
      allow_horizontal &&
      has_resize_edge(node.style.resize_edges, k_resize_edge_right);
  const bool bottom_enabled =
      allow_vertical &&
      has_resize_edge(node.style.resize_edges, k_resize_edge_bottom);

  if (left_enabled && top_enabled &&
      resize_handle_rect(node, UIHitPart::ResizeTopLeft).contains(point)) {
    return UIHitPart::ResizeTopLeft;
  }
  if (right_enabled && top_enabled &&
      resize_handle_rect(node, UIHitPart::ResizeTopRight).contains(point)) {
    return UIHitPart::ResizeTopRight;
  }
  if (left_enabled && bottom_enabled &&
      resize_handle_rect(node, UIHitPart::ResizeBottomLeft).contains(point)) {
    return UIHitPart::ResizeBottomLeft;
  }
  if (right_enabled && bottom_enabled &&
      resize_handle_rect(node, UIHitPart::ResizeBottomRight).contains(point)) {
    return UIHitPart::ResizeBottomRight;
  }
  if (left_enabled &&
      resize_handle_rect(node, UIHitPart::ResizeLeft).contains(point)) {
    return UIHitPart::ResizeLeft;
  }
  if (top_enabled &&
      resize_handle_rect(node, UIHitPart::ResizeTop).contains(point)) {
    return UIHitPart::ResizeTop;
  }
  if (right_enabled &&
      resize_handle_rect(node, UIHitPart::ResizeRight).contains(point)) {
    return UIHitPart::ResizeRight;
  }
  if (bottom_enabled &&
      resize_handle_rect(node, UIHitPart::ResizeBottom).contains(point)) {
    return UIHitPart::ResizeBottom;
  }

  return std::nullopt;
}

void append_resize_handle_commands(
    UIDocument &document,
    UINodeId node_id,
    const UIDocument::UINode &node,
    const UIResolvedStyle &resolved
) {
  if (!node_supports_panel_resize(node)) {
    return;
  }

  const UIHitPart part = node.layout.resize_active_part != UIHitPart::Body
                             ? node.layout.resize_active_part
                             : node.layout.resize_hovered_part;
  if (!is_panel_resize_part(part)) {
    return;
  }

  UIDrawCommand command;
  command.type = DrawCommandType::Rect;
  command.node_id = node_id;
  command.rect = resize_handle_rect(node, part);
  apply_self_clip(command, node);
  command.color = (node.layout.resize_active_part == part
                       ? node.style.resize_handle_active_color
                       : node.style.resize_handle_hovered_color) *
                  glm::vec4(1.0f, 1.0f, 1.0f, resolved.opacity);
  command.border_radius = is_corner_resize_part(part) ? 4.0f : 2.0f;
  document.draw_list().commands.push_back(std::move(command));
}

} // namespace astralix::ui
