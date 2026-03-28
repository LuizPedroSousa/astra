#include "systems/ui-system/resize.hpp"

#include "systems/ui-system/core.hpp"
#include "foundations.hpp"
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

CursorIcon cursor_icon_for_hit_part(const ui::UIDocument &document,
                                    ui::UINodeId node_id, ui::UIHitPart part) {
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
    const std::optional<std::pair<Target, ui::UIHitPart>> &active_hit) {
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

void update_panel_move_drag(const UISystem::PanelMoveDrag &drag,
                            glm::vec2 pointer) {
  if (drag.panel_target.document == nullptr) {
    return;
  }

  const auto *node = drag.panel_target.document->node(drag.panel_target.node_id);
  if (node == nullptr || !ui::node_supports_panel_drag(*node)) {
    return;
  }

  const ui::UiRect parent_bounds =
      parent_content_bounds(*drag.panel_target.document,
                            drag.panel_target.node_id);
  const glm::vec2 delta = pointer - drag.start_pointer;

  ui::UiRect next_bounds = drag.start_bounds;
  next_bounds.x = drag.start_bounds.x + delta.x;
  next_bounds.y = drag.start_bounds.y + delta.y;
  next_bounds = ui::clamp_rect_to_bounds(next_bounds, parent_bounds);

  write_absolute_bounds_to_style(drag.panel_target, parent_bounds, next_bounds);
}

std::optional<UISystem::SplitterResizeDrag>
begin_splitter_resize_drag(const Target &target, glm::vec2 pointer) {
  if (target.document == nullptr) {
    return std::nullopt;
  }

  const auto *splitter = target.document->node(target.node_id);
  if (splitter == nullptr || splitter->type != ui::NodeType::Splitter) {
    return std::nullopt;
  }

  const auto *parent = target.document->node(splitter->parent);
  if (parent == nullptr) {
    return std::nullopt;
  }

  auto find_adjacent = [&](int step) -> ui::UINodeId {
    auto it = std::find(parent->children.begin(), parent->children.end(),
                        target.node_id);
    if (it == parent->children.end()) {
      return ui::k_invalid_node_id;
    }

    int index =
        static_cast<int>(std::distance(parent->children.begin(), it)) + step;
    while (index >= 0 && index < static_cast<int>(parent->children.size())) {
      const ui::UINodeId candidate_id =
          parent->children[static_cast<size_t>(index)];
      const auto *candidate = target.document->node(candidate_id);
      if (candidate != nullptr && candidate->visible &&
          candidate->style.position_type != ui::PositionType::Absolute &&
          candidate->type != ui::NodeType::Splitter) {
        return candidate_id;
      }

      index += step;
    }

    return ui::k_invalid_node_id;
  };

  const ui::UINodeId previous_node_id = find_adjacent(-1);
  const ui::UINodeId next_node_id = find_adjacent(1);
  if (previous_node_id == ui::k_invalid_node_id ||
      next_node_id == ui::k_invalid_node_id) {
    return std::nullopt;
  }

  const auto *previous_node = target.document->node(previous_node_id);
  const auto *next_node = target.document->node(next_node_id);
  if (previous_node == nullptr || next_node == nullptr) {
    return std::nullopt;
  }

  const ui::FlexDirection parent_direction = parent->style.flex_direction;
  const float previous_size = parent_direction == ui::FlexDirection::Row
                                  ? previous_node->layout.measured_size.x
                                  : previous_node->layout.measured_size.y;
  const float next_size = parent_direction == ui::FlexDirection::Row
                              ? next_node->layout.measured_size.x
                              : next_node->layout.measured_size.y;

  target.document->mutate_style(
      previous_node_id, [previous_size](ui::UIStyle &style) {
        style.flex_basis = ui::UILength::pixels(previous_size);
        style.flex_grow = 0.0f;
      });
  target.document->mutate_style(next_node_id, [next_size](ui::UIStyle &style) {
    style.flex_basis = ui::UILength::pixels(next_size);
    style.flex_grow = 0.0f;
  });

  return UISystem::SplitterResizeDrag{
      .target = target,
      .start_pointer = pointer,
      .previous_node_id = previous_node_id,
      .next_node_id = next_node_id,
      .previous_start_size = previous_size,
      .next_start_size = next_size,
      .parent_direction = parent_direction,
  };
}

void update_splitter_resize_drag(const UISystem::SplitterResizeDrag &drag,
                                 glm::vec2 pointer) {
  if (drag.target.document == nullptr) {
    return;
  }

  const auto *parent = drag.target.document->node(
      drag.target.document->parent(drag.target.node_id));
  const auto *previous_node = drag.target.document->node(drag.previous_node_id);
  const auto *next_node = drag.target.document->node(drag.next_node_id);
  if (parent == nullptr || previous_node == nullptr || next_node == nullptr) {
    return;
  }

  const float raw_delta = drag.parent_direction == ui::FlexDirection::Row
                              ? pointer.x - drag.start_pointer.x
                              : pointer.y - drag.start_pointer.y;
  const float parent_basis = drag.parent_direction == ui::FlexDirection::Row
                                 ? parent->layout.content_bounds.width
                                 : parent->layout.content_bounds.height;
  const float rem_basis =
      drag.target.document != nullptr ? drag.target.document->root_font_size()
                                      : 16.0f;
  const auto [previous_min, previous_max] = resolved_main_axis_limits(
      *previous_node, parent_basis, drag.parent_direction, rem_basis);
  const auto [next_min, next_max] = resolved_main_axis_limits(
      *next_node, parent_basis, drag.parent_direction, rem_basis);

  const float delta_min = std::max(previous_min - drag.previous_start_size,
                                   drag.next_start_size - next_max);
  const float delta_max = std::min(previous_max - drag.previous_start_size,
                                   drag.next_start_size - next_min);
  if (delta_min > delta_max) {
    return;
  }

  const float clamped_delta = std::clamp(raw_delta, delta_min, delta_max);
  const float previous_size = drag.previous_start_size + clamped_delta;
  const float next_size = drag.next_start_size - clamped_delta;

  drag.target.document->mutate_style(
      drag.previous_node_id, [previous_size](ui::UIStyle &style) {
        style.flex_basis = ui::UILength::pixels(previous_size);
        style.flex_grow = 0.0f;
      });
  drag.target.document->mutate_style(
      drag.next_node_id, [next_size](ui::UIStyle &style) {
        style.flex_basis = ui::UILength::pixels(next_size);
        style.flex_grow = 0.0f;
      });
}

void update_panel_resize_drag(const UISystem::PanelResizeDrag &drag,
                              glm::vec2 pointer) {
  if (drag.target.document == nullptr) {
    return;
  }

  const auto *node = drag.target.document->node(drag.target.node_id);
  if (node == nullptr || !ui::node_supports_panel_resize(*node)) {
    return;
  }

  const ui::UiRect parent_bounds =
      parent_content_bounds(*drag.target.document, drag.target.node_id);
  const glm::vec2 delta = pointer - drag.start_pointer;

  ui::UiRect next_bounds = drag.start_bounds;
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
          node->style.min_width, parent_bounds.width,
          drag.target.document != nullptr ? drag.target.document->root_font_size()
                                          : 16.0f
      );
  const float max_width =
      resolve_max_length(
          node->style.max_width, parent_bounds.width,
          drag.target.document != nullptr ? drag.target.document->root_font_size()
                                          : 16.0f
      );
  const float min_height =
      resolve_min_length(
          node->style.min_height, parent_bounds.height,
          drag.target.document != nullptr ? drag.target.document->root_font_size()
                                          : 16.0f
      );
  const float max_height =
      resolve_max_length(
          node->style.max_height, parent_bounds.height,
          drag.target.document != nullptr ? drag.target.document->root_font_size()
                                          : 16.0f
      );

  next_bounds = ui::clamp_panel_resize_bounds(
      drag.start_bounds, next_bounds, parent_bounds, drag.part, min_width,
      max_width, min_height, max_height
  );

  write_absolute_bounds_to_style(drag.target, parent_bounds, next_bounds);
}

} // namespace astralix::ui_system_core
