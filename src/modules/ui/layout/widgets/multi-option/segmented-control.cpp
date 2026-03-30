#include "layout/widgets/multi-option/segmented-control.hpp"

#include "layout/common.hpp"

#include <algorithm>

namespace astralix::ui {
namespace {

constexpr float k_segmented_item_padding_x = 14.0f;
constexpr float k_segmented_item_padding_y = 8.0f;

} // namespace

glm::vec2 measure_segmented_control_size(
    const UIDocument::UINode &node,
    const UILayoutContext &context
) {
  const float line_height = measure_line_height(node, context);
  const float item_height = line_height + k_segmented_item_padding_y * 2.0f;
  float total_width = node.style.padding.horizontal();
  for (size_t index = 0u; index < node.segmented_control.options.size();
       ++index) {
    total_width += measure_label_width(
                       node, context, node.segmented_control.options[index]
                   ) +
                   k_segmented_item_padding_x * 2.0f;
    if (index + 1u < node.segmented_control.options.size()) {
      total_width += node.style.control_gap;
    }
  }

  return glm::vec2(total_width, node.style.padding.vertical() + item_height);
}

void update_segmented_control_layout(
    UIDocument::UINode &node,
    const UILayoutContext &context
) {
  node.layout.segmented_control = UILayoutMetrics::SegmentedControlLayout{};
  const UIRect content = node.layout.content_bounds;
  if (content.width <= 0.0f || content.height <= 0.0f ||
      node.segmented_control.options.empty()) {
    return;
  }

  const float line_height = measure_line_height(node, context);
  const float item_height = std::min(
      content.height,
      line_height + k_segmented_item_padding_y * 2.0f
  );
  const float y =
      content.y + std::max(0.0f, (content.height - item_height) * 0.5f);

  float cursor_x = content.x;
  node.layout.segmented_control.item_rects.reserve(
      node.segmented_control.options.size()
  );
  for (size_t index = 0u; index < node.segmented_control.options.size();
       ++index) {
    const float item_width = measure_label_width(
                                 node,
                                 context,
                                 node.segmented_control.options[index]
                             ) +
                             k_segmented_item_padding_x * 2.0f;
    node.layout.segmented_control.item_rects.push_back(UIRect{
        .x = cursor_x,
        .y = y,
        .width = item_width,
        .height = item_height,
    });
    cursor_x += item_width + node.style.control_gap;
  }
}

void append_segmented_control_commands(
    UIDocument &document,
    UINodeId node_id,
    const UIDocument::UINode &node,
    const UILayoutContext &context,
    const UIResolvedStyle &resolved
) {
  const float line_height = measure_line_height(node, context);
  for (size_t index = 0u; index < node.layout.segmented_control.item_rects.size();
       ++index) {
    const UIRect item_rect = node.layout.segmented_control.item_rects[index];
    const bool selected = index == node.segmented_control.selected_index;
    const bool hovered =
        node.layout.segmented_control.hovered_item_index.has_value() &&
        *node.layout.segmented_control.hovered_item_index == index;
    const bool active =
        node.layout.segmented_control.active_item_index.has_value() &&
        *node.layout.segmented_control.active_item_index == index;

    UIDrawCommand bg_command;
    bg_command.type = DrawCommandType::Rect;
    bg_command.node_id = node_id;
    bg_command.rect = item_rect;
    apply_content_clip(bg_command, node);
    bg_command.color =
        (selected
             ? node.style.accent_color *
                   glm::vec4(1.0f, 1.0f, 1.0f, active ? 0.4f : 0.28f)
         : hovered ? glm::vec4(0.16f, 0.24f, 0.35f, active ? 0.95f : 0.82f)
         : active  ? glm::vec4(0.1f, 0.16f, 0.24f, 0.98f)
                   : glm::vec4(0.0f));
    bg_command.color *= glm::vec4(1.0f, 1.0f, 1.0f, resolved.opacity);
    bg_command.border_color =
        (selected ? node.style.accent_color
                  : glm::vec4(0.32f, 0.45f, 0.6f, hovered ? 0.5f : 0.28f)) *
        glm::vec4(1.0f, 1.0f, 1.0f, resolved.opacity);
    bg_command.border_width = node.style.border_width;
    bg_command.border_radius = node.style.border_radius;
    document.draw_list().commands.push_back(std::move(bg_command));

    UIDrawCommand text_command;
    text_command.type = DrawCommandType::Text;
    text_command.node_id = node_id;
    text_command.rect = item_rect;
    apply_content_clip(text_command, node);
    text_command.text_origin = glm::vec2(
        item_rect.x + k_segmented_item_padding_x,
        item_rect.y + std::max(0.0f, (item_rect.height - line_height) * 0.5f)
    );
    text_command.text = node.segmented_control.options[index];
    text_command.font_id = resolve_ui_font_id(node, context);
    text_command.font_size = resolve_ui_font_size(node, context);
    text_command.color = selected
                             ? glm::vec4(0.98f, 1.0f, 1.0f, 1.0f)
                             : resolved.text_color;
    text_command.color.a *= resolved.opacity;
    document.draw_list().commands.push_back(std::move(text_command));
  }
}

} // namespace astralix::ui
