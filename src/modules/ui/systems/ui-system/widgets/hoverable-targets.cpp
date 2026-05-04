#include "systems/ui-system/core.hpp"

#include "foundations.hpp"

namespace astralix::ui_system_core {
namespace {

bool is_action_hoverable_type(ui::NodeType type) {
  return type == ui::NodeType::Pressable;
}

bool is_input_hoverable_type(ui::NodeType type) {
  return type == ui::NodeType::Checkbox || type == ui::NodeType::Slider ||
         type == ui::NodeType::Select || type == ui::NodeType::Combobox ||
         type == ui::NodeType::TextInput;
}

bool is_layout_hoverable_type(ui::NodeType type) {
  return type == ui::NodeType::Splitter;
}

bool is_multi_option_hoverable_type(ui::NodeType type) {
  return type == ui::NodeType::SegmentedControl ||
         type == ui::NodeType::ChipGroup;
}

bool is_hoverable_widget_type(ui::NodeType type) {
  return is_action_hoverable_type(type) || is_input_hoverable_type(type) ||
         is_layout_hoverable_type(type) ||
         is_multi_option_hoverable_type(type);
}

bool has_custom_interaction_callback(const ui::UIDocument::UINode &node) {
  return static_cast<bool>(node.on_pointer_event) ||
         static_cast<bool>(node.on_custom_hit_test);
}

} // namespace

std::optional<Target> find_hoverable_target(
    const RootEntry &entry,
    ui::UINodeId node_id
) {
  return map_to_ancestor_target(
      entry,
      node_id,
      [](const ui::UIDocument &document, ui::UINodeId current) {
        while (current != ui::k_invalid_node_id) {
          const auto *node = document.node(current);
          if (node == nullptr) {
            return std::optional<ui::UINodeId>{};
          }

          if ((is_hoverable_widget_type(node->type) ||
               has_custom_interaction_callback(*node)) &&
              ui::node_chain_allows_interaction(document, current)) {
            return std::optional<ui::UINodeId>{current};
          }

          current = node->parent;
        }

        return std::optional<ui::UINodeId>{};
      }
  );
}

} // namespace astralix::ui_system_core
