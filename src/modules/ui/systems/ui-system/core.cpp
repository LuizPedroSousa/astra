#include "systems/ui-system/core.hpp"

#include "managers/window-manager.hpp"
#include "foundations.hpp"
#include <algorithm>
#include <limits>

namespace astralix::ui_system_core {

bool same_target(const std::optional<Target> &lhs,
                 const std::optional<Target> &rhs) {
  if (!lhs.has_value() || !rhs.has_value()) {
    return !lhs.has_value() && !rhs.has_value();
  }

  return lhs->entity_id == rhs->entity_id && lhs->node_id == rhs->node_id &&
         lhs->document == rhs->document;
}

ui::UILayoutContext make_context(const RootEntry &entry,
                                 const glm::vec2 &viewport_size) {
  return ui::UILayoutContext{
      .viewport_size = viewport_size,
      .default_font_id = entry.root->default_font_id,
      .default_font_size = entry.root->default_font_size,
  };
}

bool target_uses_root(const RootEntry &entry, const Target &target) {
  return entry.entity_id == target.entity_id &&
         entry.document == target.document;
}

const RootEntry *find_root_entry(const std::vector<RootEntry> &roots,
                                 const Target &target) {
  auto it =
      std::find_if(roots.begin(), roots.end(), [&](const RootEntry &entry) {
        return target_uses_root(entry, target);
      });
  return it != roots.end() ? &*it : nullptr;
}

bool target_available(const std::vector<RootEntry> &roots,
                      const std::optional<Target> &target,
                      bool require_input_enabled) {
  if (!target.has_value() || target->document == nullptr) {
    return false;
  }

  const auto *node = target->document->node(target->node_id);
  if (node == nullptr ||
      !ui::node_chain_allows_interaction(*target->document, target->node_id)) {
    return false;
  }

  auto root_it =
      std::find_if(roots.begin(), roots.end(), [&](const RootEntry &entry) {
        return target_uses_root(entry, *target) &&
               (!require_input_enabled || entry.root->input_enabled);
      });

  return root_it != roots.end();
}

std::optional<Target> target_from_node(const RootEntry &entry,
                                       ui::UINodeId node_id) {
  if (node_id == ui::k_invalid_node_id) {
    return std::nullopt;
  }

  return Target{
      .entity_id = entry.entity_id,
      .document = entry.document,
      .node_id = node_id,
  };
}

std::optional<PointerHit> target_from_hit(const RootEntry &entry,
                                          const ui::UiHitResult &hit) {
  auto target = target_from_node(entry, hit.node_id);
  if (!target.has_value()) {
    return std::nullopt;
  }

  return PointerHit{
      .target = *target,
      .part = hit.part,
      .item_index = hit.item_index,
  };
}

std::optional<Target> map_to_ancestor_target(
    const RootEntry &entry, ui::UINodeId node_id,
    const std::function<std::optional<ui::UINodeId>(const ui::UIDocument &,
                                                    ui::UINodeId)> &mapper) {
  if (entry.document == nullptr || node_id == ui::k_invalid_node_id) {
    return std::nullopt;
  }

  auto mapped_node_id = mapper(*entry.document, node_id);
  return mapped_node_id.has_value() ? target_from_node(entry, *mapped_node_id)
                                    : std::nullopt;
}

std::optional<Target> find_hoverable_target(const RootEntry &entry,
                                            ui::UINodeId node_id) {
  return map_to_ancestor_target(
      entry, node_id, [](const ui::UIDocument &document, ui::UINodeId current) {
        while (current != ui::k_invalid_node_id) {
          const auto *node = document.node(current);
          if (node == nullptr) {
            return std::optional<ui::UINodeId>{};
          }

          if ((node->type == ui::NodeType::Pressable ||
               node->type == ui::NodeType::SegmentedControl ||
               node->type == ui::NodeType::ChipGroup ||
               node->type == ui::NodeType::Checkbox ||
               node->type == ui::NodeType::Slider ||
               node->type == ui::NodeType::Select ||
               node->type == ui::NodeType::TextInput ||
               node->type == ui::NodeType::Splitter) &&
              ui::node_chain_allows_interaction(document, current)) {
            return std::optional<ui::UINodeId>{current};
          }

          current = node->parent;
        }

        return std::optional<ui::UINodeId>{};
      });
}

std::optional<Target> find_drag_handle_target(const RootEntry &entry,
                                              ui::UINodeId node_id) {
  return map_to_ancestor_target(
      entry, node_id, [](const ui::UIDocument &document, ui::UINodeId current) {
        while (current != ui::k_invalid_node_id) {
          const auto *node = document.node(current);
          if (node == nullptr) {
            return std::optional<ui::UINodeId>{};
          }

          if (ui::node_is_drag_handle(*node) &&
              ui::node_chain_allows_interaction(document, current)) {
            return std::optional<ui::UINodeId>{current};
          }

          current = node->parent;
        }

        return std::optional<ui::UINodeId>{};
      });
}

std::optional<Target> find_draggable_panel_target(const RootEntry &entry,
                                                  ui::UINodeId node_id) {
  return map_to_ancestor_target(
      entry, node_id, [](const ui::UIDocument &document, ui::UINodeId current) {
        while (current != ui::k_invalid_node_id) {
          const auto *node = document.node(current);
          if (node == nullptr) {
            return std::optional<ui::UINodeId>{};
          }

          if (ui::node_supports_panel_drag(*node) &&
              ui::node_chain_allows_interaction(document, current)) {
            return std::optional<ui::UINodeId>{current};
          }

          current = node->parent;
        }

        return std::optional<ui::UINodeId>{};
      });
}

std::optional<ui::UILayoutContext>
context_for_target(const std::vector<RootEntry> &roots, const Target &target,
                   const glm::vec2 &viewport_size) {
  const RootEntry *entry = find_root_entry(roots, target);
  if (entry == nullptr) {
    return std::nullopt;
  }

  return make_context(*entry, viewport_size);
}

std::vector<Target> collect_focus_order(const std::vector<RootEntry> &roots) {
  std::vector<Target> order;

  for (const RootEntry &entry : roots) {
    if (!entry.root->input_enabled || entry.document == nullptr) {
      continue;
    }

    for (ui::UINodeId node_id : ui::collect_focusable_order(*entry.document)) {
      order.push_back(Target{
          .entity_id = entry.entity_id,
          .document = entry.document,
          .node_id = node_id,
      });
    }
  }

  return order;
}

bool should_reset_caret_for_key(const ui::UIKeyInputEvent &event) {
  using input::KeyCode;

  switch (event.key_code) {
  case KeyCode::Left:
  case KeyCode::Right:
  case KeyCode::Home:
  case KeyCode::End:
  case KeyCode::Backspace:
  case KeyCode::Delete:
    return true;
  case KeyCode::A:
  case KeyCode::C:
  case KeyCode::V:
  case KeyCode::X:
    return event.modifiers.primary_shortcut();
  default:
    return false;
  }
}

bool shift_pressed() {
  return input::IS_KEY_DOWN(input::KeyCode::LeftShift) ||
         input::IS_KEY_DOWN(input::KeyCode::RightShift);
}

float resolve_length(const ui::UILength &value, float basis, float rem_basis,
                     float auto_value) {
  switch (value.unit) {
  case ui::UiLengthUnit::Pixels:
    return value.value;
  case ui::UiLengthUnit::Percent:
    return basis * value.value;
  case ui::UiLengthUnit::Rem:
    return rem_basis * value.value;
  case ui::UiLengthUnit::Auto:
  default:
    return auto_value;
  }
}

float resolve_min_length(const ui::UILength &value, float basis,
                         float rem_basis) {
  return value.unit == ui::UiLengthUnit::Auto ? 0.0f
                                              : resolve_length(
                                                    value, basis, rem_basis);
}

float resolve_max_length(const ui::UILength &value, float basis,
                         float rem_basis) {
  return value.unit == ui::UiLengthUnit::Auto
             ? std::numeric_limits<float>::infinity()
             : resolve_length(value, basis, rem_basis);
}

ui::UiRect parent_content_bounds(const ui::UIDocument &document,
                                 ui::UINodeId node_id) {
  const ui::UINodeId parent_id = document.parent(node_id);
  if (parent_id == ui::k_invalid_node_id) {
    const glm::vec2 canvas = document.canvas_size();
    return ui::UiRect{
        .x = 0.0f, .y = 0.0f, .width = canvas.x, .height = canvas.y};
  }

  const auto *parent = document.node(parent_id);
  if (parent == nullptr) {
    const glm::vec2 canvas = document.canvas_size();
    return ui::UiRect{
        .x = 0.0f, .y = 0.0f, .width = canvas.x, .height = canvas.y};
  }

  return parent->layout.content_bounds;
}

void write_absolute_bounds_to_style(const Target &target,
                                    const ui::UiRect &parent_bounds,
                                    const ui::UiRect &bounds) {
  if (target.document == nullptr) {
    return;
  }

  target.document->mutate_style(target.node_id, [&](ui::UIStyle &style) {
    style.left = ui::UILength::pixels(bounds.x - parent_bounds.x);
    style.top = ui::UILength::pixels(bounds.y - parent_bounds.y);
    style.width = ui::UILength::pixels(bounds.width);
    style.height = ui::UILength::pixels(bounds.height);
    style.right = ui::UILength::auto_value();
    style.bottom = ui::UILength::auto_value();
  });
}

void canonicalize_absolute_bounds(const Target &target) {
  if (target.document == nullptr) {
    return;
  }

  const auto *node = target.document->node(target.node_id);
  if (node == nullptr ||
      (!ui::node_supports_panel_resize(*node) &&
       !ui::node_supports_panel_drag(*node))) {
    return;
  }

  const ui::UiRect parent_bounds =
      parent_content_bounds(*target.document, target.node_id);
  write_absolute_bounds_to_style(target, parent_bounds, node->layout.bounds);
}

std::pair<float, float>
resolved_main_axis_limits(const ui::UIDocument::UINode &node, float basis,
                          ui::FlexDirection direction, float rem_basis) {
  if (direction == ui::FlexDirection::Row) {
    return {
        resolve_min_length(node.style.min_width, basis, rem_basis),
        resolve_max_length(node.style.max_width, basis, rem_basis),
    };
  }

  return {
      resolve_min_length(node.style.min_height, basis, rem_basis),
      resolve_max_length(node.style.max_height, basis, rem_basis),
  };
}

} // namespace astralix::ui_system_core
