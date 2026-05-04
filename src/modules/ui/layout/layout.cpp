#include "containers/vector.hpp"
#include "layout/common.hpp"
#include "layout/internal.hpp"
#include "layout/widgets/data-viz/line-chart.hpp"
#include "layout/widgets/graph/graph-view.hpp"
#include "trace.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>
#include <vector>

namespace astralix::ui {
namespace {

glm::vec2
clamp_available_size(glm::vec2 available_size) {
  return glm::vec2(
      std::max(0.0f, available_size.x), std::max(0.0f, available_size.y)
  );
}

glm::vec2 inner_size_for_outer_size(
    const UIDocument::UINode &node,
    glm::vec2 outer_size
) {
  const UIRect outer_rect{
      .width = std::max(0.0f, outer_size.x),
      .height = std::max(0.0f, outer_size.y),
  };
  const UIRect inner_rect = inset_rect(outer_rect, node.style.padding);
  return glm::vec2(inner_rect.width, inner_rect.height);
}

bool participates_in_intrinsic_pass(const UIDocument::UINode &node) {
  return node.visible && node.type != NodeType::Popover;
}

bool participates_in_flow_measurement(const UIDocument::UINode &node) {
  return participates_in_intrinsic_pass(node) &&
         node.style.position_type != PositionType::Absolute;
}

glm::vec2 resolve_preferred_size(
    const UIDocument::UINode &node,
    glm::vec2 available_size,
    const UILayoutContext &context,
    glm::vec2 content_size
) {
  available_size = clamp_available_size(available_size);
  content_size = clamp_available_size(content_size);

  float width = resolve_length(
      node.style.width,
      available_size.x,
      context.default_font_size,
      content_size.x
  );
  float height = resolve_length(
      node.style.height,
      available_size.y,
      context.default_font_size,
      content_size.y
  );

  width = clamp_dimension(
      width,
      node.style.min_width,
      node.style.max_width,
      available_size.x,
      context.default_font_size,
      content_size.x
  );
  height = clamp_dimension(
      height,
      node.style.min_height,
      node.style.max_height,
      available_size.y,
      context.default_font_size,
      content_size.y
  );

  return glm::vec2(width, height);
}

glm::vec2 estimate_preferred_size_for_children(
    const UIDocument::UINode &node,
    glm::vec2 available_size,
    const UILayoutContext &context
) {
  return resolve_preferred_size(
      node,
      available_size,
      context,
      clamp_available_size(available_size)
  );
}

glm::vec2 compute_node_content_size(
    const UIDocument &document,
    UINodeId node_id,
    const UILayoutContext &context
) {
  const auto *node = document.node(node_id);
  if (node == nullptr || !node->visible) {
    return glm::vec2(0.0f);
  }

  switch (node->type) {
    case NodeType::Text:
      return measure_text_size(*node, context);
    case NodeType::Image:
      return measure_image_size(*node);
    case NodeType::RenderImageView:
      return measure_render_image_view_size(*node);
    case NodeType::TextInput:
      return measure_text_input_size(*node, context);
    case NodeType::Combobox:
      return measure_combobox_size(*node, context);
    case NodeType::Splitter:
      return measure_splitter_size(document, *node);
    case NodeType::Checkbox:
      return measure_checkbox_size(*node, context);
    case NodeType::Slider:
      return measure_slider_size(*node, context);
    case NodeType::Select:
      return measure_select_size(*node, context);
    case NodeType::SegmentedControl:
      return measure_segmented_control_size(*node, context);
    case NodeType::ChipGroup:
      return measure_chip_group_size(*node, context);
    case NodeType::GraphView:
      return measure_graph_view_size(*node, context);
    case NodeType::View:
    case NodeType::Pressable:
    case NodeType::ScrollView:
    case NodeType::Popover:
    case NodeType::LineChart: {
      float total_main = 0.0f;
      float max_cross = 0.0f;
      uint32_t flow_children = 0u;

      for (UINodeId child_id : node->children) {
        const auto *child = document.node(child_id);
        if (child == nullptr || !participates_in_flow_measurement(*child)) {
          continue;
        }

        const glm::vec2 child_size = child->layout.intrinsic.preferred_size;
        const float horizontal_margin = child->style.margin.horizontal();
        const float vertical_margin = child->style.margin.vertical();
        if (node->style.flex_direction == FlexDirection::Row) {
          total_main += child_size.x + horizontal_margin;
          max_cross = std::max(max_cross, child_size.y + vertical_margin);
        } else {
          total_main += child_size.y + vertical_margin;
          max_cross = std::max(max_cross, child_size.x + horizontal_margin);
        }

        flow_children++;
      }

      if (flow_children > 1u) {
        total_main += node->style.gap * static_cast<float>(flow_children - 1u);
      }

      if (node->style.flex_direction == FlexDirection::Row) {
        return glm::vec2(
            total_main + node->style.padding.horizontal(),
            max_cross + node->style.padding.vertical()
        );
      }

      return glm::vec2(
          max_cross + node->style.padding.horizontal(),
          total_main + node->style.padding.vertical()
      );
    }
  }

  return glm::vec2(0.0f);
}

void compute_intrinsic_pass(
    UIDocument &document,
    UINodeId node_id,
    glm::vec2 available_size,
    const UILayoutContext &context
) {
  auto *node = document.node(node_id);
  if (node == nullptr) {
    return;
  }

  if (!node->visible) {
    node->layout.intrinsic.content_size = glm::vec2(0.0f);
    node->layout.intrinsic.preferred_size = glm::vec2(0.0f);
    return;
  }

  glm::vec2 initial_child_available;
  {
    ASTRA_PROFILE_N("intrinsic::estimate_size");
    available_size = clamp_available_size(available_size);
    const glm::vec2 estimated_outer_size =
        estimate_preferred_size_for_children(*node, available_size, context);
    initial_child_available =
        inner_size_for_outer_size(*node, estimated_outer_size);
  }

  {
    ASTRA_PROFILE_N("intrinsic::children_first_pass");
    for (UINodeId child_id : node->children) {
      auto *child = document.node(child_id);
      if (child == nullptr || !participates_in_intrinsic_pass(*child)) {
        continue;
      }

      compute_intrinsic_pass(document, child_id, initial_child_available, context);
    }
  }

  glm::vec2 content_size;
  glm::vec2 preferred_size;
  {
    ASTRA_PROFILE_N("intrinsic::resolve_size");
    content_size = compute_node_content_size(document, node_id, context);
    preferred_size =
        resolve_preferred_size(*node, available_size, context, content_size);
  }

  const glm::vec2 final_child_available =
      inner_size_for_outer_size(*node, preferred_size);
  const bool child_basis_changed =
      std::abs(final_child_available.x - initial_child_available.x) > 0.5f ||
      std::abs(final_child_available.y - initial_child_available.y) > 0.5f;
  if (child_basis_changed) {
    {
      ASTRA_PROFILE_N("intrinsic::children_repass");
      for (UINodeId child_id : node->children) {
        auto *child = document.node(child_id);
        if (child == nullptr || !participates_in_intrinsic_pass(*child)) {
          continue;
        }

        compute_intrinsic_pass(document, child_id, final_child_available, context);
      }
    }

    {
      ASTRA_PROFILE_N("intrinsic::resolve_size_repass");
      content_size = compute_node_content_size(document, node_id, context);
      preferred_size =
          resolve_preferred_size(*node, available_size, context, content_size);
    }
  }

  node->layout.intrinsic.content_size = content_size;
  node->layout.intrinsic.preferred_size = preferred_size;
}

struct FlowItem {
  UINodeId node_id = k_invalid_node_id;
  glm::vec2 preferred_size = glm::vec2(0.0f);
  float main_size = 0.0f;
  float cross_size = 0.0f;
  float min_main_size = 0.0f;
  float main_margin_leading = 0.0f;
  float main_margin_trailing = 0.0f;
  float cross_margin_leading = 0.0f;
  float cross_margin_trailing = 0.0f;
  float flex_grow = 0.0f;
  float flex_shrink = 0.0f;
  AlignItems align = AlignItems::Stretch;
};

struct PendingMainSize {
  size_t index = 0u;
  float size = 0.0f;
};

struct PlannedBounds {
  UINodeId node_id = k_invalid_node_id;
  UIRect bounds;
};

AlignItems resolve_align(const UIDocument::UINode &node, AlignItems parent) {
  switch (node.style.align_self) {
    case AlignSelf::Start:
      return AlignItems::Start;
    case AlignSelf::Center:
      return AlignItems::Center;
    case AlignSelf::End:
      return AlignItems::End;
    case AlignSelf::Stretch:
      return AlignItems::Stretch;
    case AlignSelf::Auto:
    default:
      return parent;
  }
}

void layout_node(
    UIDocument &document,
    UINodeId node_id,
    const UIRect &bounds,
    std::optional<UIRect> inherited_clip,
    const UILayoutContext &context
);

void update_popover_layout(
    UIDocument &document,
    UINodeId node_id,
    const UILayoutContext &context
) {
  auto *node = document.node(node_id);
  if (node == nullptr || node->type != NodeType::Popover || !node->visible ||
      !node->enabled || !node->popover.open) {
    return;
  }

  compute_intrinsic_pass(document, node_id, context.viewport_size, context);
  const glm::vec2 measured_size = node->layout.intrinsic.preferred_size;
  if (measured_size.x <= 0.0f || measured_size.y <= 0.0f) {
    node->layout.popover.popup_rect = UIRect{};
    return;
  }

  float popup_x = 0.0f;
  float popup_y = 0.0f;
  const float anchor_gap =
      node->popover.anchor_kind == UIPopupAnchorKind::Node ? 4.0f : 0.0f;

  if (node->popover.anchor_kind == UIPopupAnchorKind::Node) {
    const auto *anchor = document.node(node->popover.anchor_node_id);
    if (anchor == nullptr || !anchor->visible) {
      document.close_popover(node_id);
      if (auto *updated = document.node(node_id); updated != nullptr) {
        updated->layout.popover.popup_rect = UIRect{};
      }
      return;
    }

    switch (node->popover.placement) {
      case UIPopupPlacement::BottomStart:
        popup_x = anchor->layout.bounds.x;
        popup_y = anchor->layout.bounds.bottom() + anchor_gap;
        break;
      case UIPopupPlacement::TopStart:
        popup_x = anchor->layout.bounds.x;
        popup_y = anchor->layout.bounds.y - measured_size.y - anchor_gap;
        break;
      case UIPopupPlacement::RightStart:
        popup_x = anchor->layout.bounds.right() + anchor_gap;
        popup_y = anchor->layout.bounds.y;
        break;
    }
  } else {
    switch (node->popover.placement) {
      case UIPopupPlacement::BottomStart:
        popup_x = node->popover.anchor_point.x;
        popup_y = node->popover.anchor_point.y;
        break;
      case UIPopupPlacement::TopStart:
        popup_x = node->popover.anchor_point.x;
        popup_y = node->popover.anchor_point.y - measured_size.y;
        break;
      case UIPopupPlacement::RightStart:
        popup_x = node->popover.anchor_point.x;
        popup_y = node->popover.anchor_point.y;
        break;
    }
  }

  const UIRect popup_rect = clamp_rect_to_bounds(
      UIRect{
          .x = popup_x,
          .y = popup_y,
          .width = measured_size.x,
          .height = measured_size.y,
      },
      UIRect{
          .x = 0.0f,
          .y = 0.0f,
          .width = context.viewport_size.x,
          .height = context.viewport_size.y,
      }
  );

  layout_node(document, node_id, popup_rect, std::nullopt, context);
  if (auto *updated = document.node(node_id); updated != nullptr) {
    updated->layout.popover.popup_rect = popup_rect;
  }
}

void layout_children(
    UIDocument &document,
    UINodeId node_id,
    const UILayoutContext &context
) {
  auto *node = document.node(node_id);
  if (node == nullptr) {
    return;
  }

  const UIRect full_inner_bounds = node->layout.content_bounds;
  struct PlannedChildren {
    std::vector<PlannedBounds> bounds;
    float content_width = 0.0f;
    float content_height = 0.0f;
  };

  const auto plan_children = [&](const UIRect &inner_bounds) -> PlannedChildren {
    SmallVector<FlowItem, 16> flow_items;
    flow_items.reserve(node->children.size());

    for (UINodeId child_id : node->children) {
      auto *child = document.node(child_id);
      if (child == nullptr || !child->visible ||
          child->type == NodeType::Popover ||
          child->style.position_type == PositionType::Absolute) {
        continue;
      }

      const glm::vec2 preferred = child->layout.intrinsic.preferred_size;

      FlowItem item;
      item.node_id = child_id;
      item.preferred_size = preferred;
      item.flex_grow = child->style.flex_grow;
      item.flex_shrink = child->style.flex_shrink;
      item.align = resolve_align(*child, node->style.align_items);

      if (node->style.flex_direction == FlexDirection::Row) {
        item.min_main_size =
            child->style.min_width.unit != UILengthUnit::Auto
                ? resolve_length(
                      child->style.min_width,
                      inner_bounds.width,
                      context.default_font_size,
                      preferred.x
                  )
                : 0.0f;
        item.main_size = resolve_length(
            child->style.flex_basis,
            inner_bounds.width,
            context.default_font_size,
            resolve_length(
                child->style.width,
                inner_bounds.width,
                context.default_font_size,
                preferred.x
            )
        );
        item.cross_size = resolve_length(
            child->style.height,
            inner_bounds.height,
            context.default_font_size,
            preferred.y
        );
        item.main_margin_leading = child->style.margin.left;
        item.main_margin_trailing = child->style.margin.right;
        item.cross_margin_leading = child->style.margin.top;
        item.cross_margin_trailing = child->style.margin.bottom;

        item.main_size = clamp_dimension(
            item.main_size,
            child->style.min_width,
            child->style.max_width,
            inner_bounds.width,
            context.default_font_size,
            preferred.x
        );
        item.cross_size = clamp_dimension(
            item.cross_size,
            child->style.min_height,
            child->style.max_height,
            inner_bounds.height,
            context.default_font_size,
            preferred.y
        );
      } else {
        item.min_main_size =
            child->style.min_height.unit != UILengthUnit::Auto
                ? resolve_length(
                      child->style.min_height,
                      inner_bounds.height,
                      context.default_font_size,
                      preferred.y
                  )
                : 0.0f;
        item.main_size = resolve_length(
            child->style.flex_basis,
            inner_bounds.height,
            context.default_font_size,
            resolve_length(
                child->style.height,
                inner_bounds.height,
                context.default_font_size,
                preferred.y
            )
        );
        item.cross_size = resolve_length(
            child->style.width,
            inner_bounds.width,
            context.default_font_size,
            preferred.x
        );
        item.main_margin_leading = child->style.margin.top;
        item.main_margin_trailing = child->style.margin.bottom;
        item.cross_margin_leading = child->style.margin.left;
        item.cross_margin_trailing = child->style.margin.right;

        item.main_size = clamp_dimension(
            item.main_size,
            child->style.min_height,
            child->style.max_height,
            inner_bounds.height,
            context.default_font_size,
            preferred.y
        );
        item.cross_size = clamp_dimension(
            item.cross_size,
            child->style.min_width,
            child->style.max_width,
            inner_bounds.width,
            context.default_font_size,
            preferred.x
        );
      }

      flow_items.push_back(item);
    }

    const float container_main = node->style.flex_direction == FlexDirection::Row
                                     ? inner_bounds.width
                                     : inner_bounds.height;
    const float container_cross = node->style.flex_direction == FlexDirection::Row
                                      ? inner_bounds.height
                                      : inner_bounds.width;

    float total_main = 0.0f;
    float total_flex_grow = 0.0f;
    float total_flex_shrink = 0.0f;

    for (const FlowItem &item : flow_items) {
      total_main +=
          item.main_size + item.main_margin_leading + item.main_margin_trailing;
      total_flex_grow += item.flex_grow;
      total_flex_shrink += item.flex_shrink * item.main_size;
    }

    if (flow_items.size() > 1u) {
      total_main += node->style.gap * static_cast<float>(flow_items.size() - 1u);
    }

    const float free_space = container_main - total_main;
    const bool scrolls_along_main_axis =
        node->type == NodeType::ScrollView &&
        ((node->style.flex_direction == FlexDirection::Row &&
          scrolls_horizontally(node->style.scroll_mode)) ||
         (node->style.flex_direction == FlexDirection::Column &&
          scrolls_vertically(node->style.scroll_mode)) ||
         node->style.scroll_mode == ScrollMode::Both);

    if (!scrolls_along_main_axis) {
      if (free_space > 0.0f && total_flex_grow > 0.0f) {
        for (FlowItem &item : flow_items) {
          item.main_size += free_space * (item.flex_grow / total_flex_grow);
        }
      } else if (free_space < 0.0f && total_flex_shrink > 0.0f) {
        float remaining_shrink = -free_space;
        SmallVector<size_t, 16> active_indices;
        active_indices.reserve(flow_items.size());
        for (size_t index = 0u; index < flow_items.size(); ++index) {
          if (flow_items[index].flex_shrink > 0.0f &&
              flow_items[index].main_size > flow_items[index].min_main_size) {
            active_indices.push_back(index);
          }
        }

        while (remaining_shrink > 0.001f && !active_indices.empty()) {
          float weighted_shrink = 0.0f;
          for (const size_t index : active_indices) {
            weighted_shrink +=
                flow_items[index].flex_shrink * flow_items[index].main_size;
          }

          if (weighted_shrink <= 0.0f) {
            break;
          }

          bool clamped_any = false;
          SmallVector<PendingMainSize, 16> next_sizes;
          next_sizes.reserve(active_indices.size());
          SmallVector<size_t, 16> next_active_indices;
          next_active_indices.reserve(active_indices.size());

          for (const size_t index : active_indices) {
            FlowItem &item = flow_items[index];
            const float shrink_weight =
                (item.flex_shrink * item.main_size) / weighted_shrink;
            const float requested_shrink = remaining_shrink * shrink_weight;
            const float target_size = item.main_size - requested_shrink;

            if (target_size <= item.min_main_size + 0.001f) {
              remaining_shrink -= std::max(
                  0.0f, item.main_size - item.min_main_size
              );
              item.main_size = item.min_main_size;
              clamped_any = true;
              continue;
            }

            next_sizes.push_back(PendingMainSize{
                .index = index,
                .size = target_size,
            });
            next_active_indices.push_back(index);
          }

          if (!clamped_any) {
            for (const PendingMainSize &next_size : next_sizes) {
              flow_items[next_size.index].main_size = next_size.size;
            }
            remaining_shrink = 0.0f;
            break;
          }

          active_indices = std::move(next_active_indices);
        }
      }
    }

    float consumed_main = 0.0f;
    for (const FlowItem &item : flow_items) {
      consumed_main +=
          item.main_size + item.main_margin_leading + item.main_margin_trailing;
    }

    if (flow_items.size() > 1u) {
      consumed_main +=
          node->style.gap * static_cast<float>(flow_items.size() - 1u);
    }

    const float remaining_space = std::max(0.0f, container_main - consumed_main);
    const UIFlowSpacing spacing = resolve_flow_spacing(
        node->style.justify_content,
        node->style.gap,
        remaining_space,
        flow_items.size()
    );
    const float leading_space = spacing.leading;
    const float between_space = spacing.between;

    PlannedChildren planned;
    planned.bounds.reserve(node->children.size());

    float cursor = leading_space;
    for (const FlowItem &item : flow_items) {
      auto *child = document.node(item.node_id);
      if (child == nullptr) {
        continue;
      }

      float cross_size = item.cross_size;
      if (item.align == AlignItems::Stretch &&
          ((node->style.flex_direction == FlexDirection::Row &&
            child->style.height.unit == UILengthUnit::Auto) ||
           (node->style.flex_direction == FlexDirection::Column &&
            child->style.width.unit == UILengthUnit::Auto))) {
        cross_size = std::max(
            0.0f,
            container_cross - item.cross_margin_leading -
                item.cross_margin_trailing
        );
      }

      float cross_offset = 0.0f;
      switch (item.align) {
        case AlignItems::Center:
          cross_offset =
              (container_cross - cross_size - item.cross_margin_leading -
               item.cross_margin_trailing) *
              0.5f;
          break;
        case AlignItems::End:
          cross_offset = container_cross - cross_size -
                         item.cross_margin_leading - item.cross_margin_trailing;
          break;
        case AlignItems::Stretch:
        case AlignItems::Start:
        default:
          cross_offset = 0.0f;
          break;
      }

      UIRect child_bounds;
      if (node->style.flex_direction == FlexDirection::Row) {
        child_bounds = UIRect{
            .x = inner_bounds.x + cursor + item.main_margin_leading,
            .y = inner_bounds.y + cross_offset + item.cross_margin_leading,
            .width = item.main_size,
            .height = cross_size,
        };
      } else {
        child_bounds = UIRect{
            .x = inner_bounds.x + cross_offset + item.cross_margin_leading,
            .y = inner_bounds.y + cursor + item.main_margin_leading,
            .width = cross_size,
            .height = item.main_size,
        };
      }

      planned.bounds.push_back(PlannedBounds{
          .node_id = item.node_id,
          .bounds = child_bounds,
      });
      planned.content_width = std::max(
          planned.content_width, child_bounds.right() - inner_bounds.x
      );
      planned.content_height = std::max(
          planned.content_height, child_bounds.bottom() - inner_bounds.y
      );

      cursor += item.main_margin_leading + item.main_size +
                item.main_margin_trailing + between_space;
    }

    for (UINodeId child_id : node->children) {
      auto *child = document.node(child_id);
      if (child == nullptr || !child->visible ||
          child->type == NodeType::Popover ||
          child->style.position_type != PositionType::Absolute) {
        continue;
      }

      const glm::vec2 preferred = child->layout.intrinsic.preferred_size;

      float width = resolve_length(
          child->style.width,
          inner_bounds.width,
          context.default_font_size,
          preferred.x
      );
      float height = resolve_length(
          child->style.height,
          inner_bounds.height,
          context.default_font_size,
          preferred.y
      );

      if (child->style.left.unit != UILengthUnit::Auto &&
          child->style.right.unit != UILengthUnit::Auto &&
          child->style.width.unit == UILengthUnit::Auto) {
        width = std::max(
            0.0f,
            inner_bounds.width -
                resolve_length(
                    child->style.left,
                    inner_bounds.width,
                    context.default_font_size
                ) -
                resolve_length(
                    child->style.right,
                    inner_bounds.width,
                    context.default_font_size
                )
        );
      }

      if (child->style.top.unit != UILengthUnit::Auto &&
          child->style.bottom.unit != UILengthUnit::Auto &&
          child->style.height.unit == UILengthUnit::Auto) {
        height = std::max(
            0.0f,
            inner_bounds.height -
                resolve_length(
                    child->style.top,
                    inner_bounds.height,
                    context.default_font_size
                ) -
                resolve_length(
                    child->style.bottom,
                    inner_bounds.height,
                    context.default_font_size
                )
        );
      }

      width = clamp_dimension(
          width,
          child->style.min_width,
          child->style.max_width,
          inner_bounds.width,
          context.default_font_size,
          preferred.x
      );
      height = clamp_dimension(
          height,
          child->style.min_height,
          child->style.max_height,
          inner_bounds.height,
          context.default_font_size,
          preferred.y
      );

      const float x =
          child->style.left.unit != UILengthUnit::Auto
              ? inner_bounds.x +
                    resolve_length(
                        child->style.left,
                        inner_bounds.width,
                        context.default_font_size
                    )
              : inner_bounds.right() -
                    resolve_length(
                        child->style.right,
                        inner_bounds.width,
                        context.default_font_size
                    ) -
                    width;
      const float y =
          child->style.top.unit != UILengthUnit::Auto
              ? inner_bounds.y +
                    resolve_length(
                        child->style.top,
                        inner_bounds.height,
                        context.default_font_size
                    )
              : inner_bounds.bottom() -
                    resolve_length(
                        child->style.bottom,
                        inner_bounds.height,
                        context.default_font_size
                    ) -
                    height;

      UIRect child_bounds{.x = x, .y = y, .width = width, .height = height};
      if (node_supports_panel_resize(*child)) {
        child_bounds = clamp_rect_to_bounds(child_bounds, inner_bounds);
      }
      planned.bounds.push_back(PlannedBounds{
          .node_id = child_id,
          .bounds = child_bounds,
      });
      planned.content_width = std::max(
          planned.content_width, child_bounds.right() - inner_bounds.x
      );
      planned.content_height = std::max(
          planned.content_height, child_bounds.bottom() - inner_bounds.y
      );
    }

    return planned;
  };

  UIRect effective_inner_bounds = full_inner_bounds;
  PlannedChildren planned = plan_children(effective_inner_bounds);

  if (node->type == NodeType::ScrollView &&
      node->style.scrollbar_visibility != ScrollbarVisibility::Hidden) {
    const float scrollbar_thickness =
        std::max(0.0f, node->style.scrollbar_thickness);
    if (scrollbar_thickness > 0.0f) {
      const bool wants_vertical = scrolls_vertically(node->style.scroll_mode);
      const bool wants_horizontal = scrolls_horizontally(node->style.scroll_mode);
      const bool always_show =
          node->style.scrollbar_visibility == ScrollbarVisibility::Always;

      for (size_t iteration = 0u; iteration < 3u; ++iteration) {
        const bool show_vertical =
            wants_vertical &&
            (always_show || planned.content_height > effective_inner_bounds.height);
        const bool show_horizontal =
            wants_horizontal &&
            (always_show || planned.content_width > effective_inner_bounds.width);

        UIRect next_inner_bounds = full_inner_bounds;
        if (show_vertical) {
          next_inner_bounds.width = std::max(
              0.0f, next_inner_bounds.width - scrollbar_thickness
          );
        }
        if (show_horizontal) {
          next_inner_bounds.height = std::max(
              0.0f, next_inner_bounds.height - scrollbar_thickness
          );
        }

        if (std::fabs(next_inner_bounds.width - effective_inner_bounds.width) <
                0.001f &&
            std::fabs(next_inner_bounds.height - effective_inner_bounds.height) <
                0.001f) {
          break;
        }

        effective_inner_bounds = next_inner_bounds;
        planned = plan_children(effective_inner_bounds);
      }
    }
  }

  node->layout.scroll.viewport_size =
      glm::vec2(effective_inner_bounds.width, effective_inner_bounds.height);

  node->layout.scroll.content_size =
      glm::vec2(planned.content_width, planned.content_height);
  node->layout.scroll.max_offset = glm::max(
      node->layout.scroll.content_size - node->layout.scroll.viewport_size,
      glm::vec2(0.0f)
  );
  node->layout.scroll.offset = clamp_scroll_offset(
      node->layout.scroll.offset,
      node->layout.scroll.max_offset,
      node->style.scroll_mode
  );
  update_scrollbar_geometry(*node);

  const glm::vec2 scroll_translation(
      scrolls_horizontally(node->style.scroll_mode)
          ? -node->layout.scroll.offset.x
          : 0.0f,
      scrolls_vertically(node->style.scroll_mode)
          ? -node->layout.scroll.offset.y
          : 0.0f
  );

  for (const PlannedBounds &planned_child : planned.bounds) {
    UIRect translated_bounds = planned_child.bounds;
    translated_bounds.x += scroll_translation.x;
    translated_bounds.y += scroll_translation.y;

    layout_node(
        document,
        planned_child.node_id,
        translated_bounds,
        child_clip_rect(*node),
        context
    );
  }
}

void layout_node(
    UIDocument &document,
    UINodeId node_id,
    const UIRect &bounds,
    std::optional<UIRect> inherited_clip,
    const UILayoutContext &context
) {
  auto *node = document.node(node_id);
  if (node == nullptr) {
    return;
  }

  node->layout.bounds = bounds;
  node->layout.content_bounds = inset_rect(bounds, node->style.padding);
  node->layout.measured_size = glm::vec2(bounds.width, bounds.height);

  node->layout.has_clip = inherited_clip.has_value();
  node->layout.clip_bounds = inherited_clip.value_or(UIRect{});

  std::optional<UIRect> child_clip = inherited_clip;
  if (node->style.overflow == Overflow::Hidden) {
    child_clip = child_clip.has_value()
                     ? intersect_rect(*child_clip, node->layout.content_bounds)
                     : node->layout.content_bounds;
  }

  node->layout.has_content_clip = child_clip.has_value();
  node->layout.content_clip_bounds = child_clip.value_or(UIRect{});

  layout_children(document, node_id, context);

  switch (node->type) {
    case NodeType::Checkbox:
      update_checkbox_layout(*node, context);
      break;
    case NodeType::Slider:
      update_slider_layout(*node);
      break;
    case NodeType::Select:
      update_select_layout(*node, context);
      break;
    case NodeType::Combobox:
      update_combobox_layout(*node, context);
      break;
    case NodeType::SegmentedControl:
      update_segmented_control_layout(*node, context);
      break;
    case NodeType::ChipGroup:
      update_chip_group_layout(*node, context);
      break;
    case NodeType::GraphView:
      update_graph_view_layout(*node, context);
      break;
    default:
      break;
  }
}

void append_draw_commands(
    UIDocument &document,
    UINodeId node_id,
    const UILayoutContext &context,
    bool parent_enabled = true
) {
  auto *node = document.node(node_id);
  if (node == nullptr || !node->visible) {
    return;
  }

  if (node->layout.has_clip &&
      !intersects(node->layout.bounds, node->layout.clip_bounds)) {
    return;
  }

  const bool effective_enabled = parent_enabled && node->enabled;
  const UIResolvedStyle resolved =
      resolve_style(node->style, node->paint_state, effective_enabled);
  const UIRect bounds = node->layout.bounds;
  const UIRect content_bounds = node->layout.content_bounds;

  if ((resolved.background_color.a > 0.0f || resolved.border_width > 0.0f) &&
      bounds.width > 0.0f && bounds.height > 0.0f) {
    glm::vec4 fill = resolved.background_color;
    fill.a *= resolved.opacity;
    UIDrawCommand command;
    command.type = DrawCommandType::Rect;
    command.node_id = node_id;
    command.rect = bounds;
    apply_self_clip(command, *node);
    command.color = fill;
    command.border_color = resolved.border_color;
    command.border_color.a *= resolved.opacity;
    command.border_width = resolved.border_width;
    command.border_radius = resolved.border_radius;
    document.draw_list().commands.push_back(std::move(command));
  }

  if (node->type == NodeType::Image) {
    append_image_node_commands(document, node_id, *node, resolved);
  }

  if (node->type == NodeType::RenderImageView) {
    append_render_image_view_node_commands(document, node_id, *node, resolved);
  }

  if (node->type == NodeType::Text) {
    append_text_node_commands(document, node_id, *node, context, resolved);
  }

  if (node->type == NodeType::TextInput) {
    auto font = resolve_ui_font(*node, context);
    const float font_size = resolve_ui_font_size(*node, context);
    const float line_height =
        font != nullptr ? font->line_height(font_size) : font_size;
    const UIRect text_rect =
        resolve_single_line_text_rect(content_bounds, line_height);
    append_editable_text_commands(
        document,
        node_id,
        *node,
        context,
        resolved,
        text_rect
    );
  }

  if (node->type == NodeType::Checkbox) {
    append_checkbox_commands(document, node_id, *node, context, resolved);
  }

  if (node->type == NodeType::Slider) {
    append_slider_commands(document, node_id, *node, resolved);
  }

  if (node->type == NodeType::Select) {
    append_select_field_commands(document, node_id, *node, context, resolved);
  }

  if (node->type == NodeType::Combobox) {
    append_combobox_field_commands(document, node_id, *node, context, resolved);
  }

  if (node->type == NodeType::SegmentedControl) {
    append_segmented_control_commands(
        document,
        node_id,
        *node,
        context,
        resolved
    );
  }

  if (node->type == NodeType::ChipGroup) {
    append_chip_group_commands(document, node_id, *node, context, resolved);
  }

  if (node->type == NodeType::LineChart) {
    append_line_chart_commands(document, node_id, *node, resolved);
  }

  if (node->type == NodeType::GraphView) {
    append_graph_view_commands(document, node_id, *node, context, resolved);
  }

  for (UINodeId child_id : node->children) {
    const auto *child = document.node(child_id);
    if (child != nullptr && child->type == NodeType::Popover) {
      continue;
    }

    if (node->type == NodeType::ScrollView && child != nullptr) {
      const auto clip = child_clip_rect(*node);
      if (clip.has_value() && !intersects(child->layout.bounds, *clip)) {
        continue;
      }
    }

    append_draw_commands(document, child_id, context, effective_enabled);
  }

  append_scrollbar_commands(document, node_id, *node, resolved);
  append_resize_handle_commands(document, node_id, *node, resolved);
}

void append_popover_overlay_commands(
    UIDocument &document,
    UINodeId node_id,
    const UILayoutContext &context
) {
  const auto *node = document.node(node_id);
  if (node == nullptr || node->type != NodeType::Popover || !node->visible ||
      !node->enabled || !node->popover.open) {
    return;
  }

  append_draw_commands(document, node_id, context);
}

std::optional<UIHitResult>
hit_test_node(const UIDocument &document, UINodeId node_id, glm::vec2 point) {
  const auto *node = document.node(node_id);
  if (node == nullptr || !node->visible || !node->enabled) {
    return std::nullopt;
  }

  if (node->layout.has_clip && !node->layout.clip_bounds.contains(point)) {
    return std::nullopt;
  }

  if (!node->layout.bounds.contains(point)) {
    return std::nullopt;
  }

  const glm::vec2 local_position(
      point.x - node->layout.content_bounds.x,
      point.y - node->layout.content_bounds.y
  );

  if (auto resize_hit = hit_test_resize_handles(*node, point);
      resize_hit.has_value()) {
    return UIHitResult{.node_id = node_id, .part = *resize_hit};
  }

  if (auto splitter_hit = hit_test_splitter(*node, node_id);
      splitter_hit.has_value()) {
    return splitter_hit;
  }

  if (node->type == NodeType::Slider) {
    if (node->layout.slider.thumb_rect.contains(point)) {
      return UIHitResult{.node_id = node_id, .part = UIHitPart::SliderThumb};
    }

    return UIHitResult{.node_id = node_id, .part = UIHitPart::SliderTrack};
  }

  if (node->type == NodeType::SegmentedControl) {
    for (size_t index = 0u;
         index < node->layout.segmented_control.item_rects.size();
         ++index) {
      if (node->layout.segmented_control.item_rects[index].contains(point)) {
        return UIHitResult{
            .node_id = node_id,
            .part = UIHitPart::SegmentedControlItem,
            .item_index = index,
        };
      }
    }
  }

  if (node->type == NodeType::ChipGroup) {
    for (size_t index = 0u; index < node->layout.chip_group.item_rects.size();
         ++index) {
      if (node->layout.chip_group.item_rects[index].contains(point)) {
        return UIHitResult{
            .node_id = node_id,
            .part = UIHitPart::ChipItem,
            .item_index = index,
        };
      }
    }
  }

  if (node->type == NodeType::Select) {
    return UIHitResult{.node_id = node_id, .part = UIHitPart::SelectField};
  }

  if (node->type == NodeType::Combobox) {
    return UIHitResult{.node_id = node_id, .part = UIHitPart::ComboboxField};
  }

  if (auto scroll_hit = hit_test_scroll_view(*node, node_id, point);
      scroll_hit.has_value()) {
    return scroll_hit;
  }

  if (node->on_custom_hit_test) {
    if (auto custom_hit = node->on_custom_hit_test(local_position);
        custom_hit.has_value()) {
      custom_hit->local_position = local_position;
      return UIHitResult{
          .node_id = node_id,
          .part = UIHitPart::Body,
          .custom = std::move(custom_hit),
      };
    }
  }

  for (auto it = node->children.rbegin(); it != node->children.rend(); ++it) {
    if (const auto *child = document.node(*it);
        child != nullptr && child->type == NodeType::Popover) {
      continue;
    }

    auto hit = hit_test_node(document, *it, point);
    if (hit.has_value()) {
      return hit;
    }
  }

  return UIHitResult{
      .node_id = node_id,
      .part = node->type == NodeType::TextInput  ? UIHitPart::TextInputText
              : node->type == NodeType::Combobox ? UIHitPart::ComboboxField
                                                 : UIHitPart::Body,
  };
}

} // namespace

void layout_document(UIDocument &document, const UILayoutContext &context) {
  ASTRA_PROFILE_N("ui::layout_document");
  document.set_canvas_size(context.viewport_size);
  document.set_root_font_size(context.default_font_size);

  const UINodeId root_id = document.root();
  auto *root = document.node(root_id);
  if (root == nullptr) {
    document.draw_list().clear();
    document.clear_dirty();
    return;
  }

  {
    ASTRA_PROFILE_N("ui::compute_intrinsic_pass");
    compute_intrinsic_pass(document, root_id, context.viewport_size, context);
  }
  const glm::vec2 preferred = root->layout.intrinsic.preferred_size;

  const float width = resolve_length(
      root->style.width,
      context.viewport_size.x,
      context.default_font_size,
      std::max(context.viewport_size.x, preferred.x)
  );
  const float height = resolve_length(
      root->style.height,
      context.viewport_size.y,
      context.default_font_size,
      std::max(context.viewport_size.y, preferred.y)
  );

  {
    ASTRA_PROFILE_N("ui::layout_node");
    layout_node(
        document,
        root_id,
        UIRect{
            .x = 0.0f,
            .y = 0.0f,
            .width = width,
            .height = height,
        },
        std::nullopt,
        context
    );
  }

  const std::vector<UINodeId> open_popovers = document.open_popover_stack();
  for (UINodeId popover_id : open_popovers) {
    update_popover_layout(document, popover_id, context);
  }

  document.mark_paint_dirty();
  document.clear_layout_dirty();
}

std::optional<UIHitResult>
hit_test_document(const UIDocument &document, glm::vec2 point) {
  ASTRA_PROFILE_N("ui::hit_test_document");
  if (document.root() == k_invalid_node_id) {
    return std::nullopt;
  }

  for (auto it = document.open_popover_stack().rbegin();
       it != document.open_popover_stack().rend();
       ++it) {
    const auto *popup = document.node(*it);
    if (popup == nullptr || popup->type != NodeType::Popover ||
        !popup->visible || !popup->enabled || !popup->popover.open ||
        !popup->layout.popover.popup_rect.contains(point)) {
      continue;
    }

    if (auto hit = hit_test_node(document, *it, point); hit.has_value()) {
      return hit;
    }

    return UIHitResult{.node_id = *it, .part = UIHitPart::Body};
  }

  const UINodeId open_popup_id = document.open_popup_node();
  if (open_popup_id != k_invalid_node_id) {
    if (const auto *popup = document.node(open_popup_id);
        popup != nullptr && popup->visible && popup->enabled) {
      if (popup->type == NodeType::Select && popup->select.open) {
        for (size_t index = 0u; index < popup->layout.select.option_rects.size();
             ++index) {
          if (popup->layout.select.option_rects[index].contains(point)) {
            return UIHitResult{
                .node_id = open_popup_id,
                .part = UIHitPart::SelectOption,
                .item_index = index,
            };
          }
        }
      } else if (popup->type == NodeType::Combobox && popup->combobox.open) {
        for (size_t index = 0u;
             index < popup->layout.combobox.option_rects.size();
             ++index) {
          if (popup->layout.combobox.option_rects[index].contains(point)) {
            return UIHitResult{
                .node_id = open_popup_id,
                .part = UIHitPart::ComboboxOption,
                .item_index = index,
            };
          }
        }
      }
    }
  }

  return hit_test_node(document, document.root(), point);
}

void build_draw_list(UIDocument &document, const UILayoutContext &context) {
  ASTRA_PROFILE_N("ui::build_draw_list");
  document.draw_list().clear();

  if (document.root() == k_invalid_node_id) {
    document.clear_dirty();
    return;
  }

  append_draw_commands(document, document.root(), context);
  if (document.open_popup_node() != k_invalid_node_id) {
    if (const auto *popup = document.node(document.open_popup_node());
        popup != nullptr && popup->type == NodeType::Select) {
      append_select_overlay_commands(
          document,
          document.open_popup_node(),
          context
      );
    } else if (popup != nullptr && popup->type == NodeType::Combobox) {
      append_combobox_overlay_commands(
          document,
          document.open_popup_node(),
          context
      );
    }
  }

  for (UINodeId popover_id : document.open_popover_stack()) {
    append_popover_overlay_commands(document, popover_id, context);
  }
  document.clear_paint_dirty();
}

} // namespace astralix::ui
