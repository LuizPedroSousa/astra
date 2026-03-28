#include "systems/ui-system/scroll.hpp"

#include "foundations.hpp"
#include "systems/ui-system/core.hpp"
#include <algorithm>

namespace astralix::ui_system_core {
namespace {

glm::vec2 wheel_delta_pixels_for_node(const ui::UIDocument::UINode &node, const ui::UIMouseWheelInputEvent &event) {
  glm::vec2 delta(0.0f);
  const float horizontal_pixels = -event.offset.x * 36.0f;
  const float vertical_pixels = -event.offset.y * 36.0f;

  if (ui::scrolls_horizontally(node.style.scroll_mode)) {
    delta.x += horizontal_pixels;
  }

  if (event.modifiers.shift && event.offset.y != 0.0f &&
      ui::scrolls_horizontally(node.style.scroll_mode)) {
    delta.x += vertical_pixels;
  } else if (ui::scrolls_vertically(node.style.scroll_mode)) {
    delta.y += vertical_pixels;
  } else if (ui::scrolls_horizontally(node.style.scroll_mode)) {
    delta.x += vertical_pixels;
  }

  return delta;
}

void clear_scrollbar_visual_state(const RootEntry &entry) {
  if (entry.document == nullptr) {
    return;
  }

  for (ui::UINodeId node_id : entry.document->root_to_leaf_order()) {
    auto *node = entry.document->node(node_id);
    if (node == nullptr || node->type != ui::NodeType::ScrollView) {
      continue;
    }

    auto &scroll = node->layout.scroll;
    scroll.vertical_thumb_hovered = false;
    scroll.vertical_thumb_active = false;
    scroll.horizontal_thumb_hovered = false;
    scroll.horizontal_thumb_active = false;
  }
}

} // namespace

std::optional<ScrollDispatch>
find_scroll_dispatch(const std::vector<RootEntry> &roots, const Target &deepest_target, const ui::UIMouseWheelInputEvent &event) {
  if (deepest_target.document == nullptr) {
    return std::nullopt;
  }

  const RootEntry *root_entry = find_root_entry(roots, deepest_target);
  if (root_entry == nullptr || !root_entry->root->input_enabled) {
    return std::nullopt;
  }

  ui::UINodeId current = deepest_target.node_id;
  while (current != ui::k_invalid_node_id) {
    auto *node = deepest_target.document->node(current);
    auto *scroll = deepest_target.document->scroll_state(current);
    if (node != nullptr && scroll != nullptr &&
        node->style.scroll_mode != ui::ScrollMode::None) {
      const glm::vec2 delta = wheel_delta_pixels_for_node(*node, event);
      if (delta != glm::vec2(0.0f)) {
        const glm::vec2 next_offset =
            ui::clamp_scroll_offset(scroll->offset + delta, scroll->max_offset, node->style.scroll_mode);
        if (next_offset != scroll->offset) {
          return ScrollDispatch{
              .target =
                  Target{
                      .entity_id = root_entry->entity_id,
                      .document = deepest_target.document,
                      .node_id = current,
                  },
              .delta = delta,
          };
        }
      }
    }

    current = deepest_target.document->parent(current);
  }

  return std::nullopt;
}

void page_scroll_track(const Target &target, ui::UIHitPart part, glm::vec2 pointer) {
  if (target.document == nullptr) {
    return;
  }

  const auto *node = target.document->node(target.node_id);
  auto *scroll = target.document->scroll_state(target.node_id);
  if (node == nullptr || scroll == nullptr) {
    return;
  }

  glm::vec2 next_offset = scroll->offset;
  switch (part) {
    case ui::UIHitPart::VerticalScrollbarTrack:
      next_offset.y += pointer.y < scroll->vertical_thumb_rect.y
                           ? -scroll->viewport_size.y
                           : scroll->viewport_size.y;
      break;
    case ui::UIHitPart::HorizontalScrollbarTrack:
      next_offset.x += pointer.x < scroll->horizontal_thumb_rect.x
                           ? -scroll->viewport_size.x
                           : scroll->viewport_size.x;
      break;
    default:
      return;
  }

  next_offset = ui::clamp_scroll_offset(next_offset, scroll->max_offset, node->style.scroll_mode);
  if (next_offset != scroll->offset) {
    target.document->set_scroll_offset(target.node_id, next_offset);
  }
}

void update_scrollbar_drag(const Target &target, ui::UIHitPart part, float grab_offset, glm::vec2 pointer) {
  if (target.document == nullptr) {
    return;
  }

  const auto *node = target.document->node(target.node_id);
  auto *scroll = target.document->scroll_state(target.node_id);
  if (node == nullptr || scroll == nullptr) {
    return;
  }

  glm::vec2 next_offset = scroll->offset;
  switch (part) {
    case ui::UIHitPart::VerticalScrollbarThumb: {
      const float travel = std::max(0.0f, scroll->vertical_track_rect.height - scroll->vertical_thumb_rect.height);
      const float thumb_position = std::clamp(
          pointer.y - scroll->vertical_track_rect.y - grab_offset, 0.0f, travel
      );
      const float ratio = travel > 0.0f ? thumb_position / travel : 0.0f;
      next_offset.y = scroll->max_offset.y * ratio;
      break;
    }
    case ui::UIHitPart::HorizontalScrollbarThumb: {
      const float travel =
          std::max(0.0f, scroll->horizontal_track_rect.width - scroll->horizontal_thumb_rect.width);
      const float thumb_position =
          std::clamp(pointer.x - scroll->horizontal_track_rect.x - grab_offset, 0.0f, travel);
      const float ratio = travel > 0.0f ? thumb_position / travel : 0.0f;
      next_offset.x = scroll->max_offset.x * ratio;
      break;
    }
    default:
      return;
  }

  next_offset = ui::clamp_scroll_offset(next_offset, scroll->max_offset, node->style.scroll_mode);
  if (next_offset != scroll->offset) {
    target.document->set_scroll_offset(target.node_id, next_offset);
  }
}

void apply_scrollbar_visual_state(
    const std::vector<RootEntry> &roots,
    const std::optional<PointerHit> &hover_hit,
    const std::optional<std::pair<Target, ui::UIHitPart>> &active_scrollbar
) {
  for (const RootEntry &entry : roots) {
    clear_scrollbar_visual_state(entry);
  }

  if (hover_hit.has_value() && hover_hit->target.document != nullptr) {
    if (auto *scroll =
            hover_hit->target.document->scroll_state(hover_hit->target.node_id);
        scroll != nullptr) {
      switch (hover_hit->part) {
        case ui::UIHitPart::VerticalScrollbarThumb:
          scroll->vertical_thumb_hovered = true;
          break;
        case ui::UIHitPart::HorizontalScrollbarThumb:
          scroll->horizontal_thumb_hovered = true;
          break;
        default:
          break;
      }
    }
  }

  if (active_scrollbar.has_value() &&
      active_scrollbar->first.document != nullptr) {
    if (auto *scroll = active_scrollbar->first.document->scroll_state(
            active_scrollbar->first.node_id
        );
        scroll != nullptr) {
      switch (active_scrollbar->second) {
        case ui::UIHitPart::VerticalScrollbarThumb:
          scroll->vertical_thumb_hovered = true;
          scroll->vertical_thumb_active = true;
          break;
        case ui::UIHitPart::HorizontalScrollbarThumb:
          scroll->horizontal_thumb_hovered = true;
          scroll->horizontal_thumb_active = true;
          break;
        default:
          break;
      }
    }
  }
}

} // namespace astralix::ui_system_core
