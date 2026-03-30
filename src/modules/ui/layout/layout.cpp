#include "layout/common.hpp"
#include "layout/internal.hpp"

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

namespace astralix::ui {
namespace {

glm::vec2 measure_intrinsic_size(
    const UIDocument &document,
    UINodeId node_id,
    glm::vec2 available_size,
    const UILayoutContext &context
);

glm::vec2 measure_container_size(
    const UIDocument &document,
    UINodeId node_id,
    glm::vec2 available_size,
    const UILayoutContext &context
) {
  const auto *node = document.node(node_id);
  if (node == nullptr) {
    return glm::vec2(0.0f);
  }

  const UIRect available_rect{
      .width = std::max(0.0f, available_size.x),
      .height = std::max(0.0f, available_size.y),
  };
  const UIRect inner_rect = inset_rect(available_rect, node->style.padding);

  float total_main = 0.0f;
  float max_cross = 0.0f;
  uint32_t flow_children = 0u;

  for (UINodeId child_id : node->children) {
    const auto *child = document.node(child_id);
    if (child == nullptr || !child->visible ||
        child->style.position_type == PositionType::Absolute) {
      continue;
    }

    const glm::vec2 child_size = measure_intrinsic_size(
        document,
        child_id,
        glm::vec2(inner_rect.width, inner_rect.height),
        context
    );

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

glm::vec2 measure_intrinsic_size(
    const UIDocument &document,
    UINodeId node_id,
    glm::vec2 available_size,
    const UILayoutContext &context
) {
  const auto *node = document.node(node_id);
  if (node == nullptr || !node->visible) {
    return glm::vec2(0.0f);
  }

  float width = 0.0f;
  float height = 0.0f;

  switch (node->type) {
    case NodeType::Text: {
      const glm::vec2 text_size = measure_text_size(*node, context);
      width = text_size.x;
      height = text_size.y;
      break;
    }
    case NodeType::Image: {
      const glm::vec2 image_size = measure_image_size(*node);
      width = image_size.x;
      height = image_size.y;
      break;
    }
    case NodeType::RenderImageView: {
      const glm::vec2 image_size = measure_render_image_view_size(*node);
      width = image_size.x;
      height = image_size.y;
      break;
    }
    case NodeType::View:
    case NodeType::Pressable:
    case NodeType::ScrollView:
      if (!node->children.empty()) {
        const glm::vec2 container_size =
            measure_container_size(document, node_id, available_size, context);
        width = container_size.x;
        height = container_size.y;
      } else {
        width = node->style.padding.horizontal();
        height = node->style.padding.vertical();
      }
      break;
    case NodeType::TextInput: {
      const glm::vec2 input_size = measure_text_input_size(*node, context);
      width = input_size.x;
      height = input_size.y;
      break;
    }
    case NodeType::Combobox: {
      const glm::vec2 combobox_size = measure_combobox_size(*node, context);
      width = combobox_size.x;
      height = combobox_size.y;
      break;
    }
    case NodeType::Splitter: {
      const glm::vec2 splitter_size = measure_splitter_size(document, *node);
      width = splitter_size.x;
      height = splitter_size.y;
      break;
    }
    case NodeType::Checkbox: {
      const glm::vec2 checkbox_size = measure_checkbox_size(*node, context);
      width = checkbox_size.x;
      height = checkbox_size.y;
      break;
    }
    case NodeType::Slider: {
      const glm::vec2 slider_size = measure_slider_size(*node, context);
      width = slider_size.x;
      height = slider_size.y;
      break;
    }
    case NodeType::Select: {
      const glm::vec2 select_size = measure_select_size(*node, context);
      width = select_size.x;
      height = select_size.y;
      break;
    }
    case NodeType::SegmentedControl: {
      const glm::vec2 segmented_size =
          measure_segmented_control_size(*node, context);
      width = segmented_size.x;
      height = segmented_size.y;
      break;
    }
    case NodeType::ChipGroup: {
      const glm::vec2 chip_group_size = measure_chip_group_size(*node, context);
      width = chip_group_size.x;
      height = chip_group_size.y;
      break;
    }
  }

  const float intrinsic_width = width;
  const float intrinsic_height = height;

  width = resolve_length(
      node->style.width,
      available_size.x,
      context.default_font_size,
      intrinsic_width
  );
  height = resolve_length(
      node->style.height,
      available_size.y,
      context.default_font_size,
      intrinsic_height
  );

  width = clamp_dimension(
      width,
      node->style.min_width,
      node->style.max_width,
      available_size.x,
      context.default_font_size,
      intrinsic_width
  );
  height = clamp_dimension(
      height,
      node->style.min_height,
      node->style.max_height,
      available_size.y,
      context.default_font_size,
      intrinsic_height
  );

  return glm::vec2(width, height);
}

struct FlowItem {
  UINodeId node_id = k_invalid_node_id;
  glm::vec2 preferred_size = glm::vec2(0.0f);
  float main_size = 0.0f;
  float cross_size = 0.0f;
  float main_margin_leading = 0.0f;
  float main_margin_trailing = 0.0f;
  float cross_margin_leading = 0.0f;
  float cross_margin_trailing = 0.0f;
  float flex_grow = 0.0f;
  float flex_shrink = 0.0f;
  AlignItems align = AlignItems::Stretch;
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

void layout_children(
    UIDocument &document,
    UINodeId node_id,
    const UILayoutContext &context
) {
  auto *node = document.node(node_id);
  if (node == nullptr) {
    return;
  }

  const UIRect inner_bounds = node->layout.content_bounds;
  node->layout.scroll.viewport_size =
      glm::vec2(inner_bounds.width, inner_bounds.height);
  std::vector<FlowItem> flow_items;

  for (UINodeId child_id : node->children) {
    auto *child = document.node(child_id);
    if (child == nullptr || !child->visible ||
        child->style.position_type == PositionType::Absolute) {
      continue;
    }

    const glm::vec2 preferred = measure_intrinsic_size(
        document,
        child_id,
        glm::vec2(inner_bounds.width, inner_bounds.height),
        context
    );

    FlowItem item;
    item.node_id = child_id;
    item.preferred_size = preferred;
    item.flex_grow = child->style.flex_grow;
    item.flex_shrink = child->style.flex_shrink;
    item.align = resolve_align(*child, node->style.align_items);

    if (node->style.flex_direction == FlexDirection::Row) {
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
      for (FlowItem &item : flow_items) {
        const float shrink_basis =
            (item.flex_shrink * item.main_size) / total_flex_shrink;
        item.main_size =
            std::max(0.0f, item.main_size + free_space * shrink_basis);
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

  std::vector<std::pair<UINodeId, UIRect>> planned_bounds;
  planned_bounds.reserve(node->children.size());

  float cursor = leading_space;
  float content_width = 0.0f;
  float content_height = 0.0f;
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

    planned_bounds.emplace_back(item.node_id, child_bounds);
    content_width =
        std::max(content_width, child_bounds.right() - inner_bounds.x);
    content_height =
        std::max(content_height, child_bounds.bottom() - inner_bounds.y);

    cursor += item.main_margin_leading + item.main_size +
              item.main_margin_trailing + between_space;
  }

  for (UINodeId child_id : node->children) {
    auto *child = document.node(child_id);
    if (child == nullptr || !child->visible ||
        child->style.position_type != PositionType::Absolute) {
      continue;
    }

    const glm::vec2 preferred = measure_intrinsic_size(
        document,
        child_id,
        glm::vec2(inner_bounds.width, inner_bounds.height),
        context
    );

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
    planned_bounds.emplace_back(child_id, child_bounds);
    content_width =
        std::max(content_width, child_bounds.right() - inner_bounds.x);
    content_height =
        std::max(content_height, child_bounds.bottom() - inner_bounds.y);
  }

  node->layout.scroll.content_size = glm::vec2(content_width, content_height);
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

  for (const auto &[child_id, planned_bounds_rect] : planned_bounds) {
    UIRect translated_bounds = planned_bounds_rect;
    translated_bounds.x += scroll_translation.x;
    translated_bounds.y += scroll_translation.y;

    layout_node(
        document,
        child_id,
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

  for (UINodeId child_id : node->children) {
    append_draw_commands(document, child_id, context, effective_enabled);
  }

  append_scrollbar_commands(document, node_id, *node, resolved);
  append_resize_handle_commands(document, node_id, *node, resolved);
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

  for (auto it = node->children.rbegin(); it != node->children.rend(); ++it) {
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
  document.set_canvas_size(context.viewport_size);
  document.set_root_font_size(context.default_font_size);

  const UINodeId root_id = document.root();
  auto *root = document.node(root_id);
  if (root == nullptr) {
    document.draw_list().clear();
    document.clear_dirty();
    return;
  }

  const glm::vec2 preferred =
      measure_intrinsic_size(document, root_id, context.viewport_size, context);

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

  document.mark_paint_dirty();
  document.clear_layout_dirty();
}

std::optional<UIHitResult>
hit_test_document(const UIDocument &document, glm::vec2 point) {
  if (document.root() == k_invalid_node_id) {
    return std::nullopt;
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
  document.clear_paint_dirty();
}

} // namespace astralix::ui
