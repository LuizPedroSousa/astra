#pragma once

#include "document/document.hpp"
#include "managers/resource-manager.hpp"
#include "resources/font.hpp"
#include <algorithm>
#include <cmath>
#include <string_view>

namespace astralix::ui {

inline ResourceDescriptorID
resolve_ui_font_id(const UIStyle &style, const ResourceDescriptorID &default_font_id) {
  return style.font_id.empty() ? default_font_id : style.font_id;
}

inline float resolve_ui_font_size(const UIStyle &style, float default_font_size) {
  return style.font_size > 0.0f ? style.font_size : default_font_size;
}

inline uint32_t resolve_ui_font_pixel_size(float font_size) {
  return static_cast<uint32_t>(std::max(1.0f, std::round(font_size)));
}

inline Ref<Font> resolve_ui_font(const ResourceDescriptorID &font_id) {
  if (font_id.empty()) {
    return nullptr;
  }

  return resource_manager()->get_by_descriptor_id<Font>(font_id);
}

inline ResourceDescriptorID resolve_ui_font_id(const UIDocument::UINode &node, const UILayoutContext &context) {
  return resolve_ui_font_id(node.style, context.default_font_id);
}

inline float resolve_ui_font_size(const UIDocument::UINode &node, const UILayoutContext &context) {
  return resolve_ui_font_size(node.style, context.default_font_size);
}

inline Ref<Font> resolve_ui_font(const UIDocument::UINode &node, const UILayoutContext &context) {
  return resolve_ui_font(resolve_ui_font_id(node, context));
}

inline float ui_glyph_advance(const Font &font, char character, uint32_t pixel_size) {
  auto glyph = font.glyph(character, pixel_size);
  if (!glyph.has_value()) {
    return 0.0f;
  }

  return static_cast<float>(glyph->advance >> 6);
}

inline float measure_ui_text_width(const Font &font, std::string_view text, uint32_t pixel_size) {
  float width = 0.0f;
  for (const char character : text) {
    width += ui_glyph_advance(font, character, pixel_size);
  }

  return width;
}

inline float measure_ui_text_width(const ResourceDescriptorID &font_id, float font_size, std::string_view text) {
  auto font = resolve_ui_font(font_id);
  if (font == nullptr) {
    return static_cast<float>(text.size()) * font_size * 0.5f;
  }

  return measure_ui_text_width(*font, text, resolve_ui_font_pixel_size(font_size));
}

inline float measure_ui_text_width(const UIDocument::UINode &node, const UILayoutContext &context, std::string_view text) {
  return measure_ui_text_width(resolve_ui_font_id(node, context), resolve_ui_font_size(node, context), text);
}

inline float measure_ui_line_height(const ResourceDescriptorID &font_id, float font_size) {
  auto font = resolve_ui_font(font_id);
  return font != nullptr ? font->line_height(font_size) : font_size;
}

inline float measure_ui_line_height(const UIDocument::UINode &node, const UILayoutContext &context) {
  return measure_ui_line_height(resolve_ui_font_id(node, context), resolve_ui_font_size(node, context));
}

} // namespace astralix::ui
