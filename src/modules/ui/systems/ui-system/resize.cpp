#include "systems/ui-system/resize.hpp"

#include "foundations.hpp"
#include "systems/ui-system/core.hpp"
#include "window.hpp"
#include <algorithm>

namespace astralix::ui_system_core {
namespace {

void clear_resize_visual_state(const RootEntry &entry) {
  if (entry.document == nullptr) {
    return;
  }

  for (ui::UINodeId node_id : entry.document->root_to_leaf_order()) {
    auto *node = entry.document->node(node_id);
    if (node == nullptr || !ui::node_supports_panel_resize(*node)) {
      continue;
    }

    node->layout.resize_hovered_part = ui::UIHitPart::Body;
    node->layout.resize_active_part = ui::UIHitPart::Body;
  }
}

} // namespace

CursorIcon cursor_icon_for_hit_part(const ui::UIDocument &document, ui::UINodeId node_id, ui::UIHitPart part) {
  switch (part) {
    case ui::UIHitPart::ResizeLeft:
    case ui::UIHitPart::ResizeRight:
      return CursorIcon::ResizeHorizontal;
    case ui::UIHitPart::ResizeTop:
    case ui::UIHitPart::ResizeBottom:
      return CursorIcon::ResizeVertical;
    case ui::UIHitPart::ResizeTopLeft:
    case ui::UIHitPart::ResizeBottomRight:
      return CursorIcon::ResizeDiagonalNwSe;
    case ui::UIHitPart::ResizeTopRight:
    case ui::UIHitPart::ResizeBottomLeft:
      return CursorIcon::ResizeDiagonalNeSw;
    case ui::UIHitPart::SplitterBar: {
      const auto *node = document.node(node_id);
      const auto *parent =
          node != nullptr ? document.node(node->parent) : nullptr;
      const ui::FlexDirection direction = parent != nullptr
                                              ? parent->style.flex_direction
                                              : ui::FlexDirection::Row;
      return direction == ui::FlexDirection::Row ? CursorIcon::ResizeHorizontal
                                                 : CursorIcon::ResizeVertical;
    }
    default:
      return CursorIcon::Default;
  }
}

void apply_resize_visual_state(
    const std::vector<RootEntry> &roots,
    const std::optional<PointerHit> &hover_hit,
    const std::optional<std::pair<Target, ui::UIHitPart>> &active_hit
) {
  for (const RootEntry &entry : roots) {
    clear_resize_visual_state(entry);
  }

  if (hover_hit.has_value() && hover_hit->target.document != nullptr &&
      ui::is_panel_resize_part(hover_hit->part)) {
    if (auto *node =
            hover_hit->target.document->node(hover_hit->target.node_id);
        node != nullptr) {
      node->layout.resize_hovered_part = hover_hit->part;
    }
  }

  if (active_hit.has_value() && active_hit->first.document != nullptr &&
      ui::is_panel_resize_part(active_hit->second)) {
    if (auto *node =
            active_hit->first.document->node(active_hit->first.node_id);
        node != nullptr) {
      node->layout.resize_hovered_part = active_hit->second;
      node->layout.resize_active_part = active_hit->second;
    }
  }
}

void update_panel_move_drag(const UISystem::PanelMoveDrag &drag, glm::vec2 pointer) {
  if (drag.panel_target.document == nullptr) {
    return;
  }

  const auto *node = drag.panel_target.document->node(drag.panel_target.node_id);
  if (node == nullptr || !ui::node_supports_panel_drag(*node)) {
    return;
  }

  const ui::UIRect parent_bounds =
      parent_content_bounds(*drag.panel_target.document, drag.panel_target.node_id);
  const glm::vec2 delta = pointer - drag.start_pointer;

  ui::UIRect next_bounds = drag.start_bounds;
  next_bounds.x = drag.start_bounds.x + delta.x;
  next_bounds.y = drag.start_bounds.y + delta.y;
  next_bounds = ui::clamp_rect_to_bounds(next_bounds, parent_bounds);

  write_absolute_bounds_to_style(drag.panel_target, parent_bounds, next_bounds);
}

void update_panel_resize_drag(const UISystem::PanelResizeDrag &drag, glm::vec2 pointer) {
  if (drag.target.document == nullptr) {
    return;
  }

  const auto *node = drag.target.document->node(drag.target.node_id);
  if (node == nullptr || !ui::node_supports_panel_resize(*node)) {
    return;
  }

  const ui::UIRect parent_bounds =
      parent_content_bounds(*drag.target.document, drag.target.node_id);
  const glm::vec2 delta = pointer - drag.start_pointer;

  ui::UIRect next_bounds = drag.start_bounds;
  if (resize_part_moves_left_edge(drag.part)) {
    next_bounds.x = drag.start_bounds.x + delta.x;
    next_bounds.width = drag.start_bounds.width - delta.x;
  }
  if (resize_part_moves_right_edge(drag.part)) {
    next_bounds.width = drag.start_bounds.width + delta.x;
  }
  if (resize_part_moves_top_edge(drag.part)) {
    next_bounds.y = drag.start_bounds.y + delta.y;
    next_bounds.height = drag.start_bounds.height - delta.y;
  }
  if (resize_part_moves_bottom_edge(drag.part)) {
    next_bounds.height = drag.start_bounds.height + delta.y;
  }

  const float min_width =
      resolve_min_length(
          node->style.min_width,
          parent_bounds.width,
          drag.target.document != nullptr ? drag.target.document->root_font_size()
                                          : 16.0f,
          node->layout.measured_size.x
      );
  const float max_width =
      resolve_max_length(
          node->style.max_width,
          parent_bounds.width,
          drag.target.document != nullptr ? drag.target.document->root_font_size()
                                          : 16.0f,
          node->layout.measured_size.x
      );
  const float min_height =
      resolve_min_length(
          node->style.min_height,
          parent_bounds.height,
          drag.target.document != nullptr ? drag.target.document->root_font_size()
                                          : 16.0f,
          node->layout.measured_size.y
      );
  const float max_height =
      resolve_max_length(
          node->style.max_height,
          parent_bounds.height,
          drag.target.document != nullptr ? drag.target.document->root_font_size()
                                          : 16.0f,
          node->layout.measured_size.y
      );

  next_bounds = ui::clamp_panel_resize_bounds(
      drag.start_bounds, next_bounds, parent_bounds, drag.part, min_width, max_width, min_height, max_height
  );

  write_absolute_bounds_to_style(drag.target, parent_bounds, next_bounds);
}

} // namespace astralix::ui_system_core
