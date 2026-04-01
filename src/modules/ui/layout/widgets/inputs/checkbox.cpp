#include "layout/widgets/inputs/checkbox.hpp"

#include "layout/common.hpp"

#include <algorithm>

namespace astralix::ui {

glm::vec2 measure_checkbox_size(
    const UIDocument::UINode &node,
    const UILayoutContext &context
) {
  const float indicator = std::max(8.0f, node.style.control_indicator_size);
  const float line_height = measure_line_height(node, context);
  const float label_width =
      node.text.empty() ? 0.0f : measure_label_width(node, context, node.text);
  const float gap = node.text.empty() ? 0.0f : node.style.control_gap;

  return glm::vec2(
      node.style.padding.horizontal() + indicator + gap + label_width,
      node.style.padding.vertical() + std::max(indicator, line_height)
  );
}

void update_checkbox_layout(
    UIDocument::UINode &node,
    const UILayoutContext &context
) {
  node.layout.checkbox = UILayoutMetrics::CheckboxLayout{};

  const float indicator = std::max(8.0f, node.style.control_indicator_size);
  const float line_height = measure_line_height(node, context);
  const UIRect content = node.layout.content_bounds;
  const float indicator_y =
      content.y + std::max(0.0f, (content.height - indicator) * 0.5f);

  node.layout.checkbox.indicator_rect = UIRect{
      .x = content.x,
      .y = indicator_y,
      .width = indicator,
      .height = indicator,
  };

  const float label_x = node.layout.checkbox.indicator_rect.right() +
                        (node.text.empty() ? 0.0f : node.style.control_gap);
  node.layout.checkbox.label_rect = resolve_single_line_text_rect(
      UIRect{
          .x = label_x,
          .y = content.y,
          .width = std::max(0.0f, content.right() - label_x),
          .height = content.height,
      },
      line_height
  );
}

void append_checkbox_commands(
    UIDocument &document,
    UINodeId node_id,
    const UIDocument::UINode &node,
    const UILayoutContext &context,
    const UIResolvedStyle &resolved
) {
  const UIRect indicator = node.layout.checkbox.indicator_rect;
  if (indicator.width > 0.0f && indicator.height > 0.0f) {
    UIDrawCommand indicator_command;
    indicator_command.type = DrawCommandType::Rect;
    indicator_command.node_id = node_id;
    indicator_command.rect = indicator;
    apply_content_clip(indicator_command, node);
    indicator_command.color = glm::vec4(0.02f, 0.05f, 0.1f, 0.92f) *
                              glm::vec4(1.0f, 1.0f, 1.0f, resolved.opacity);
    indicator_command.border_color =
        (node.checkbox.checked ? node.style.accent_color
                               : resolved.border_color) *
        glm::vec4(1.0f, 1.0f, 1.0f, resolved.opacity);
    indicator_command.border_width = 1.0f;
    indicator_command.border_radius = 5.0f;
    document.draw_list().commands.push_back(std::move(indicator_command));

    if (node.checkbox.checked) {
      const float inset = std::max(3.0f, indicator.width * 0.2f);
      UIDrawCommand mark_command;
      mark_command.type = DrawCommandType::Rect;
      mark_command.node_id = node_id;
      mark_command.rect = inset_rect(indicator, UIEdges::all(inset));
      apply_content_clip(mark_command, node);
      mark_command.color = node.style.accent_color *
                           glm::vec4(1.0f, 1.0f, 1.0f, resolved.opacity);
      mark_command.border_radius = 3.0f;
      document.draw_list().commands.push_back(std::move(mark_command));
    }
  }

  if (!node.text.empty()) {
    append_text_commands(
        document,
        node_id,
        node.layout.checkbox.label_rect,
        node,
        context,
        resolved,
        node.text,
        resolved.text_color,
        0.0f,
        false,
        false
    );
  }
}

} // namespace astralix::ui
