#include "layout/widgets/inputs/combobox.hpp"

#include "layout/common.hpp"
#include "layout/widgets/inputs/text-input.hpp"

#include <algorithm>

namespace astralix::ui {

glm::vec2 measure_combobox_size(
    const UIDocument::UINode &node,
    const UILayoutContext &context
) {
  auto font = resolve_ui_font(node, context);
  const float font_size = resolve_ui_font_size(node, context);
  const uint32_t resolved_font_size = resolve_ui_font_pixel_size(font_size);

  float text_width = 0.0f;
  float line_height = font_size;
  if (font != nullptr) {
    text_width = std::max(
        measure_ui_text_width(*font, node.text, resolved_font_size),
        measure_ui_text_width(*font, node.placeholder, resolved_font_size)
    );
    text_width = std::max(
        text_width,
        measure_ui_text_width(*font, node.autocomplete_text, resolved_font_size)
    );
    for (const auto &option : node.combobox.options) {
      text_width = std::max(
          text_width,
          measure_ui_text_width(*font, option, resolved_font_size)
      );
    }
    line_height = font->line_height(font_size);
  }

  const float indicator = std::max(12.0f, node.style.control_indicator_size);
  return glm::vec2(
      text_width + node.style.padding.horizontal() + node.style.control_gap +
          indicator,
      std::max(line_height, indicator) + node.style.padding.vertical()
  );
}

void update_combobox_layout(
    UIDocument::UINode &node,
    const UILayoutContext &context
) {
  node.layout.combobox = UILayoutMetrics::ComboboxLayout{};
  if (!node.combobox.open || node.combobox.options.empty()) {
    return;
  }

  const float line_height = measure_line_height(node, context);
  const float row_height =
      std::max(line_height, node.style.control_indicator_size) + 10.0f;
  const size_t visible_rows =
      std::min<size_t>(8u, node.combobox.options.size());
  float popup_width = node.layout.bounds.width;
  for (const auto &option : node.combobox.options) {
    popup_width = std::max(
        popup_width,
        measure_label_width(node, context, option) +
            node.style.padding.horizontal() + node.style.control_indicator_size
    );
  }

  const float popup_height = row_height * static_cast<float>(visible_rows) + 8.0f;
  float popup_x = node.layout.bounds.x;
  float popup_y = node.layout.bounds.bottom() + 4.0f;

  if (popup_x + popup_width > context.viewport_size.x) {
    popup_x = std::max(0.0f, context.viewport_size.x - popup_width);
  }

  if (popup_y + popup_height > context.viewport_size.y) {
    popup_y = std::max(0.0f, node.layout.bounds.y - popup_height - 4.0f);
  }

  node.layout.combobox.popup_rect = UIRect{
      .x = popup_x,
      .y = popup_y,
      .width = popup_width,
      .height = popup_height,
  };

  node.layout.combobox.option_rects.reserve(visible_rows);
  for (size_t index = 0u; index < visible_rows; ++index) {
    node.layout.combobox.option_rects.push_back(UIRect{
        .x = popup_x + 4.0f,
        .y = popup_y + 4.0f + row_height * static_cast<float>(index),
        .width = std::max(0.0f, popup_width - 8.0f),
        .height = row_height,
    });
  }
}

void append_combobox_field_commands(
    UIDocument &document,
    UINodeId node_id,
    const UIDocument::UINode &node,
    const UILayoutContext &context,
    const UIResolvedStyle &resolved
) {
  const UIRect content = node.layout.content_bounds;
  const float indicator = std::max(12.0f, node.style.control_indicator_size);
  const float line_height = measure_line_height(node, context);
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
  append_editable_text_commands(
      document,
      node_id,
      node,
      context,
      resolved,
      text_rect
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
  arrow_command.text = node.combobox.open ? "^" : "v";
  arrow_command.font_id = resolve_ui_font_id(node, context);
  arrow_command.font_size = resolve_ui_font_size(node, context);
  arrow_command.color = resolved.text_color;
  arrow_command.color.a *= resolved.opacity;
  document.draw_list().commands.push_back(std::move(arrow_command));
}

void append_combobox_overlay_commands(
    UIDocument &document,
    UINodeId node_id,
    const UILayoutContext &context
) {
  const auto *node = document.node(node_id);
  if (node == nullptr || node->type != NodeType::Combobox || !node->visible ||
      !node->enabled || !node->combobox.open ||
      node->layout.combobox.popup_rect.width <= 0.0f ||
      node->layout.combobox.popup_rect.height <= 0.0f) {
    return;
  }

  const UIResolvedStyle resolved =
      resolve_style(node->style, node->paint_state, true);

  UIDrawCommand popup_command;
  popup_command.type = DrawCommandType::Rect;
  popup_command.node_id = node_id;
  popup_command.rect = node->layout.combobox.popup_rect;
  popup_command.color = glm::vec4(0.141f, 0.157f, 0.200f, 0.98f) *
                        glm::vec4(1.0f, 1.0f, 1.0f, resolved.opacity);
  popup_command.border_color =
      resolved.border_color * glm::vec4(1.0f, 1.0f, 1.0f, resolved.opacity);
  popup_command.border_width = std::max(1.0f, resolved.border_width);
  popup_command.border_radius = std::max(8.0f, resolved.border_radius);
  document.draw_list().commands.push_back(std::move(popup_command));

  const float font_size = resolve_ui_font_size(*node, context);
  const float line_height = measure_line_height(*node, context);
  for (size_t index = 0u; index < node->layout.combobox.option_rects.size();
       ++index) {
    const UIRect option_rect = node->layout.combobox.option_rects[index];
    const bool hovered = node->layout.combobox.hovered_option_index.has_value() &&
                         *node->layout.combobox.hovered_option_index == index;
    const bool highlighted = index == node->combobox.highlighted_index;

    if (hovered || highlighted) {
      UIDrawCommand option_bg;
      option_bg.type = DrawCommandType::Rect;
      option_bg.node_id = node_id;
      option_bg.rect = option_rect;
      option_bg.color = node->style.accent_color * glm::vec4(
                                                       1.0f,
                                                       1.0f,
                                                       1.0f,
                                                       (hovered ? 0.28f : 0.22f) *
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
    option_text.text = node->combobox.options[index];
    option_text.font_id = resolve_ui_font_id(*node, context);
    option_text.font_size = font_size;
    option_text.color = resolved.text_color;
    option_text.color.a *= resolved.opacity;
    document.draw_list().commands.push_back(std::move(option_text));
  }
}

} // namespace astralix::ui
