#include "systems/ui-system/widgets/inputs/text-input.hpp"

#include "foundations.hpp"
#include "text-metrics.hpp"
#include <algorithm>
#include <cmath>

namespace astralix::ui_system_core {
namespace {

bool node_supports_text_editing(const ui::UIDocument::UINode &node) {
  return node.type == ui::NodeType::TextInput ||
         node.type == ui::NodeType::Combobox;
}

ui::UIRect resolve_single_line_text_rect(const ui::UIRect &content_bounds, float line_height) {
  const float resolved_height = std::max(content_bounds.height, line_height);
  const float y_offset =
      std::max(0.0f, (content_bounds.height - line_height) * 0.5f);

  return ui::UIRect{
      .x = content_bounds.x,
      .y = content_bounds.y + y_offset,
      .width = content_bounds.width,
      .height = resolved_height,
  };
}

ui::UIRect text_input_text_rect(const ui::UIDocument::UINode &node, const ui::UILayoutContext &context) {
  auto font = ui::resolve_ui_font(node, context);
  const float font_size = ui::resolve_ui_font_size(node, context);
  const float line_height =
      font != nullptr ? font->line_height(font_size) : font_size;
  return resolve_single_line_text_rect(node.layout.content_bounds, line_height);
}

void queue_text_input_change(const Target &target) {
  if (target.document == nullptr) {
    return;
  }

  const auto *node = target.document->node(target.node_id);
  if (node == nullptr || !node->on_change) {
    return;
  }

  auto callback = node->on_change;
  auto value = node->text;
  target.document->queue_callback([callback, value]() { callback(value); });
}

} // namespace

size_t text_input_index_from_pointer(const ui::UIDocument::UINode &node, const ui::UILayoutContext &context, glm::vec2 point) {
  auto font = ui::resolve_ui_font(node, context);
  if (font == nullptr) {
    return point.x <= node.layout.content_bounds.x ? 0u : node.text.size();
  }

  const float font_size = ui::resolve_ui_font_size(node, context);
  const uint32_t resolved_font_size = ui::resolve_ui_font_pixel_size(font_size);
  const ui::UIRect text_rect = text_input_text_rect(node, context);
  const float local_x = point.x - text_rect.x + node.text_scroll_x;

  return ui::nearest_text_index(node.text, local_x, [&](char character) {
    return ui::ui_glyph_advance(*font, character, resolved_font_size);
  });
}

void sync_text_input_scroll(const Target &target, const ui::UILayoutContext &context) {
  if (target.document == nullptr) {
    return;
  }

  auto *node = target.document->node(target.node_id);
  if (node == nullptr || !node_supports_text_editing(*node)) {
    return;
  }

  const float viewport_width = node->layout.content_bounds.width;
  float next_scroll_x = 0.0f;

  auto font = ui::resolve_ui_font(*node, context);
  if (font != nullptr && viewport_width > 0.0f) {
    const float font_size = ui::resolve_ui_font_size(*node, context);
    const uint32_t resolved_font_size =
        ui::resolve_ui_font_pixel_size(font_size);
    const float content_width =
        ui::measure_ui_text_width(*font, node->text, resolved_font_size);
    const float caret_x = ui::measure_text_prefix_advance(
        node->text, node->caret.index, [&](char character) {
          return ui::ui_glyph_advance(*font, character, resolved_font_size);
        }
    );

    next_scroll_x = ui::scroll_x_to_keep_range_visible(
        node->text_scroll_x, caret_x, caret_x + 1.0f, content_width, viewport_width
    );
  }

  if (node->text_scroll_x != next_scroll_x) {
    node->text_scroll_x = next_scroll_x;
    target.document->mark_paint_dirty();
  }
}

void set_text_input_selection_and_caret(const Target &target, size_t anchor, size_t focus, const ui::UILayoutContext &context) {
  if (target.document == nullptr) {
    return;
  }

  target.document->set_text_selection(
      target.node_id, ui::UITextSelection{.anchor = anchor, .focus = focus}
  );
  target.document->set_caret(target.node_id, focus, true);
  sync_text_input_scroll(target, context);
}

void focus_text_input(const Target &target, const ui::UILayoutContext &context, bool select_all) {
  if (target.document == nullptr) {
    return;
  }

  const auto *node = target.document->node(target.node_id);
  if (node == nullptr || !node_supports_text_editing(*node)) {
    return;
  }

  const size_t caret_index =
      select_all ? node->text.size()
                 : ui::clamp_text_index(node->text, node->caret.index);
  const size_t anchor_index = select_all ? 0u : caret_index;
  set_text_input_selection_and_caret(target, anchor_index, caret_index, context);
}

std::string selected_text(const ui::UIDocument::UINode &node) {
  if (node.selection.empty()) {
    return {};
  }

  return node.text.substr(node.selection.start(), node.selection.end() - node.selection.start());
}

std::pair<size_t, size_t> edit_range(const ui::UIDocument::UINode &node) {
  if (!node.selection.empty()) {
    return {node.selection.start(), node.selection.end()};
  }

  return {node.caret.index, node.caret.index};
}

bool apply_text_input_value(const Target &target, std::string next_text, size_t caret_index, const ui::UILayoutContext &context, bool queue_change) {
  if (target.document == nullptr) {
    return false;
  }

  const auto *current = target.document->node(target.node_id);
  if (current == nullptr || !node_supports_text_editing(*current)) {
    return false;
  }

  const bool changed = current->text != next_text;
  target.document->set_text(target.node_id, std::move(next_text));
  set_text_input_selection_and_caret(target, caret_index, caret_index, context);

  if (changed && queue_change) {
    queue_text_input_change(target);
  }

  return changed;
}

} // namespace astralix::ui_system_core
