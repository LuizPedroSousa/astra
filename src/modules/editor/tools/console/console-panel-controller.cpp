#include "console-panel-controller.hpp"

#include "entry-presentation.hpp"
#include "math.hpp"
#include "serialization-context-readers.hpp"

#include <managers/window-manager.hpp>

#include <algorithm>
#include <limits>

namespace astralix::editor {
namespace panel = console_panel;

void ConsolePanelController::mount(const PanelMountContext &context) {
  m_document = context.document;
  m_default_font_id = "fonts::noto_sans_mono";
  m_default_font_size = context.default_font_size;
  m_row_slots.clear();
  m_virtual_list.reset();

  if (m_document != nullptr && m_log_scroll_node != ui::k_invalid_node_id) {
    m_virtual_list = std::make_unique<ui::VirtualListController>(
        m_document,
        m_log_scroll_node,
        [this](size_t slot_index) { return create_row_slot(slot_index); },
        [this](size_t slot_index, ui::UINodeId, size_t item_index) {
          bind_row_slot(slot_index, item_index);
        },
        "console_log_rows"
    );
    m_virtual_list->set_overscan(3u);
  }

  if (m_document != nullptr && m_input_node != ui::k_invalid_node_id) {
    m_document->mutate_style(m_input_node, [this](ui::UIStyle &style) {
      style.font_id = m_default_font_id;
    });
    if (auto *input_node = m_document->node(m_input_node); input_node != nullptr) {
      input_node->combobox.open_on_arrow_keys = false;
    }
    m_document->set_on_key_input(
        m_input_node, [this](const ui::UIKeyInputEvent &event) {
          if (event.key_code == input::KeyCode::R &&
              event.modifiers.primary_shortcut()) {
            if (event.repeat) {
              return;
            }
            summon_suggestions();
            return;
          }

          if (event.key_code == input::KeyCode::Up) {
            navigate_history(-1);
            return;
          }

          if (event.key_code == input::KeyCode::Down) {
            navigate_history(1);
          }
        }
    );
  }

  ConsoleManager::get().set_open(true);
  reset();
}

void ConsolePanelController::unmount() {
  ConsoleManager::get().set_open(false);
  m_virtual_list.reset();
  m_row_slots.clear();
  m_document = nullptr;
}

void ConsolePanelController::update(const PanelUpdateContext &) { update(); }

void ConsolePanelController::load_state(Ref<SerializationContext> state) {
  m_follow_tail =
      serialization::context::read_bool(state, "follow_tail").value_or(true);
  m_expand_all_details =
      serialization::context::read_bool(state, "expand_all_details")
          .value_or(false);
  m_filters_expanded =
      serialization::context::read_bool(state, "filters_expanded")
          .value_or(true);
  m_show_log_entries =
      serialization::context::read_bool(state, "show_log_entries")
          .value_or(true);
  m_show_command_entries =
      serialization::context::read_bool(state, "show_command_entries")
          .value_or(true);
  m_show_output_entries =
      serialization::context::read_bool(state, "show_output_entries")
          .value_or(true);
  m_density = saturate(
      serialization::context::read_float(state, "density").value_or(0.5f)
  );
  m_severity_filter = panel::severity_filter_from_index(
      static_cast<size_t>(
          serialization::context::read_int(state, "severity_filter_index")
              .value_or(0)
      )
  );
}

void ConsolePanelController::save_state(Ref<SerializationContext> state) const {
  if (state == nullptr) {
    return;
  }

  (*state)["follow_tail"] = m_follow_tail;
  (*state)["expand_all_details"] = m_expand_all_details;
  (*state)["filters_expanded"] = m_filters_expanded;
  (*state)["show_log_entries"] = m_show_log_entries;
  (*state)["show_command_entries"] = m_show_command_entries;
  (*state)["show_output_entries"] = m_show_output_entries;
  (*state)["density"] = m_density;
  (*state)["severity_filter_index"] =
      static_cast<int>(severity_filter_index());
}

void ConsolePanelController::reset() {
  m_entries_version = 0u;
  m_force_follow_on_next_refresh = false;
  m_visible_entries.clear();
  clear_history_navigation();
  m_expanded_source_index.reset();

  if (m_virtual_list != nullptr) {
    m_virtual_list->reset();
  }

  if (m_document != nullptr) {
    m_document->set_visible(m_root_node, true);
    m_document->set_visible(m_filters_row_node, m_filters_expanded);
    m_document->mutate_style(
        m_severity_node,
        [accent = panel::severity_accent_color(m_severity_filter)](
            ui::UIStyle &style
        ) { style.accent_color = accent; }
    );
    m_document->set_segmented_selected_index(
        m_severity_node, severity_filter_index()
    );
    m_document->set_chip_selected(
        m_source_filters_node, 0u, m_show_log_entries
    );
    m_document->set_chip_selected(
        m_source_filters_node, 1u, m_show_command_entries
    );
    m_document->set_chip_selected(
        m_source_filters_node, 2u, m_show_output_entries
    );
    m_document->set_text(m_input_node, m_input_value);
    m_document->set_caret(m_input_node, m_input_value.size(), false);
  }

  refresh_suggestions(false);
  refresh(true);
}

void ConsolePanelController::set_open(bool open) {
  auto &console = ConsoleManager::get();
  if (console.is_open() == open) {
    return;
  }

  console.set_open(open);
  clear_history_navigation();
  if (auto window = window_manager()->active_window(); window != nullptr) {
    window->capture_cursor(!open);
  }

  if (m_document != nullptr) {
    m_document->set_visible(m_root_node, open);
    if (open) {
      m_document->suppress_next_character_input(static_cast<uint32_t>('`'));
      m_document->request_focus(m_input_node);
      m_document->set_caret(m_input_node, m_input_value.size(), true);
      refresh_suggestions(false);
    } else {
      m_document->set_combobox_open(m_input_node, false);
    }
  }

  if (open) {
    m_force_follow_on_next_refresh = true;
    refresh(true);
    if (m_follow_tail) {
      scroll_to_bottom();
    }
  }
}

void ConsolePanelController::update() {
  if (m_document == nullptr || m_log_scroll_node == ui::k_invalid_node_id) {
    return;
  }

  refresh();

  if (m_force_follow_on_next_refresh && m_follow_tail) {
    scroll_to_bottom();
  }

  m_force_follow_on_next_refresh = false;
}

void ConsolePanelController::set_input_value(std::string value) {
  clear_history_navigation();
  m_input_value = std::move(value);
  refresh_suggestions(suggestions_open());
}

void ConsolePanelController::accept_suggestion(std::string value) {
  clear_history_navigation();
  m_input_value = std::move(value);
  refresh_suggestions(false);
}

void ConsolePanelController::summon_suggestions() { refresh_suggestions(true); }

void ConsolePanelController::submit_command(std::string value) {
  bool accepted_highlighted_suggestion = false;
  if (m_document != nullptr && m_input_node != ui::k_invalid_node_id &&
      m_document->combobox_open(m_input_node)) {
    if (const auto *suggestions = m_document->combobox_options(m_input_node);
        suggestions != nullptr && !suggestions->empty()) {
      const size_t highlighted_index = std::min(
          m_document->combobox_highlighted_index(m_input_node),
          suggestions->size() - 1u
      );
      value = (*suggestions)[highlighted_index];
      accepted_highlighted_suggestion = true;
    }
  }

  clear_history_navigation();
  m_input_value = std::move(value);

  auto result = ConsoleManager::get().execute(m_input_value);
  if (!result.executed) {
    if (accepted_highlighted_suggestion) {
      set_input_text(m_input_value);
      refresh_suggestions(false);
    }
    return;
  }

  m_force_follow_on_next_refresh = true;
  set_input_text({});
  refresh_suggestions(false);
}

void ConsolePanelController::clear_entries() {
  ConsoleManager::get().clear_entries();
  m_entries_version = 0u;
  m_force_follow_on_next_refresh = true;
}

void ConsolePanelController::set_follow_tail(bool follow_tail) {
  if (m_follow_tail == follow_tail) {
    return;
  }

  m_follow_tail = follow_tail;
  if (m_follow_tail) {
    m_force_follow_on_next_refresh = true;
    scroll_to_bottom();
  }
}

void ConsolePanelController::set_expand_all_details(bool expand_all_details) {
  if (m_expand_all_details == expand_all_details) {
    return;
  }

  m_expand_all_details = expand_all_details;
  if (!m_expand_all_details) {
    m_expanded_source_index.reset();
  }

  refresh(true);
}

void ConsolePanelController::set_density(float density) {
  const float clamped_density = saturate(density);
  if (m_density == clamped_density) {
    return;
  }

  m_density = clamped_density;
  refresh(true);
}

size_t ConsolePanelController::severity_filter_index() const {
  switch (m_severity_filter) {
    case SeverityFilter::Info:
      return 1u;
    case SeverityFilter::Warning:
      return 2u;
    case SeverityFilter::Error:
      return 3u;
    case SeverityFilter::Debug:
      return 4u;
    case SeverityFilter::All:
    default:
      return 0u;
  }
}

void ConsolePanelController::set_severity_filter_index(size_t index) {
  const SeverityFilter next_filter = panel::severity_filter_from_index(index);
  if (m_severity_filter == next_filter) {
    return;
  }

  m_severity_filter = next_filter;
  if (m_document != nullptr) {
    m_document->mutate_style(
        m_severity_node,
        [accent = panel::severity_accent_color(m_severity_filter)](
            ui::UIStyle &style
        ) { style.accent_color = accent; }
    );
    m_document->set_segmented_selected_index(
        m_severity_node, severity_filter_index()
    );
  }
  refresh(true);
}

void ConsolePanelController::toggle_filters_expanded() {
  m_filters_expanded = !m_filters_expanded;
  if (m_document != nullptr) {
    m_document->set_visible(m_filters_row_node, m_filters_expanded);
    m_document->set_visible(m_filters_divider_node, m_filters_expanded);
  }
}

bool ConsolePanelController::source_filter_enabled(size_t index) const {
  switch (index) {
    case 0u:
      return m_show_log_entries;
    case 1u:
      return m_show_command_entries;
    case 2u:
      return m_show_output_entries;
    default:
      return false;
  }
}

void ConsolePanelController::set_source_filter_enabled(size_t index, bool enabled) {
  bool *target = nullptr;
  switch (index) {
    case 0u:
      target = &m_show_log_entries;
      break;
    case 1u:
      target = &m_show_command_entries;
      break;
    case 2u:
      target = &m_show_output_entries;
      break;
    default:
      return;
  }

  if (*target == enabled) {
    return;
  }

  *target = enabled;
  if (m_document != nullptr) {
    m_document->set_chip_selected(m_source_filters_node, index, enabled);
  }
  refresh(true);
}

void ConsolePanelController::clear_history_navigation() {
  m_history_navigation_index.reset();
  m_history_navigation_draft.clear();
}

void ConsolePanelController::navigate_history(int direction) {
  const auto &history = ConsoleManager::get().history();
  if (history.empty()) {
    return;
  }

  if (direction < 0) {
    if (!m_history_navigation_index.has_value()) {
      m_history_navigation_draft = m_input_value;
      m_history_navigation_index = history.size() - 1u;
    } else if (*m_history_navigation_index > 0u) {
      --(*m_history_navigation_index);
    }

    set_input_text(history[*m_history_navigation_index]);
    refresh_suggestions(false);
    return;
  }

  if (direction > 0) {
    if (!m_history_navigation_index.has_value()) {
      return;
    }

    if (*m_history_navigation_index + 1u < history.size()) {
      ++(*m_history_navigation_index);
      set_input_text(history[*m_history_navigation_index]);
    } else {
      const std::string draft = m_history_navigation_draft;
      clear_history_navigation();
      set_input_text(draft);
    }

    refresh_suggestions(false);
  }
}

void ConsolePanelController::scroll_to_bottom() {
  if (m_document == nullptr || m_log_scroll_node == ui::k_invalid_node_id) {
    return;
  }

  const auto *scroll_state = m_document->scroll_state(m_log_scroll_node);
  const float current_x =
      scroll_state != nullptr ? scroll_state->offset.x : 0.0f;
  m_document->set_scroll_offset(
      m_log_scroll_node,
      glm::vec2(current_x, std::numeric_limits<float>::max())
  );
}

void ConsolePanelController::set_input_text(const std::string &value) {
  m_input_value = value;

  if (m_document == nullptr || m_input_node == ui::k_invalid_node_id) {
    return;
  }

  m_document->set_text(m_input_node, value);
  m_document->set_text_selection(
      m_input_node,
      ui::UITextSelection{.anchor = value.size(), .focus = value.size()}
  );
  m_document->set_caret(m_input_node, value.size(), true);
  m_document->request_focus(m_input_node);
}

void ConsolePanelController::refresh_suggestions(bool open_popup) {
  if (m_document == nullptr || m_input_node == ui::k_invalid_node_id) {
    return;
  }

  const auto suggestions = build_console_command_suggestions(
      m_input_value,
      ConsoleManager::get().commands(),
      ConsoleManager::get().history()
  );
  const auto history_autocomplete = build_console_history_autocomplete(
      m_input_value, ConsoleManager::get().history()
  );

  m_document->set_combobox_options(m_input_node, suggestions);
  m_document->set_combobox_highlighted_index(m_input_node, 0u);
  m_document->set_autocomplete_text(
      m_input_node, history_autocomplete.value_or(std::string{})
  );
  m_document->set_combobox_open(
      m_input_node, open_popup && !suggestions.empty()
  );
}

bool ConsolePanelController::suggestions_open() const {
  if (m_document == nullptr || m_input_node == ui::k_invalid_node_id) {
    return false;
  }

  return m_document->combobox_open(m_input_node);
}

} // namespace astralix::editor
