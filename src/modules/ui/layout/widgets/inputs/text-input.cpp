#include "layout/widgets/inputs/text-input.hpp"

#include "layout/common.hpp"

#include <algorithm>
#include <cctype>

namespace astralix::ui {
namespace {

bool starts_with_case_insensitive(
    std::string_view text,
    std::string_view prefix
) {
  if (prefix.size() > text.size()) {
    return false;
  }

  for (size_t index = 0u; index < prefix.size(); ++index) {
    if (std::tolower(static_cast<unsigned char>(text[index])) !=
        std::tolower(static_cast<unsigned char>(prefix[index]))) {
      return false;
    }
  }

  return true;
}

} // namespace

glm::vec2 measure_text_input_size(
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
    line_height = font->line_height(font_size);
  }

  return glm::vec2(
      text_width + node.style.padding.horizontal(),
      line_height + node.style.padding.vertical()
  );
}

void append_editable_text_commands(
    UIDocument &document,
    UINodeId node_id,
    const UIDocument::UINode &node,
    const UILayoutContext &context,
    const UIResolvedStyle &resolved,
    const UIRect &text_rect
) {
  auto font = resolve_ui_font(node, context);
  const float font_size = resolve_ui_font_size(node, context);
  const float line_height =
      font != nullptr ? font->line_height(font_size) : font_size;

  if (!node.text.empty()) {
    append_text_commands(
        document,
        node_id,
        text_rect,
        node,
        context,
        resolved,
        node.text,
        resolved.text_color,
        node.text_scroll_x,
        node.paint_state.focused && !node.selection.empty(),
        false
    );

    const bool show_autocomplete =
        font != nullptr && node.paint_state.focused && node.selection.empty() &&
        node.caret.active && node.caret.index == node.text.size() &&
        node.autocomplete_text.size() > node.text.size() &&
        starts_with_case_insensitive(node.autocomplete_text, node.text);
    if (show_autocomplete) {
      const uint32_t resolved_font_size = resolve_ui_font_pixel_size(font_size);
      const float prefix_width = measure_text_prefix_advance(
          node.text, node.text.size(), [&](char character) {
            return ui_glyph_advance(*font, character, resolved_font_size);
          }
      );

      UIDrawCommand autocomplete_command;
      autocomplete_command.type = DrawCommandType::Text;
      autocomplete_command.node_id = node_id;
      autocomplete_command.rect = text_rect;
      apply_content_clip(autocomplete_command, node);
      autocomplete_command.text_origin = glm::vec2(
          text_rect.x - node.text_scroll_x + prefix_width,
          text_rect.y
      );
      autocomplete_command.text =
          node.autocomplete_text.substr(node.text.size());
      autocomplete_command.font_id = resolve_ui_font_id(node, context);
      autocomplete_command.font_size = font_size;
      autocomplete_command.color = resolved.placeholder_text_color;
      autocomplete_command.color.a *= resolved.opacity;
      document.draw_list().commands.push_back(std::move(autocomplete_command));
    }

    if (node.paint_state.focused && node.caret.active && node.caret.visible &&
        font != nullptr) {
      const uint32_t resolved_font_size = resolve_ui_font_pixel_size(font_size);
      const float caret_x = measure_text_prefix_advance(
          node.text, node.caret.index, [&](char character) {
            return ui_glyph_advance(*font, character, resolved_font_size);
          }
      );

      UIDrawCommand caret_command;
      caret_command.type = DrawCommandType::Rect;
      caret_command.node_id = node_id;
      caret_command.rect = UIRect{
          .x = text_rect.x - node.text_scroll_x + caret_x,
          .y = text_rect.y,
          .width = 1.0f,
          .height = line_height,
      };
      apply_content_clip(caret_command, node);
      caret_command.color = resolved.caret_color;
      document.draw_list().commands.push_back(std::move(caret_command));
    }
    return;
  }

  if (!node.placeholder.empty()) {
    UIDrawCommand placeholder_command;
    placeholder_command.type = DrawCommandType::Text;
    placeholder_command.node_id = node_id;
    placeholder_command.rect = text_rect;
    apply_content_clip(placeholder_command, node);
    placeholder_command.text_origin = glm::vec2(text_rect.x, text_rect.y);
    placeholder_command.text = node.placeholder;
    placeholder_command.font_id = resolve_ui_font_id(node, context);
    placeholder_command.font_size = font_size;
    placeholder_command.color = resolved.placeholder_text_color;
    placeholder_command.color.a *= resolved.opacity;
    document.draw_list().commands.push_back(std::move(placeholder_command));
  }

  if (node.paint_state.focused && node.caret.active && node.caret.visible) {
    UIDrawCommand caret_command;
    caret_command.type = DrawCommandType::Rect;
    caret_command.node_id = node_id;
    caret_command.rect = UIRect{
        .x = text_rect.x,
        .y = text_rect.y,
        .width = 1.0f,
        .height = line_height,
    };
    apply_content_clip(caret_command, node);
    caret_command.color = resolved.caret_color;
    document.draw_list().commands.push_back(std::move(caret_command));
  }
}

} // namespace astralix::ui
