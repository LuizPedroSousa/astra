#include "layout/widgets/inputs/select.hpp"

#include "layout/common.hpp"

#include <algorithm>

namespace astralix::ui {
namespace {

std::string_view select_display_text(const UIDocument::UINode &node) {
  if (!node.select.options.empty() &&
      node.select.selected_index < node.select.options.size()) {
    return node.select.options[node.select.selected_index];
  }

  return node.placeholder;
}

} // namespace

glm::vec2 measure_select_size(
    const UIDocument::UINode &node,
    const UILayoutContext &context
) {
  float label_width = measure_label_width(node, context, node.placeholder);
  for (const auto &option : node.select.options) {
    label_width =
        std::max(label_width, measure_label_width(node, context, option));
  }

  const float line_height = measure_line_height(node, context);
  const float indicator = std::max(12.0f, node.style.control_indicator_size);
  return glm::vec2(
      node.style.padding.horizontal() + label_width + node.style.control_gap +
          indicator,
      node.style.padding.vertical() + std::max(line_height, indicator)
  );
}

void update_select_layout(
    UIDocument::UINode &node,
    const UILayoutContext &context
) {
  node.layout.select = UILayoutMetrics::SelectLayout{};
  if (!node.select.open || node.select.options.empty()) {
    return;
  }

  const float line_height = measure_line_height(node, context);
  const float row_height =
      std::max(line_height, node.style.control_indicator_size) + 10.0f;
  float popup_width = node.layout.bounds.width;
  for (const auto &option : node.select.options) {
    popup_width = std::max(
        popup_width,
        measure_label_width(node, context, option) +
            node.style.padding.horizontal() + node.style.control_indicator_size
    );
  }

  const float popup_height = row_height * node.select.options.size() + 8.0f;
  float popup_x = node.layout.bounds.x;
  float popup_y = node.layout.bounds.bottom() + 4.0f;

  if (popup_x + popup_width > context.viewport_size.x) {
    popup_x = std::max(0.0f, context.viewport_size.x - popup_width);
  }

  if (popup_y + popup_height > context.viewport_size.y) {
    popup_y = std::max(0.0f, node.layout.bounds.y - popup_height - 4.0f);
  }

  node.layout.select.popup_rect = UIRect{
      .x = popup_x,
      .y = popup_y,
      .width = popup_width,
      .height = popup_height,
  };

  node.layout.select.option_rects.reserve(node.select.options.size());
  for (size_t index = 0u; index < node.select.options.size(); ++index) {
    node.layout.select.option_rects.push_back(UIRect{
        .x = popup_x + 4.0f,
        .y = popup_y + 4.0f + row_height * static_cast<float>(index),
        .width = std::max(0.0f, popup_width - 8.0f),
        .height = row_height,
    });
  }
}

void append_select_field_commands(
    UIDocument &document,
    UINodeId node_id,
    const UIDocument::UINode &node,
    const UILayoutContext &context,
    const UIResolvedStyle &resolved
) {
  const UIRect content = node.layout.content_bounds;
  const float indicator = std::max(12.0f, node.style.control_indicator_size);
  const float line_height = measure_line_height(node, context);
  const bool has_selection =
      !node.select.options.empty() &&
      node.select.selected_index < node.select.options.size();
  const std::string_view label = select_display_text(node);

  const UIRect text_rect = resolve_single_line_text_rect(
      UIRect{
          .x = content.x,
          .y = content.y,
          .width = std::max(
              0.0f, content.width - indicator - node.style.control_gap
          ),
          .height = content.height,
      },
      line_height
  );
  append_text_commands(
      document,
      node_id,
      text_rect,
      node,
      context,
      resolved,
      label,
      has_selection ? resolved.text_color : resolved.placeholder_text_color,
      0.0f,
      false,
      false
  );

  UIDrawCommand arrow_command;
  arrow_command.type = DrawCommandType::Text;
  arrow_command.node_id = node_id;
  arrow_command.rect = UIRect{
      .x = std::max(content.x, content.right() - indicator),
      .y = content.y,
      .width = indicator,
      .height = content.height,
  };
  apply_content_clip(arrow_command, node);
  arrow_command.text_origin = glm::vec2(arrow_command.rect.x, text_rect.y);
  arrow_command.text = node.select.open ? "^" : "v";
  arrow_command.font_id = resolve_ui_font_id(node, context);
  arrow_command.font_size = resolve_ui_font_size(node, context);
  arrow_command.color = resolved.text_color;
  arrow_command.color.a *= resolved.opacity;
  document.draw_list().commands.push_back(std::move(arrow_command));
}

void append_select_overlay_commands(
    UIDocument &document,
    UINodeId node_id,
    const UILayoutContext &context
) {
  const auto *node = document.node(node_id);
  if (node == nullptr || node->type != NodeType::Select || !node->visible ||
      !node->enabled || !node->select.open ||
      node->layout.select.popup_rect.width <= 0.0f ||
      node->layout.select.popup_rect.height <= 0.0f) {
    return;
  }

  const UIResolvedStyle resolved =
      resolve_style(node->style, node->paint_state, true);

  UIDrawCommand popup_command;
  popup_command.type = DrawCommandType::Rect;
  popup_command.node_id = node_id;
  popup_command.rect = node->layout.select.popup_rect;
  popup_command.color = glm::vec4(0.141f, 0.157f, 0.200f, 0.98f) *
                        glm::vec4(1.0f, 1.0f, 1.0f, resolved.opacity);
  popup_command.border_color =
      resolved.border_color * glm::vec4(1.0f, 1.0f, 1.0f, resolved.opacity);
  popup_command.border_width = std::max(1.0f, resolved.border_width);
  popup_command.border_radius = std::max(8.0f, resolved.border_radius);
  document.draw_list().commands.push_back(std::move(popup_command));

  const float font_size = resolve_ui_font_size(*node, context);
  const float line_height = measure_line_height(*node, context);
  for (size_t index = 0u; index < node->layout.select.option_rects.size();
       ++index) {
    const UIRect option_rect = node->layout.select.option_rects[index];
    const bool selected = index == node->select.selected_index;
    const bool hovered = node->layout.select.hovered_option_index.has_value() &&
                         *node->layout.select.hovered_option_index == index;
    const bool highlighted = index == node->select.highlighted_index;

    if (selected || hovered || highlighted) {
      UIDrawCommand option_bg;
      option_bg.type = DrawCommandType::Rect;
      option_bg.node_id = node_id;
      option_bg.rect = option_rect;
      option_bg.color = node->style.accent_color * glm::vec4(
                                                       1.0f,
                                                       1.0f,
                                                       1.0f,
                                                       (hovered ? 0.28f
                                                                : highlighted ? 0.22f
                                                                              : 0.18f) *
                                                           resolved.opacity
                                                   );
      option_bg.border_radius = 6.0f;
      document.draw_list().commands.push_back(std::move(option_bg));
    }

    UIDrawCommand option_text;
    option_text.type = DrawCommandType::Text;
    option_text.node_id = node_id;
    option_text.rect = option_rect;
    option_text.text_origin = glm::vec2(
        option_rect.x + node->style.padding.left,
        option_rect.y +
            std::max(0.0f, (option_rect.height - line_height) * 0.5f)
    );
    option_text.text = node->select.options[index];
    option_text.font_id = resolve_ui_font_id(*node, context);
    option_text.font_size = font_size;
    option_text.color = resolved.text_color;
    option_text.color.a *= resolved.opacity;
    document.draw_list().commands.push_back(std::move(option_text));
  }
}

} // namespace astralix::ui
