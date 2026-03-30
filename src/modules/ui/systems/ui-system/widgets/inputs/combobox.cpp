#include "systems/ui-system/widgets/inputs/combobox.hpp"

#include "systems/ui-system/widgets/inputs/text-input.hpp"

#include <algorithm>

namespace astralix::ui_system_core {
namespace {

void set_combobox_highlight(const Target &target, size_t index) {
  if (target.document == nullptr) {
    return;
  }

  auto *node = target.document->node(target.node_id);
  if (node == nullptr || node->type != ui::NodeType::Combobox ||
      node->combobox.options.empty()) {
    return;
  }

  const size_t clamped = std::min(index, node->combobox.options.size() - 1u);
  if (node->combobox.highlighted_index == clamped) {
    return;
  }

  target.document->set_combobox_highlighted_index(target.node_id, clamped);
}

} // namespace

void move_combobox_highlight(const Target &target, int direction) {
  if (target.document == nullptr) {
    return;
  }

  const auto *node = target.document->node(target.node_id);
  if (node == nullptr || node->type != ui::NodeType::Combobox ||
      node->combobox.options.empty()) {
    return;
  }

  const int max_index = static_cast<int>(node->combobox.options.size()) - 1;
  const int next_index = std::clamp(
      static_cast<int>(node->combobox.highlighted_index) + direction, 0,
      max_index
  );
  set_combobox_highlight(target, static_cast<size_t>(next_index));
}

bool confirm_combobox_option(
    const Target &target,
    size_t index,
    const ui::UILayoutContext &context,
    bool queue_callback
) {
  if (target.document == nullptr) {
    return false;
  }

  const auto *before_node = target.document->node(target.node_id);
  if (before_node == nullptr || before_node->type != ui::NodeType::Combobox ||
      before_node->combobox.options.empty()) {
    return false;
  }

  const size_t clamped =
      std::min(index, before_node->combobox.options.size() - 1u);
  const std::string value = before_node->combobox.options[clamped];
  const bool changed = before_node->text != value;

  target.document->set_text(target.node_id, value);
  set_text_input_selection_and_caret(
      target, value.size(), value.size(), context
  );
  target.document->set_combobox_open(target.node_id, false);

  if (queue_callback) {
    if (const auto *node = target.document->node(target.node_id);
        node != nullptr && node->on_select &&
        clamped < node->combobox.options.size()) {
      auto callback = node->on_select;
      auto label = node->combobox.options[clamped];
      target.document->queue_callback(
          [callback, clamped, label]() { callback(clamped, label); }
      );
    }
  }

  return changed;
}

} // namespace astralix::ui_system_core
