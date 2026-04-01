#include "systems/ui-system/widgets/layout/splitter.hpp"

#include "systems/ui-system/core.hpp"

#include <algorithm>

namespace astralix::ui_system_core {

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
    auto it = std::find(
        parent->children.begin(),
        parent->children.end(),
        target.node_id
    );
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
      previous_node_id,
      [previous_size](ui::UIStyle &style) {
        style.flex_basis = ui::UILength::pixels(previous_size);
        style.flex_grow = 0.0f;
      }
  );
  target.document->mutate_style(
      next_node_id,
      [next_size](ui::UIStyle &style) {
        style.flex_basis = ui::UILength::pixels(next_size);
        style.flex_grow = 0.0f;
      }
  );

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

void update_splitter_resize_drag(
    const UISystem::SplitterResizeDrag &drag,
    glm::vec2 pointer
) {
  if (drag.target.document == nullptr) {
    return;
  }

  const auto *parent = drag.target.document->node(
      drag.target.document->parent(drag.target.node_id)
  );
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
      *previous_node,
      parent_basis,
      drag.parent_direction,
      rem_basis
  );
  const auto [next_min, next_max] = resolved_main_axis_limits(
      *next_node,
      parent_basis,
      drag.parent_direction,
      rem_basis
  );

  const float delta_min = std::max(
      previous_min - drag.previous_start_size,
      drag.next_start_size - next_max
  );
  const float delta_max = std::min(
      previous_max - drag.previous_start_size,
      drag.next_start_size - next_min
  );
  if (delta_min > delta_max) {
    return;
  }

  const float clamped_delta = std::clamp(raw_delta, delta_min, delta_max);
  const float previous_size = drag.previous_start_size + clamped_delta;
  const float next_size = drag.next_start_size - clamped_delta;

  drag.target.document->mutate_style(
      drag.previous_node_id,
      [previous_size](ui::UIStyle &style) {
        style.flex_basis = ui::UILength::pixels(previous_size);
        style.flex_grow = 0.0f;
      }
  );
  drag.target.document->mutate_style(
      drag.next_node_id,
      [next_size](ui::UIStyle &style) {
        style.flex_basis = ui::UILength::pixels(next_size);
        style.flex_grow = 0.0f;
      }
  );
}

} // namespace astralix::ui_system_core
