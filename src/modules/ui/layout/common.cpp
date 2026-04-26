#include "layout/common.hpp"

#include <algorithm>
#include <cmath>

namespace astralix::ui {

float clamp_dimension(
    float value,
    UILength min_value,
    UILength max_value,
    float basis,
    float rem_basis,
    float intrinsic_value
) {
  if (min_value.unit != UILengthUnit::Auto) {
    const float min_dimension =
        resolve_length(min_value, basis, rem_basis, intrinsic_value);
    value = std::max(value, min_dimension);
  }

  if (max_value.unit != UILengthUnit::Auto) {
    const float max_dimension =
        resolve_length(max_value, basis, rem_basis, intrinsic_value);
    value = std::min(value, max_dimension);
  }

  return std::max(0.0f, value);
}

float resolve_length(
    UILength value,
    float basis,
    float rem_basis,
    float auto_value
) {
  switch (value.unit) {
    case UILengthUnit::Pixels:
      return value.value;
    case UILengthUnit::Percent:
      return basis * value.value;
    case UILengthUnit::Rem:
      return rem_basis * value.value;
    case UILengthUnit::MaxContent:
    case UILengthUnit::Auto:
    default:
      return auto_value;
  }
}

float measure_label_width(
    const UIDocument::UINode &node,
    const UILayoutContext &context,
    std::string_view text
) {
  auto font = resolve_ui_font(node, context);
  const float font_size = resolve_ui_font_size(node, context);
  const uint32_t resolved_font_size = resolve_ui_font_pixel_size(font_size);
  if (font == nullptr) {
    return static_cast<float>(text.size()) * font_size * 0.5f;
  }

  return measure_ui_text_width(*font, text, resolved_font_size);
}

float measure_line_height(
    const UIDocument::UINode &node,
    const UILayoutContext &context
) {
  return measure_ui_line_height(node, context);
}

UIRect
resolve_single_line_text_rect(const UIRect &content_bounds, float line_height) {
  const float resolved_height = std::max(content_bounds.height, line_height);
  const float y_offset =
      std::max(0.0f, (content_bounds.height - line_height) * 0.5f);

  return UIRect{
      .x = content_bounds.x,
      .y = content_bounds.y + y_offset,
      .width = content_bounds.width,
      .height = resolved_height,
  };
}

void apply_self_clip(UIDrawCommand &command, const UIDocument::UINode &node) {
  command.has_clip = node.layout.has_clip;
  command.clip_rect = node.layout.clip_bounds;
}

void apply_content_clip(
    UIDrawCommand &command,
    const UIDocument::UINode &node
) {
  command.has_clip = node.layout.has_content_clip;
  command.clip_rect = node.layout.content_clip_bounds;
}

std::optional<UIRect> child_clip_rect(const UIDocument::UINode &node) {
  if (!node.layout.has_content_clip) {
    return std::nullopt;
  }

  return node.layout.content_clip_bounds;
}

void append_text_commands(
    UIDocument &document,
    UINodeId node_id,
    const UIRect &text_rect,
    const UIDocument::UINode &node,
    const UILayoutContext &context,
    const UIResolvedStyle &resolved,
    std::string_view text,
    glm::vec4 text_color,
    float text_scroll_x,
    bool draw_selection,
    bool draw_caret
) {
  if (text.empty()) {
    return;
  }

  auto font = resolve_ui_font(node, context);
  const float font_size = resolve_ui_font_size(node, context);
  const glm::vec2 text_origin(text_rect.x - text_scroll_x, text_rect.y);

  if (font != nullptr && draw_selection && !node.selection.empty()) {
    const uint32_t resolved_font_size =
        static_cast<uint32_t>(std::max(1.0f, std::round(font_size)));
    const auto &glyphs = font->glyphs(resolved_font_size);
    const auto &glyph_lut = font->glyph_lut(resolved_font_size);
    const float start_x = measure_text_prefix_advance(
        node.text, node.selection.start(), [&](char character) {
          return ui_glyph_advance(glyphs, glyph_lut, character);
        }
    );
    const float end_x = measure_text_prefix_advance(
        node.text, node.selection.end(), [&](char character) {
          return ui_glyph_advance(glyphs, glyph_lut, character);
        }
    );

    if (end_x > start_x) {
      UIDrawCommand selection_command;
      selection_command.type = DrawCommandType::Rect;
      selection_command.node_id = node_id;
      selection_command.rect = UIRect{
          .x = text_origin.x + start_x,
          .y = text_rect.y,
          .width = end_x - start_x,
          .height = std::max(text_rect.height, font->line_height(font_size)),
      };
      apply_content_clip(selection_command, node);
      selection_command.color = resolved.selection_color;
      document.draw_list().commands.push_back(std::move(selection_command));
    }
  }

  UIDrawCommand command;
  command.type = DrawCommandType::Text;
  command.node_id = node_id;
  command.rect = text_rect;
  command.text_origin = text_origin;
  command.text = std::string(text);
  command.font_id = resolve_ui_font_id(node, context);
  command.font_size = font_size;
  command.color = text_color;
  command.color.a *= resolved.opacity;
  apply_content_clip(command, node);
  document.draw_list().commands.push_back(std::move(command));

  if (font != nullptr && draw_caret && node.caret.active && node.caret.visible) {
    const uint32_t resolved_font_size =
        static_cast<uint32_t>(std::max(1.0f, std::round(font_size)));
    const auto &glyphs = font->glyphs(resolved_font_size);
    const auto &glyph_lut = font->glyph_lut(resolved_font_size);
    const float caret_x = measure_text_prefix_advance(
        node.text, node.caret.index, [&](char character) {
          return ui_glyph_advance(glyphs, glyph_lut, character);
        }
    );

    UIDrawCommand caret_command;
    caret_command.type = DrawCommandType::Rect;
    caret_command.node_id = node_id;
    caret_command.rect = UIRect{
        .x = text_origin.x + caret_x,
        .y = text_rect.y,
        .width = 1.0f,
        .height = std::max(text_rect.height, font->line_height(font_size)),
    };
    apply_content_clip(caret_command, node);
    caret_command.color = resolved.caret_color;
    document.draw_list().commands.push_back(std::move(caret_command));
  }
}

} // namespace astralix::ui
