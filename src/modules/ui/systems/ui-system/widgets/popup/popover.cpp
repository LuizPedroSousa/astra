#include "systems/ui-system/widgets/popup/popover.hpp"

#include "foundations.hpp"

namespace astralix::ui_system_core {
namespace {

bool document_has_open_popover(
    const ui::UIDocument &document,
    const std::function<bool(const ui::UIPopoverState &)> &predicate
) {
  for (ui::UINodeId node_id : document.open_popover_stack()) {
    const auto *node = document.node(node_id);
    if (node == nullptr || node->type != ui::NodeType::Popover ||
        !node->visible || !node->enabled || !node->popover.open) {
      continue;
    }

    if (!predicate || predicate(node->popover)) {
      return true;
    }
  }

  return false;
}

bool document_point_hits_open_popover(
    const ui::UIDocument &document,
    glm::vec2 point
) {
  for (auto it = document.open_popover_stack().rbegin();
       it != document.open_popover_stack().rend();
       ++it) {
    const auto *node = document.node(*it);
    if (node == nullptr || node->type != ui::NodeType::Popover ||
        !node->visible || !node->enabled || !node->popover.open) {
      continue;
    }

    if (node->layout.popover.popup_rect.contains(point)) {
      return true;
    }
  }

  return false;
}

} // namespace

bool point_hits_open_popover(
    const std::vector<RootEntry> &roots,
    glm::vec2 point
) {
  for (auto it = roots.rbegin(); it != roots.rend(); ++it) {
    if (!it->root->input_enabled || it->document == nullptr) {
      continue;
    }

    if (document_point_hits_open_popover(*it->document, point)) {
      return true;
    }
  }

  return false;
}

bool close_popovers_on_outside_press(
    const std::vector<RootEntry> &roots,
    glm::vec2 point
) {
  if (point_hits_open_popover(roots, point)) {
    return false;
  }

  bool closed_any = false;
  for (const RootEntry &entry : roots) {
    if (entry.document == nullptr ||
        !document_has_open_popover(
            *entry.document,
            [](const ui::UIPopoverState &popover) {
              return popover.close_on_outside_click;
            }
        )) {
      continue;
    }

    entry.document->close_all_popovers();
    closed_any = true;
  }

  return closed_any;
}

bool close_popovers_on_escape(
    const std::vector<RootEntry> &roots,
    const ui::UIKeyInputEvent &event
) {
  if (event.key_code != input::KeyCode::Escape) {
    return false;
  }

  bool closed_any = false;
  for (const RootEntry &entry : roots) {
    if (entry.document == nullptr ||
        !document_has_open_popover(
            *entry.document,
            [](const ui::UIPopoverState &popover) {
              return popover.close_on_escape;
            }
        )) {
      continue;
    }

    entry.document->close_all_popovers();
    closed_any = true;
  }

  return closed_any;
}

std::optional<Target> find_secondary_click_target(
    const RootEntry &entry,
    ui::UINodeId node_id,
    ui::UIHitPart part
) {
  if (ui::is_scrollbar_part(part) || ui::is_resize_part(part)) {
    return std::nullopt;
  }

  return map_to_ancestor_target(
      entry,
      node_id,
      [](const ui::UIDocument &document, ui::UINodeId current) {
        while (current != ui::k_invalid_node_id) {
          const auto *node = document.node(current);
          if (node == nullptr) {
            return std::optional<ui::UINodeId>{};
          }

          if (node->on_secondary_click &&
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
