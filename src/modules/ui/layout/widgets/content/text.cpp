#include "layout/widgets/content/text.hpp"

#include "layout/common.hpp"

namespace astralix::ui {

glm::vec2 measure_text_size(
    const UIDocument::UINode &node,
    const UILayoutContext &context
) {
  const float font_size = resolve_ui_font_size(node, context);
  if (node.text.empty()) {
    return glm::vec2(0.0f);
  }

  auto font = resolve_ui_font(node, context);
  if (font == nullptr) {
    return glm::vec2(
        measure_ui_text_width(node, context, node.text),
        measure_ui_line_height(node, context)
    );
  }

  return font->measure_text(node.text, font_size);
}

void append_text_node_commands(
    UIDocument &document,
    UINodeId node_id,
    const UIDocument::UINode &node,
    const UILayoutContext &context,
    const UIResolvedStyle &resolved
) {
  if (node.text.empty()) {
    return;
  }

  append_text_commands(
      document,
      node_id,
      node.layout.bounds,
      node,
      context,
      resolved,
      node.text,
      resolved.text_color,
      0.0f,
      !node.selection.empty(),
      node.paint_state.focused && node.caret.active
  );
}

} // namespace astralix::ui
