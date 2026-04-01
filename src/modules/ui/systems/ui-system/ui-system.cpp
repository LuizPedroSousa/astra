#include "systems/ui-system/ui-system.hpp"

#include "event-dispatcher.hpp"
#include "events/mouse-listener.hpp"
#include "foundations.hpp"
#include "layout/layout.hpp"
#include "managers/scene-manager.hpp"
#include "managers/window-manager.hpp"
#include "systems/ui-system/controls.hpp"
#include "systems/ui-system/resize.hpp"
#include "systems/ui-system/widgets/layout/scroll-view.hpp"
#include "systems/ui-system/widgets/inputs/text-input.hpp"
#include "systems/ui-system/widgets/popup/popover.hpp"
#include <algorithm>

namespace astralix {
namespace detail = ui_system_core;

namespace {

std::optional<CursorIcon> cursor_icon_for_target(
    const detail::Target &target
) {
  if (target.document == nullptr) {
    return std::nullopt;
  }

  const auto *node = target.document->node(target.node_id);
  if (node == nullptr || !node->style.cursor.has_value()) {
    return std::nullopt;
  }

  switch (*node->style.cursor) {
    case ui::CursorStyle::Pointer:
      return CursorIcon::Pointer;
    case ui::CursorStyle::Default:
    default:
      return std::nullopt;
  }
}

input::KeyModifiers current_key_modifiers() {
  return input::KeyModifiers{
      .shift = input::IS_KEY_DOWN(input::KeyCode::LeftShift) ||
               input::IS_KEY_DOWN(input::KeyCode::RightShift),
      .control = input::IS_KEY_DOWN(input::KeyCode::LeftControl) ||
                 input::IS_KEY_DOWN(input::KeyCode::RightControl),
      .alt = input::IS_KEY_DOWN(input::KeyCode::LeftAlt) ||
             input::IS_KEY_DOWN(input::KeyCode::RightAlt),
      .super = input::IS_KEY_DOWN(input::KeyCode::LeftSuper) ||
               input::IS_KEY_DOWN(input::KeyCode::RightSuper),
  };
}

} // namespace

void UISystem::start() {
  auto *dispatcher = EventDispatcher::get();

  dispatcher->attach<input::KeyboardListener, input::KeyPressedEvent>(
      [this](input::KeyPressedEvent *event) {
        m_key_inputs.push_back(QueuedKeyInput{
            .event = ui::UIKeyInputEvent{
                .key_code = event->key_code,
                .modifiers = event->modifiers,
                .repeat = event->repeat,
            },
        });
      }
  );

  dispatcher->attach<input::KeyboardListener, input::CharacterInputEvent>(
      [this](input::CharacterInputEvent *event) {
        m_character_inputs.push_back(QueuedCharacterInput{
            .event = ui::UICharacterInputEvent{
                .codepoint = event->codepoint,
                .modifiers = event->modifiers,
            },
        });
      }
  );

  dispatcher->attach<MouseListener, MouseWheelEvent>(
      [this](MouseWheelEvent *event) {
        m_mouse_wheel_inputs.push_back(QueuedMouseWheelInput{
            .event = ui::UIMouseWheelInputEvent{
                .offset = glm::vec2(static_cast<float>(event->xoffset), static_cast<float>(event->yoffset)),
                .modifiers = event->modifiers,
            },
        });
      }
  );
}

void UISystem::fixed_update(double) {}

void UISystem::pre_update(double) {}

void UISystem::update(double dt) {
  auto *scene = SceneManager::get()->get_active_scene();
  auto window = window_manager()->active_window();

  if (scene == nullptr || window == nullptr) {
    clear_pointer_state();
    m_last_scene = scene;
    clear_queued_inputs();
    return;
  }

  const bool scene_changed = m_last_scene != scene;
  m_last_scene = scene;

  auto &world = scene->world();
  const glm::vec2 viewport_size(static_cast<float>(window->width()), static_cast<float>(window->height()));
  const bool pointer_enabled = !input::IS_CURSOR_CAPTURED();

  auto roots = gather_root_entries(world, viewport_size);
  process_programmatic_focus(roots, viewport_size);
  validate_targets(roots, scene_changed, pointer_enabled);

  glm::vec2 pointer(0.0f);
  const auto deepest_hit = hit_test(roots, pointer_enabled, pointer);

  update_active_drags(roots, pointer_enabled, pointer, viewport_size);
  resolve_hot_target(roots, deepest_hit);
  handle_pointer_press(roots, deepest_hit, pointer_enabled, pointer, viewport_size);
  handle_pointer_release(roots, pointer_enabled, deepest_hit, pointer);
  dispatch_key_input(roots, viewport_size);
  dispatch_character_input(roots, viewport_size);
  dispatch_scroll_input(roots, deepest_hit);
  tick_caret_blink(dt);
  flush_and_relayout(roots, viewport_size);
  apply_visual_state(roots, deepest_hit);
  resolve_cursor_icon(roots, deepest_hit, pointer_enabled);
  build_draw_lists(roots, viewport_size);
  clear_queued_inputs();
}

std::vector<detail::RootEntry>
UISystem::gather_root_entries(ecs::World &world, const glm::vec2 &viewport_size) const {
  std::vector<detail::RootEntry> roots;
  world.each<rendering::UIRoot>([&](EntityID entity_id,
                                    rendering::UIRoot &root) {
    if (!world.active(entity_id) || !root.visible || root.document == nullptr) {
      return;
    }

    roots.push_back(detail::RootEntry{
        .entity_id = entity_id,
        .root = &root,
        .document = root.document,
    });
  });

  std::sort(roots.begin(), roots.end(), [](const detail::RootEntry &lhs, const detail::RootEntry &rhs) {
    return lhs.root->sort_order < rhs.root->sort_order;
  });

  for (const detail::RootEntry &entry : roots) {
    auto context = detail::make_context(entry, viewport_size);
    if (entry.document->dirty()) {
      ui::layout_document(*entry.document, context);
    }
  }

  return roots;
}

void UISystem::process_programmatic_focus(
    const std::vector<detail::RootEntry> &roots,
    const glm::vec2 &viewport_size
) {
  for (const detail::RootEntry &entry : roots) {
    if (!entry.root->input_enabled || entry.document == nullptr) {
      continue;
    }

    const ui::UINodeId requested_focus_node =
        entry.document->consume_requested_focus();
    if (requested_focus_node == ui::k_invalid_node_id) {
      continue;
    }

    auto requested_target =
        detail::target_from_node(entry, requested_focus_node);
    if (!requested_target.has_value() ||
        !ui::node_chain_allows_interaction(*entry.document, requested_focus_node)) {
      continue;
    }

    set_focused_target(requested_target);
    if (auto context =
            detail::context_for_target(roots, *requested_target, viewport_size);
        context.has_value()) {
      const auto *requested_node =
          requested_target->document->node(requested_target->node_id);
      if (requested_node != nullptr &&
          (requested_node->type == ui::NodeType::TextInput ||
           requested_node->type == ui::NodeType::Combobox)) {
        detail::focus_text_input(*requested_target, *context, requested_node->select_all_on_focus);
      }
    }
  }
}

void UISystem::validate_targets(const std::vector<detail::RootEntry> &roots, bool scene_changed, bool pointer_enabled) {
  if (!detail::target_available(roots, m_focused_target, true)) {
    clear_focused_target(true);
  }
  if (!detail::target_available(roots, m_hot_target, true)) {
    clear_hot_target();
  }
  if (!detail::target_available(roots, m_active_target, true)) {
    clear_active_target(false);
  }
  if (m_text_selection_drag.has_value() &&
      !detail::target_available(roots, m_text_selection_drag->target, true)) {
    clear_text_selection_drag();
  }
  if (m_scrollbar_drag.has_value() &&
      !detail::target_available(roots, m_scrollbar_drag->target, true)) {
    clear_scrollbar_drag();
  }
  if (m_slider_drag.has_value() &&
      !detail::target_available(roots, m_slider_drag->target, true)) {
    clear_slider_drag();
  }
  if (m_secondary_click_press.has_value() &&
      !detail::target_available(
          roots,
          std::optional<Target>{m_secondary_click_press->target},
          true
      )) {
    clear_secondary_click_state();
  }
  if (m_panel_move_drag.has_value() &&
      (!detail::target_available(roots, m_panel_move_drag->panel_target, true) ||
       !detail::target_available(roots, m_panel_move_drag->handle_target, true))) {
    clear_panel_move_drag();
  }
  if (m_panel_resize_drag.has_value() &&
      !detail::target_available(roots, m_panel_resize_drag->target, true)) {
    clear_panel_resize_drag();
  }
  if (m_splitter_resize_drag.has_value() &&
      !detail::target_available(roots, m_splitter_resize_drag->target, true)) {
    clear_splitter_resize_drag();
  }

  if (scene_changed || !pointer_enabled) {
    clear_pointer_state();
    if (auto window = window_manager()->active_window(); window != nullptr) {
      window->set_cursor_icon(CursorIcon::Default);
    }
  }
}

std::optional<detail::PointerHit>
UISystem::hit_test(const std::vector<detail::RootEntry> &roots, bool pointer_enabled, glm::vec2 &pointer) const {
  if (!pointer_enabled) {
    return std::nullopt;
  }

  const auto cursor = input::CURSOR_POSITION();
  pointer =
      glm::vec2(static_cast<float>(cursor.x), static_cast<float>(cursor.y));

  for (auto it = roots.rbegin(); it != roots.rend(); ++it) {
    if (!it->root->input_enabled || it->document == nullptr) {
      continue;
    }

    auto hit = ui::hit_test_document(*it->document, pointer);
    if (!hit.has_value()) {
      continue;
    }

    if (auto deepest_hit = detail::target_from_hit(*it, *hit);
        deepest_hit.has_value()) {
      return deepest_hit;
    }
  }

  return std::nullopt;
}

void UISystem::update_active_drags(const std::vector<detail::RootEntry> &roots, bool pointer_enabled, const glm::vec2 &pointer, const glm::vec2 &viewport_size) {
  if (!pointer_enabled ||
      !input::IS_MOUSE_BUTTON_DOWN(input::MouseButton::Left)) {
    return;
  }

  if (m_panel_move_drag.has_value()) {
    detail::update_panel_move_drag(*m_panel_move_drag, pointer);
  } else if (m_panel_resize_drag.has_value()) {
    detail::update_panel_resize_drag(*m_panel_resize_drag, pointer);
  } else if (m_splitter_resize_drag.has_value()) {
    detail::update_splitter_resize_drag(*m_splitter_resize_drag, pointer);
  } else if (m_scrollbar_drag.has_value()) {
    detail::update_scrollbar_drag(m_scrollbar_drag->target, m_scrollbar_drag->part, m_scrollbar_drag->grab_offset, pointer);
  } else if (m_slider_drag.has_value()) {
    detail::update_slider_drag(m_slider_drag->target, pointer);
  } else if (m_text_selection_drag.has_value()) {
    if (auto context = detail::context_for_target(
            roots, m_text_selection_drag->target, viewport_size
        );
        context.has_value()) {
      const auto *node = m_text_selection_drag->target.document->node(
          m_text_selection_drag->target.node_id
      );
      if (node != nullptr &&
          (node->type == ui::NodeType::TextInput ||
           node->type == ui::NodeType::Combobox)) {
        const size_t focus_index =
            detail::text_input_index_from_pointer(*node, *context, pointer);
        detail::set_text_input_selection_and_caret(
            m_text_selection_drag->target, m_text_selection_drag->anchor_index, focus_index, *context
        );
      }
    }
  }
}

void UISystem::resolve_hot_target(
    const std::vector<detail::RootEntry> &roots,
    const std::optional<detail::PointerHit> &deepest_hit
) {
  std::optional<Target> next_hot_target;
  if (m_panel_move_drag.has_value()) {
    next_hot_target = m_panel_move_drag->handle_target;
  } else if (m_splitter_resize_drag.has_value()) {
    next_hot_target = m_splitter_resize_drag->target;
  } else if (m_slider_drag.has_value()) {
    next_hot_target = m_slider_drag->target;
  } else if (m_text_selection_drag.has_value()) {
    next_hot_target = m_text_selection_drag->target;
  } else if (!m_scrollbar_drag.has_value() && !m_panel_resize_drag.has_value() && !m_splitter_resize_drag.has_value() && deepest_hit.has_value()) {
    const detail::RootEntry *root_entry =
        detail::find_root_entry(roots, deepest_hit->target);
    if (root_entry != nullptr && !ui::is_scrollbar_part(deepest_hit->part) &&
        !ui::is_panel_resize_part(deepest_hit->part)) {
      next_hot_target = detail::find_hoverable_target(
          *root_entry, deepest_hit->target.node_id
      );
      if (!next_hot_target.has_value() &&
          deepest_hit->part == ui::UIHitPart::Body) {
        auto drag_handle_target = detail::find_drag_handle_target(
            *root_entry, deepest_hit->target.node_id
        );
        if (drag_handle_target.has_value() &&
            detail::find_draggable_panel_target(
                *root_entry, drag_handle_target->node_id
            )
                .has_value()) {
          next_hot_target = drag_handle_target;
        }
      }
    }
  }

  if (detail::same_target(m_hot_target, next_hot_target)) {
    return;
  }

  clear_hot_target();
  if (next_hot_target.has_value() && next_hot_target->document != nullptr) {
    next_hot_target->document->set_hot_node(next_hot_target->node_id);
    if (const auto *node =
            next_hot_target->document->node(next_hot_target->node_id);
        node != nullptr && node->on_hover) {
      next_hot_target->document->queue_callback(node->on_hover);
    }
  }

  m_hot_target = next_hot_target;
}

void UISystem::handle_pointer_press(
    const std::vector<detail::RootEntry> &roots,
    const std::optional<detail::PointerHit> &deepest_hit, bool pointer_enabled,
    const glm::vec2 &pointer, const glm::vec2 &viewport_size
) {
  if (!pointer_enabled) {
    return;
  }

  if ((input::IS_MOUSE_BUTTON_PRESSED(input::MouseButton::Left) ||
       input::IS_MOUSE_BUTTON_PRESSED(input::MouseButton::Right)) &&
      detail::close_popovers_on_outside_press(roots, pointer)) {
    clear_secondary_click_state();
    return;
  }

  if (input::IS_MOUSE_BUTTON_PRESSED(input::MouseButton::Right)) {
    clear_secondary_click_state();
    if (!deepest_hit.has_value()) {
      return;
    }

    const detail::RootEntry *root_entry =
        detail::find_root_entry(roots, deepest_hit->target);
    if (root_entry == nullptr) {
      return;
    }

    auto secondary_target = detail::find_secondary_click_target(
        *root_entry,
        deepest_hit->target.node_id,
        deepest_hit->part
    );
    if (!secondary_target.has_value()) {
      return;
    }

    m_secondary_click_press = SecondaryClickPress{
        .target = *secondary_target,
        .pointer = pointer,
        .modifiers = current_key_modifiers(),
    };
    return;
  }

  if (!input::IS_MOUSE_BUTTON_PRESSED(input::MouseButton::Left)) {
    return;
  }

  clear_active_target(false);
  clear_drag_state();

  if (!deepest_hit.has_value()) {
    clear_focused_target(true);
    return;
  }

  const detail::RootEntry *root_entry =
      detail::find_root_entry(roots, deepest_hit->target);
  if (root_entry == nullptr) {
    return;
  }

  const auto interactive_target =
      detail::find_hoverable_target(*root_entry, deepest_hit->target.node_id);
  if (!interactive_target.has_value() &&
      deepest_hit->part == ui::UIHitPart::Body) {
    auto drag_handle_target =
        detail::find_drag_handle_target(*root_entry, deepest_hit->target.node_id);
    if (drag_handle_target.has_value()) {
      auto panel_target = detail::find_draggable_panel_target(
          *root_entry, drag_handle_target->node_id
      );
      if (panel_target.has_value()) {
        detail::canonicalize_absolute_bounds(*panel_target);
        if (const auto *drag_panel =
                panel_target->document->node(panel_target->node_id);
            drag_panel != nullptr) {
          m_active_target = *drag_handle_target;
          m_active_target->document->set_active_node(m_active_target->node_id);
          m_panel_move_drag = PanelMoveDrag{
              .panel_target = *panel_target,
              .handle_target = *drag_handle_target,
              .start_pointer = pointer,
              .start_bounds = drag_panel->layout.bounds,
          };
          return;
        }
      }
    }
  }

  bool was_focused_before_press = false;
  bool selected_all_on_focus = false;
  auto focus_target = detail::map_to_ancestor_target(
      *root_entry, deepest_hit->target.node_id, [](const ui::UIDocument &document, ui::UINodeId node_id) {
        return ui::find_nearest_focusable_ancestor(document, node_id);
      }
  );

  if (focus_target.has_value()) {
    was_focused_before_press =
        detail::same_target(m_focused_target, focus_target);
    set_focused_target(focus_target);

    if (focus_target->document != nullptr) {
      const auto *focus_node =
          focus_target->document->node(focus_target->node_id);
      if (focus_node != nullptr &&
          (focus_node->type == ui::NodeType::TextInput ||
           focus_node->type == ui::NodeType::Combobox)) {
        if (auto context =
                detail::context_for_target(roots, *focus_target, viewport_size);
            context.has_value()) {
          if (!was_focused_before_press && focus_node->select_all_on_focus) {
            detail::focus_text_input(*focus_target, *context, true);
            selected_all_on_focus = true;
          }
        }
      }
    }
  } else {
    clear_focused_target(true);
  }

  if (ui::is_panel_resize_part(deepest_hit->part)) {
    detail::canonicalize_absolute_bounds(deepest_hit->target);
    if (const auto *resize_node =
            deepest_hit->target.document->node(deepest_hit->target.node_id);
        resize_node != nullptr) {
      m_panel_resize_drag = PanelResizeDrag{
          .target = deepest_hit->target,
          .part = deepest_hit->part,
          .start_pointer = pointer,
          .start_bounds = resize_node->layout.bounds,
      };
    }
    return;
  }

  if (deepest_hit->part == ui::UIHitPart::SplitterBar) {
    m_active_target = deepest_hit->target;
    m_active_target->document->set_active_node(m_active_target->node_id);
    detail::queue_press_callback(*m_active_target);

    if (auto drag =
            detail::begin_splitter_resize_drag(deepest_hit->target, pointer);
        drag.has_value()) {
      m_splitter_resize_drag = *drag;
    }
    return;
  }

  if (deepest_hit->part == ui::UIHitPart::VerticalScrollbarThumb ||
      deepest_hit->part == ui::UIHitPart::HorizontalScrollbarThumb) {
    auto *scroll =
        deepest_hit->target.document->scroll_state(deepest_hit->target.node_id);
    if (scroll != nullptr) {
      const float grab_offset =
          deepest_hit->part == ui::UIHitPart::VerticalScrollbarThumb
              ? pointer.y - scroll->vertical_thumb_rect.y
              : pointer.x - scroll->horizontal_thumb_rect.x;
      m_scrollbar_drag = ScrollbarDrag{
          .target = deepest_hit->target,
          .part = deepest_hit->part,
          .grab_offset = grab_offset,
      };
    }
    return;
  }

  if (deepest_hit->part == ui::UIHitPart::VerticalScrollbarTrack ||
      deepest_hit->part == ui::UIHitPart::HorizontalScrollbarTrack) {
    detail::page_scroll_track(deepest_hit->target, deepest_hit->part, pointer);
    return;
  }

  if (deepest_hit->part == ui::UIHitPart::SelectOption &&
      deepest_hit->item_index.has_value()) {
    detail::confirm_select_option(deepest_hit->target, *deepest_hit->item_index, true);
    return;
  }

  if (deepest_hit->part == ui::UIHitPart::ComboboxOption &&
      deepest_hit->item_index.has_value()) {
    if (auto context = detail::context_for_target(
            roots, deepest_hit->target, viewport_size);
        context.has_value()) {
      detail::confirm_combobox_option(
          deepest_hit->target, *deepest_hit->item_index, *context, true);
    }
    return;
  }

  if (!interactive_target.has_value() ||
      interactive_target->document == nullptr) {
    return;
  }

  m_active_target = interactive_target;
  m_active_item_part = deepest_hit->part;
  m_active_item_index = deepest_hit->item_index;
  m_active_target->document->set_active_node(m_active_target->node_id);
  detail::queue_press_callback(*m_active_target);

  if (const auto *node =
          m_active_target->document->node(m_active_target->node_id);
      node != nullptr) {
    if (node->type == ui::NodeType::Slider) {
      m_slider_drag = SliderDrag{.target = *m_active_target};
      detail::update_slider_drag(*m_active_target, pointer);
    } else if (node->type == ui::NodeType::TextInput ||
               node->type == ui::NodeType::Combobox) {
      if (auto context = detail::context_for_target(roots, *m_active_target, viewport_size);
          context.has_value()) {
        const bool extend_selection =
            detail::shift_pressed() && was_focused_before_press;
        if (selected_all_on_focus) {
          detail::sync_text_input_scroll(*m_active_target, *context);
        } else {
          const size_t focus_index =
              detail::text_input_index_from_pointer(*node, *context, pointer);
          const size_t anchor_index =
              extend_selection
                  ? (node->selection.empty() ? node->caret.index
                                             : node->selection.anchor)
                  : focus_index;
          detail::set_text_input_selection_and_caret(
              *m_active_target, anchor_index, focus_index, *context
          );
          m_text_selection_drag = TextSelectionDrag{
              .target = *m_active_target,
              .anchor_index = anchor_index,
          };
        }
      }
    }
  }
}

void UISystem::handle_pointer_release(
    const std::vector<detail::RootEntry> &roots,
    bool pointer_enabled,
    const std::optional<detail::PointerHit> &deepest_hit,
    const glm::vec2 &pointer
) {
  if (!pointer_enabled) {
    return;
  }

  if (input::IS_MOUSE_BUTTON_RELEASED(input::MouseButton::Right)) {
    auto pressed = m_secondary_click_press;
    clear_secondary_click_state();

    if (!pressed.has_value() || !deepest_hit.has_value() ||
        pressed->target.document == nullptr) {
      return;
    }

    const detail::RootEntry *root_entry =
        detail::find_root_entry(roots, deepest_hit->target);
    if (root_entry == nullptr) {
      return;
    }

    auto released_target = detail::find_secondary_click_target(
        *root_entry,
        deepest_hit->target.node_id,
        deepest_hit->part
    );
    if (!detail::same_target(
            std::optional<Target>{pressed->target},
            released_target
        )) {
      return;
    }

    const auto *node = pressed->target.document->node(pressed->target.node_id);
    if (node == nullptr || !node->on_secondary_click) {
      return;
    }

    auto callback = node->on_secondary_click;
    const ui::UIPointerButtonEvent event{
        .position = pointer,
        .button = input::MouseButton::Right,
        .modifiers = pressed->modifiers,
    };
    pressed->target.document->queue_callback(
        [callback, event]() { callback(event); }
    );
    return;
  }

  if (!input::IS_MOUSE_BUTTON_RELEASED(input::MouseButton::Left)) {
    return;
  }

  const bool had_panel_move_drag = m_panel_move_drag.has_value();
  clear_drag_state();
  if (had_panel_move_drag) {
    clear_active_target(false);
    return;
  }

  if (!m_active_target.has_value() || m_active_target->document == nullptr) {
    return;
  }

  auto released_target = m_active_target;
  const ui::UIHitPart released_item_part = m_active_item_part;
  const std::optional<size_t> released_item_index = m_active_item_index;
  clear_active_target(true);
  if (!released_target.has_value() || released_target->document == nullptr) {
    return;
  }

  const auto *released_node =
      released_target->document->node(released_target->node_id);
  if (released_node == nullptr || !m_hot_target.has_value() ||
      !detail::same_target(released_target, m_hot_target)) {
    return;
  }

  if (released_node->type == ui::NodeType::Pressable) {
    detail::queue_click_callback(*released_target);
  } else if (released_node->type == ui::NodeType::SegmentedControl) {
    if (deepest_hit->part == ui::UIHitPart::SegmentedControlItem &&
        released_item_part == ui::UIHitPart::SegmentedControlItem &&
        deepest_hit->item_index.has_value() &&
        deepest_hit->item_index == released_item_index) {
      detail::select_segmented_option(*released_target, *deepest_hit->item_index, true);
    }
  } else if (released_node->type == ui::NodeType::ChipGroup) {
    if (deepest_hit->part == ui::UIHitPart::ChipItem &&
        released_item_part == ui::UIHitPart::ChipItem &&
        deepest_hit->item_index.has_value() &&
        deepest_hit->item_index == released_item_index) {
      detail::toggle_chip(*released_target, *deepest_hit->item_index, true);
    }
  } else if (released_node->type == ui::NodeType::Checkbox) {
    detail::toggle_checkbox_value(*released_target, true);
  } else if (released_node->type == ui::NodeType::Select) {
    const bool next_open =
        !released_target->document->select_open(released_target->node_id);
    released_target->document->set_select_open(released_target->node_id, next_open);
  } else if (released_node->type == ui::NodeType::Combobox) {
    if (!released_target->document->combobox_open(released_target->node_id) &&
        !released_node->combobox.options.empty()) {
      released_target->document->set_combobox_open(released_target->node_id, true);
    }
  }
}

void UISystem::dispatch_key_input(const std::vector<detail::RootEntry> &roots, const glm::vec2 &viewport_size) {
  const auto focus_order = detail::collect_focus_order(roots);
  for (const QueuedKeyInput &queued_key : m_key_inputs) {
    const auto &event = queued_key.event;
    if (detail::close_popovers_on_escape(roots, event)) {
      continue;
    }

    if (event.key_code == input::KeyCode::Tab && !event.modifiers.shift &&
        m_focused_target.has_value() && m_focused_target->document != nullptr) {
      const auto *focused_node =
          m_focused_target->document->node(m_focused_target->node_id);
      if (focused_node != nullptr &&
          focused_node->type == ui::NodeType::Combobox &&
          m_focused_target->document->combobox_open(m_focused_target->node_id) &&
          !focused_node->combobox.options.empty()) {
        if (auto context = detail::context_for_target(
                roots, *m_focused_target, viewport_size);
            context.has_value()) {
          detail::confirm_combobox_option(
              *m_focused_target, focused_node->combobox.highlighted_index,
              *context, true);
          continue;
        }
      }
    }

    if (event.key_code == input::KeyCode::Tab && !focus_order.empty()) {
      const bool backwards = event.modifiers.shift;
      auto current_it = std::find_if(
          focus_order.begin(), focus_order.end(), [&](const Target &target) {
            return m_focused_target.has_value() &&
                   m_focused_target->entity_id == target.entity_id &&
                   m_focused_target->document == target.document &&
                   m_focused_target->node_id == target.node_id;
          }
      );

      size_t next_index = backwards ? focus_order.size() - 1u : 0u;
      if (current_it != focus_order.end()) {
        const size_t current_index =
            static_cast<size_t>(std::distance(focus_order.begin(), current_it));
        if (backwards) {
          next_index = current_index == 0u ? focus_order.size() - 1u
                                           : current_index - 1u;
        } else {
          next_index = (current_index + 1u) % focus_order.size();
        }
      }

      set_focused_target(focus_order[next_index]);
      if (auto context = detail::context_for_target(
              roots, focus_order[next_index], viewport_size
          );
          context.has_value()) {
        const auto *node = focus_order[next_index].document->node(
            focus_order[next_index].node_id
        );
        if (node != nullptr &&
            (node->type == ui::NodeType::TextInput ||
             node->type == ui::NodeType::Combobox)) {
          detail::focus_text_input(focus_order[next_index], *context, node->select_all_on_focus);
        }
      }
      continue;
    }

    if (!m_focused_target.has_value() ||
        m_focused_target->document == nullptr) {
      continue;
    }

    auto document = m_focused_target->document;
    const auto *node = document->node(m_focused_target->node_id);
    if (node == nullptr) {
      continue;
    }

    bool handled_builtin_key = false;
    if (node->type == ui::NodeType::TextInput ||
        node->type == ui::NodeType::Combobox) {
      if (auto context = detail::context_for_target(roots, *m_focused_target, viewport_size);
          context.has_value()) {
        const bool is_combobox = node->type == ui::NodeType::Combobox;
        const bool extend_selection = event.modifiers.shift;
        const bool primary_shortcut = event.modifiers.primary_shortcut();
        const size_t text_size = node->text.size();

        auto move_caret = [&](size_t next_index, bool extend) {
          const auto *current = document->node(m_focused_target->node_id);
          if (current == nullptr) {
            return;
          }

          const size_t anchor =
              extend ? (current->selection.empty() ? current->caret.index
                                                   : current->selection.anchor)
                     : next_index;
          detail::set_text_input_selection_and_caret(*m_focused_target, anchor, next_index, *context);
        };

        switch (event.key_code) {
          case input::KeyCode::Escape:
            if (!is_combobox) {
              clear_focused_target(true);
              handled_builtin_key = true;
            }
            break;
          case input::KeyCode::Enter:
          case input::KeyCode::KPEnter:
            if (!node->read_only && node->on_submit) {
              auto callback = node->on_submit;
              auto value = node->text;
              document->queue_callback([callback, value]() { callback(value); });
            }
            if (is_combobox) {
              document->set_combobox_open(m_focused_target->node_id, false);
            }
            if (node->caret.active) {
              document->reset_caret_blink(m_focused_target->node_id);
            }
            handled_builtin_key = true;
            break;
          case input::KeyCode::A:
            if (primary_shortcut) {
              detail::set_text_input_selection_and_caret(*m_focused_target, 0u, text_size, *context);
              handled_builtin_key = true;
            }
            break;
          case input::KeyCode::C:
            if (primary_shortcut) {
              input::SET_CLIPBOARD_TEXT(detail::selected_text(*node));
              handled_builtin_key = true;
            }
            break;
          case input::KeyCode::X:
            if (primary_shortcut) {
              input::SET_CLIPBOARD_TEXT(detail::selected_text(*node));
              if (!node->read_only) {
                const auto [start, end] = detail::edit_range(*node);
                std::string next_text = node->text;
                next_text.erase(start, end - start);
                detail::apply_text_input_value(*m_focused_target, std::move(next_text), start, *context, true);
              }
              handled_builtin_key = true;
            }
            break;
          case input::KeyCode::V:
            if (primary_shortcut) {
              if (!node->read_only) {
                const std::string pasted =
                    ui::sanitize_ascii_text(input::CLIPBOARD_TEXT());
                if (!pasted.empty()) {
                  const auto [start, end] = detail::edit_range(*node);
                  std::string next_text = node->text;
                  next_text.replace(start, end - start, pasted);
                  detail::apply_text_input_value(
                      *m_focused_target, std::move(next_text), start + pasted.size(), *context, true
                  );
                } else if (node->caret.active) {
                  document->reset_caret_blink(m_focused_target->node_id);
                }
              }
              handled_builtin_key = true;
            }
            break;
          case input::KeyCode::Backspace:
            if (!node->read_only) {
              if (!node->selection.empty()) {
                const size_t start = node->selection.start();
                std::string next_text = node->text;
                next_text.erase(start, node->selection.end() - start);
                detail::apply_text_input_value(*m_focused_target, std::move(next_text), start, *context, true);
              } else if (node->caret.index > 0u) {
                std::string next_text = node->text;
                next_text.erase(node->caret.index - 1u, 1u);
                detail::apply_text_input_value(
                    *m_focused_target, std::move(next_text), node->caret.index - 1u, *context, true
                );
              }
              handled_builtin_key = true;
            }
            break;
          case input::KeyCode::Delete:
            if (!node->read_only) {
              if (!node->selection.empty()) {
                const size_t start = node->selection.start();
                std::string next_text = node->text;
                next_text.erase(start, node->selection.end() - start);
                detail::apply_text_input_value(*m_focused_target, std::move(next_text), start, *context, true);
              } else if (node->caret.index < node->text.size()) {
                std::string next_text = node->text;
                next_text.erase(node->caret.index, 1u);
                detail::apply_text_input_value(*m_focused_target, std::move(next_text), node->caret.index, *context, true);
              }
              handled_builtin_key = true;
            }
            break;
          case input::KeyCode::Left:
            if (node->selection.empty()) {
              move_caret(node->caret.index > 0u ? node->caret.index - 1u : 0u, extend_selection);
            } else if (extend_selection) {
              move_caret(node->caret.index > 0u ? node->caret.index - 1u : 0u, true);
            } else {
              move_caret(node->selection.start(), false);
            }
            handled_builtin_key = true;
            break;
          case input::KeyCode::Right:
            if (node->selection.empty()) {
              move_caret(std::min(node->caret.index + 1u, text_size), extend_selection);
            } else if (extend_selection) {
              move_caret(std::min(node->caret.index + 1u, text_size), true);
            } else {
              move_caret(node->selection.end(), false);
            }
            handled_builtin_key = true;
            break;
          case input::KeyCode::Home:
            move_caret(0u, extend_selection);
            handled_builtin_key = true;
            break;
          case input::KeyCode::End:
            move_caret(text_size, extend_selection);
            handled_builtin_key = true;
            break;
          case input::KeyCode::Up:
            if (is_combobox && !node->combobox.options.empty()) {
              if (document->combobox_open(m_focused_target->node_id)) {
                detail::move_combobox_highlight(*m_focused_target, -1);
                handled_builtin_key = true;
              } else if (node->combobox.open_on_arrow_keys) {
                document->set_combobox_open(m_focused_target->node_id, true);
                handled_builtin_key = true;
              }
            }
            break;
          case input::KeyCode::Down:
            if (is_combobox && !node->combobox.options.empty()) {
              if (document->combobox_open(m_focused_target->node_id)) {
                detail::move_combobox_highlight(*m_focused_target, 1);
                handled_builtin_key = true;
              } else if (node->combobox.open_on_arrow_keys) {
                document->set_combobox_open(m_focused_target->node_id, true);
                handled_builtin_key = true;
              }
            }
            break;
          default:
            break;
        }
      }
    } else if (node->type == ui::NodeType::Checkbox) {
      switch (event.key_code) {
        case input::KeyCode::Space:
        case input::KeyCode::Enter:
        case input::KeyCode::KPEnter:
          detail::toggle_checkbox_value(*m_focused_target, true);
          handled_builtin_key = true;
          break;
        default:
          break;
      }
    } else if (node->type == ui::NodeType::Slider) {
      switch (event.key_code) {
        case input::KeyCode::Left:
        case input::KeyCode::Down:
          detail::apply_slider_value(
              *m_focused_target, node->slider.value - node->slider.step, true
          );
          handled_builtin_key = true;
          break;
        case input::KeyCode::Right:
        case input::KeyCode::Up:
          detail::apply_slider_value(
              *m_focused_target, node->slider.value + node->slider.step, true
          );
          handled_builtin_key = true;
          break;
        case input::KeyCode::Home:
          detail::apply_slider_value(*m_focused_target, node->slider.min_value, true);
          handled_builtin_key = true;
          break;
        case input::KeyCode::End:
          detail::apply_slider_value(*m_focused_target, node->slider.max_value, true);
          handled_builtin_key = true;
          break;
        default:
          break;
      }
    } else if (node->type == ui::NodeType::Select) {
      const bool is_open = document->select_open(m_focused_target->node_id);
      switch (event.key_code) {
        case input::KeyCode::Escape:
          if (is_open) {
            document->set_select_open(m_focused_target->node_id, false);
            handled_builtin_key = true;
          }
          break;
        case input::KeyCode::Enter:
        case input::KeyCode::KPEnter:
        case input::KeyCode::Space:
          if (node->select.options.empty()) {
            handled_builtin_key = true;
            break;
          }

          if (!is_open) {
            document->set_select_open(m_focused_target->node_id, true);
          } else {
            detail::confirm_select_option(*m_focused_target, node->select.highlighted_index, true);
          }
          handled_builtin_key = true;
          break;
        case input::KeyCode::Up:
          if (is_open) {
            detail::move_select_highlight(*m_focused_target, -1);
            handled_builtin_key = true;
          }
          break;
        case input::KeyCode::Down:
          if (is_open) {
            detail::move_select_highlight(*m_focused_target, 1);
            handled_builtin_key = true;
          }
          break;
        default:
          break;
      }
    } else if (node->type == ui::NodeType::SegmentedControl) {
      switch (event.key_code) {
        case input::KeyCode::Left:
        case input::KeyCode::Up:
          detail::move_segmented_selection(*m_focused_target, -1, true);
          handled_builtin_key = true;
          break;
        case input::KeyCode::Right:
        case input::KeyCode::Down:
          detail::move_segmented_selection(*m_focused_target, 1, true);
          handled_builtin_key = true;
          break;
        case input::KeyCode::Home:
          detail::select_segmented_option(*m_focused_target, 0u, true);
          handled_builtin_key = true;
          break;
        case input::KeyCode::End:
          if (!node->segmented_control.options.empty()) {
            detail::select_segmented_option(
                *m_focused_target, node->segmented_control.options.size() - 1u, true
            );
          }
          handled_builtin_key = true;
          break;
        default:
          break;
      }
    }

    if (!handled_builtin_key && node->on_key_input) {
      auto callback = node->on_key_input;
      document->queue_callback([callback, event]() { callback(event); });
    }

    if (!handled_builtin_key && node->caret.active &&
        detail::should_reset_caret_for_key(event)) {
      document->reset_caret_blink(m_focused_target->node_id);
    }
  }
}

void UISystem::dispatch_character_input(
    const std::vector<detail::RootEntry> &roots,
    const glm::vec2 &viewport_size
) {
  for (const QueuedCharacterInput &queued_character : m_character_inputs) {
    const auto &event = queued_character.event;
    if (!m_focused_target.has_value() ||
        m_focused_target->document == nullptr) {
      continue;
    }

    auto document = m_focused_target->document;
    if (document->consume_suppressed_character_input(event.codepoint)) {
      continue;
    }

    if (!ui::is_supported_text_codepoint(event.codepoint)) {
      continue;
    }

    const auto *node = document->node(m_focused_target->node_id);
    if (node == nullptr) {
      continue;
    }

    bool handled_text_input = false;
    if ((node->type == ui::NodeType::TextInput ||
         node->type == ui::NodeType::Combobox) &&
        !node->read_only) {
      if (auto context = detail::context_for_target(roots, *m_focused_target, viewport_size);
          context.has_value()) {
        const auto [start, end] = detail::edit_range(*node);
        std::string next_text = node->text;
        next_text.replace(start, end - start, std::string(1, static_cast<char>(event.codepoint)));
        detail::apply_text_input_value(*m_focused_target, std::move(next_text), start + 1u, *context, true);
        handled_text_input = true;
      }
    }

    if (!handled_text_input && node->on_character_input) {
      auto callback = node->on_character_input;
      document->queue_callback([callback, event]() { callback(event); });
    }

    if (!handled_text_input && node->caret.active) {
      document->reset_caret_blink(m_focused_target->node_id);
    }
  }
}

void UISystem::dispatch_scroll_input(
    const std::vector<detail::RootEntry> &roots,
    const std::optional<detail::PointerHit> &deepest_hit
) {
  for (const QueuedMouseWheelInput &queued_wheel : m_mouse_wheel_inputs) {
    if (m_panel_resize_drag.has_value() || m_splitter_resize_drag.has_value()) {
      continue;
    }

    if (!deepest_hit.has_value()) {
      continue;
    }

    auto scroll_dispatch = detail::find_scroll_dispatch(
        roots, deepest_hit->target, queued_wheel.event
    );
    if (!scroll_dispatch.has_value() ||
        scroll_dispatch->target.document == nullptr) {
      continue;
    }

    auto *node =
        scroll_dispatch->target.document->node(scroll_dispatch->target.node_id);
    auto *scroll = scroll_dispatch->target.document->scroll_state(
        scroll_dispatch->target.node_id
    );
    if (node == nullptr || scroll == nullptr) {
      continue;
    }

    const glm::vec2 next_offset =
        ui::clamp_scroll_offset(scroll->offset + scroll_dispatch->delta, scroll->max_offset, node->style.scroll_mode);

    if (next_offset != scroll->offset) {
      scroll_dispatch->target.document->set_scroll_offset(
          scroll_dispatch->target.node_id, next_offset
      );
    }

    if (node->on_mouse_wheel) {
      auto callback = node->on_mouse_wheel;
      auto event = queued_wheel.event;
      scroll_dispatch->target.document->queue_callback(
          [callback, event]() { callback(event); }
      );
    }
  }
}

void UISystem::tick_caret_blink(double dt) {
  if (!m_focused_target.has_value() || m_focused_target->document == nullptr) {
    return;
  }

  if (auto *node = m_focused_target->document->node(m_focused_target->node_id);
      node != nullptr && node->caret.active) {
    node->caret.blink_elapsed += dt;

    while (node->caret.blink_elapsed >= 0.5) {
      node->caret.blink_elapsed -= 0.5;
      node->caret.visible = !node->caret.visible;
      m_focused_target->document->mark_paint_dirty();
    }
  }
}

void UISystem::flush_and_relayout(const std::vector<detail::RootEntry> &roots, const glm::vec2 &viewport_size) {
  for (const detail::RootEntry &entry : roots) {
    auto context = detail::make_context(entry, viewport_size);
    entry.document->flush_callbacks();

    if (entry.document->dirty()) {
      ui::layout_document(*entry.document, context);
    }
  }
}

void UISystem::apply_visual_state(
    const std::vector<detail::RootEntry> &roots,
    const std::optional<detail::PointerHit> &deepest_hit
) {
  std::optional<detail::PointerHit> scrollbar_hover_hit;
  if (!m_text_selection_drag.has_value() && !m_scrollbar_drag.has_value() &&
      deepest_hit.has_value() &&
      ui::is_scrollbar_thumb_part(deepest_hit->part)) {
    scrollbar_hover_hit = deepest_hit;
  }

  std::optional<std::pair<Target, ui::UIHitPart>> active_scrollbar;
  if (m_scrollbar_drag.has_value()) {
    active_scrollbar =
        std::make_pair(m_scrollbar_drag->target, m_scrollbar_drag->part);
  }

  std::optional<detail::PointerHit> resize_hover_hit;
  if (!m_panel_resize_drag.has_value() && !m_splitter_resize_drag.has_value() &&
      deepest_hit.has_value() && ui::is_panel_resize_part(deepest_hit->part)) {
    resize_hover_hit = deepest_hit;
  }

  std::optional<std::pair<Target, ui::UIHitPart>> active_resize_hit;
  if (m_panel_resize_drag.has_value()) {
    active_resize_hit =
        std::make_pair(m_panel_resize_drag->target, m_panel_resize_drag->part);
  }

  detail::apply_scrollbar_visual_state(roots, scrollbar_hover_hit, active_scrollbar);
  detail::apply_resize_visual_state(roots, resize_hover_hit, active_resize_hit);
  detail::apply_select_visual_state(roots, deepest_hit);
  detail::apply_item_control_visual_state(
      roots, deepest_hit, m_active_target, m_active_item_part, m_active_item_index
  );
}

void UISystem::resolve_cursor_icon(
    const std::vector<detail::RootEntry> &roots,
    const std::optional<detail::PointerHit> &deepest_hit,
    bool pointer_enabled
) const {
  auto window = window_manager()->active_window();
  if (window == nullptr) {
    return;
  }

  CursorIcon cursor_icon = CursorIcon::Default;
  if (pointer_enabled) {
    if (m_panel_resize_drag.has_value()) {
      cursor_icon = detail::cursor_icon_for_hit_part(
          *m_panel_resize_drag->target.document,
          m_panel_resize_drag->target.node_id,
          m_panel_resize_drag->part
      );
    } else if (m_splitter_resize_drag.has_value()) {
      cursor_icon = detail::cursor_icon_for_hit_part(
          *m_splitter_resize_drag->target.document,
          m_splitter_resize_drag->target.node_id,
          ui::UIHitPart::SplitterBar
      );
    } else if (m_panel_move_drag.has_value()) {
      cursor_icon = CursorIcon::Move;
    } else if (m_slider_drag.has_value()) {
      if (auto slider_cursor = cursor_icon_for_target(m_slider_drag->target);
          slider_cursor.has_value()) {
        cursor_icon = *slider_cursor;
      }
    } else if (deepest_hit.has_value() && ui::is_resize_part(deepest_hit->part)) {
      cursor_icon = detail::cursor_icon_for_hit_part(
          *deepest_hit->target.document, deepest_hit->target.node_id, deepest_hit->part
      );
    } else if (deepest_hit.has_value()) {
      const detail::RootEntry *root_entry =
          detail::find_root_entry(roots, deepest_hit->target);
      if (root_entry != nullptr) {
        const auto hover_target =
            detail::find_hoverable_target(*root_entry, deepest_hit->target.node_id);

        if (hover_target.has_value()) {
          if (auto hover_cursor = cursor_icon_for_target(*hover_target);
              hover_cursor.has_value()) {
            cursor_icon = *hover_cursor;
          }
        }

        if (cursor_icon == CursorIcon::Default &&
            deepest_hit->part == ui::UIHitPart::Body &&
            !hover_target.has_value()) {
          auto drag_handle_target = detail::find_drag_handle_target(
              *root_entry, deepest_hit->target.node_id
          );
          if (drag_handle_target.has_value() &&
              detail::find_draggable_panel_target(
                  *root_entry, drag_handle_target->node_id
              )
                  .has_value()) {
            cursor_icon = CursorIcon::Move;
          }
        }
      }
    }
  }

  window->set_cursor_icon(cursor_icon);
}

void UISystem::build_draw_lists(const std::vector<detail::RootEntry> &roots, const glm::vec2 &viewport_size) const {
  for (const detail::RootEntry &entry : roots) {
    auto context = detail::make_context(entry, viewport_size);
    ui::build_draw_list(*entry.document, context);
  }
}

void UISystem::clear_queued_inputs() {
  m_key_inputs.clear();
  m_character_inputs.clear();
  m_mouse_wheel_inputs.clear();
}

void UISystem::clear_hot_target() {
  if (m_hot_target.has_value() && m_hot_target->document != nullptr) {
    m_hot_target->document->set_hot_node(ui::k_invalid_node_id);
  }

  m_hot_target.reset();
}

void UISystem::clear_active_target(bool queue_release) {
  if (m_active_target.has_value() && m_active_target->document != nullptr) {
    auto document = m_active_target->document;
    if (queue_release) {
      detail::queue_release_callback(*m_active_target);
    }

    document->set_active_node(ui::k_invalid_node_id);
  }

  m_active_target.reset();
  m_active_item_part = ui::UIHitPart::Body;
  m_active_item_index.reset();
}

void UISystem::clear_focused_target(bool queue_blur) {
  if (m_focused_target.has_value() && m_focused_target->document != nullptr) {
    auto document = m_focused_target->document;
    const auto *node = document->node(m_focused_target->node_id);
    if (node != nullptr &&
        (node->type == ui::NodeType::TextInput ||
         node->type == ui::NodeType::Combobox)) {
      document->set_text_selection(
          m_focused_target->node_id,
          ui::UITextSelection{.anchor = node->caret.index, .focus = node->caret.index}
      );
      document->clear_caret(m_focused_target->node_id);
    }

    if (queue_blur && node != nullptr && node->on_blur) {
      document->queue_callback(node->on_blur);
    }

    document->set_focused_node(ui::k_invalid_node_id);
  }

  m_focused_target.reset();
}

void UISystem::clear_secondary_click_state() {
  m_secondary_click_press.reset();
}

void UISystem::clear_text_selection_drag() { m_text_selection_drag.reset(); }

void UISystem::clear_scrollbar_drag() { m_scrollbar_drag.reset(); }

void UISystem::clear_slider_drag() { m_slider_drag.reset(); }

void UISystem::clear_panel_move_drag() { m_panel_move_drag.reset(); }

void UISystem::clear_panel_resize_drag() { m_panel_resize_drag.reset(); }

void UISystem::clear_splitter_resize_drag() { m_splitter_resize_drag.reset(); }

void UISystem::clear_drag_state() {
  clear_text_selection_drag();
  clear_scrollbar_drag();
  clear_slider_drag();
  clear_panel_move_drag();
  clear_panel_resize_drag();
  clear_splitter_resize_drag();
}

void UISystem::clear_pointer_state() {
  clear_hot_target();
  clear_active_target(false);
  clear_focused_target(false);
  clear_secondary_click_state();
  clear_drag_state();
}

void UISystem::set_focused_target(const std::optional<Target> &target) {
  if (detail::same_target(m_focused_target, target)) {
    return;
  }

  clear_focused_target(true);

  if (!target.has_value() || target->document == nullptr) {
    return;
  }

  target->document->set_focused_node(target->node_id);

  if (const auto *node = target->document->node(target->node_id);
      node != nullptr && node->on_focus) {
    target->document->queue_callback(node->on_focus);
  }

  m_focused_target = target;
}

} // namespace astralix
