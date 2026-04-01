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
        }
    );
    m_virtual_list->set_overscan(1u);
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
  set_input_capture(false);
  close_filter_popovers();
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
  m_severity_filter_all =
      serialization::context::read_bool(state, "severity_filter_all")
          .value_or(true);
  m_severity_filter_info =
      serialization::context::read_bool(state, "severity_filter_info")
          .value_or(false);
  m_severity_filter_warning =
      serialization::context::read_bool(state, "severity_filter_warning")
          .value_or(false);
  m_severity_filter_error =
      serialization::context::read_bool(state, "severity_filter_error")
          .value_or(false);
  m_severity_filter_debug =
      serialization::context::read_bool(state, "severity_filter_debug")
          .value_or(false);

  if (m_document != nullptr) {
    sync_filter_ui();
    refresh(true);
  }
}

void ConsolePanelController::save_state(Ref<SerializationContext> state) const {
  if (state == nullptr) {
    return;
  }

  (*state)["follow_tail"] = m_follow_tail;
  (*state)["expand_all_details"] = m_expand_all_details;
  (*state)["show_log_entries"] = m_show_log_entries;
  (*state)["show_command_entries"] = m_show_command_entries;
  (*state)["show_output_entries"] = m_show_output_entries;
  (*state)["density"] = m_density;
  (*state)["severity_filter_all"] = m_severity_filter_all;
  (*state)["severity_filter_info"] = m_severity_filter_info;
  (*state)["severity_filter_warning"] = m_severity_filter_warning;
  (*state)["severity_filter_error"] = m_severity_filter_error;
  (*state)["severity_filter_debug"] = m_severity_filter_debug;
}

void ConsolePanelController::reset() {
  m_entries_version = 0u;
  m_force_follow_on_next_refresh = false;
  m_force_scroll_to_bottom_once = false;
  m_visible_entries.clear();
  m_collapsed_source_indices.clear();
  m_line_height_cache.clear();
  m_text_width_cache.clear();
  clear_history_navigation();
  m_expanded_source_index.reset();

  if (m_virtual_list != nullptr) {
    m_virtual_list->reset();
  }

  if (m_document != nullptr) {
    m_document->set_visible(m_root_node, true);
    close_filter_popovers();
    sync_filter_ui();
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
      set_input_capture(true);
      m_document->set_caret(m_input_node, m_input_value.size(), true);
      refresh_suggestions(false);
    } else {
      set_input_capture(false);
      close_filter_popovers();
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

  if (m_force_scroll_to_bottom_once ||
      (m_force_follow_on_next_refresh && m_follow_tail)) {
    scroll_to_bottom();
  }

  m_force_follow_on_next_refresh = false;
  m_force_scroll_to_bottom_once = false;
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

  m_force_scroll_to_bottom_once = true;
  set_input_text({});
  refresh_suggestions(false);
}

void ConsolePanelController::clear_entries() {
  ConsoleManager::get().clear_entries();
  m_entries_version = 0u;
  m_collapsed_source_indices.clear();
  m_force_follow_on_next_refresh = true;
  m_force_scroll_to_bottom_once = false;
}

void ConsolePanelController::set_follow_tail(bool follow_tail) {
  if (m_follow_tail == follow_tail) {
    return;
  }

  m_follow_tail = follow_tail;
  if (m_document != nullptr &&
      m_follow_tail_toggle_node != ui::k_invalid_node_id) {
    m_document->set_checked(m_follow_tail_toggle_node, m_follow_tail);
  }
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
  m_collapsed_source_indices.clear();
  if (m_document != nullptr &&
      m_expand_details_toggle_node != ui::k_invalid_node_id) {
    m_document->set_checked(
        m_expand_details_toggle_node, m_expand_all_details
    );
  }
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
  if (m_document != nullptr && m_density_node != ui::k_invalid_node_id) {
    m_document->set_slider_value(m_density_node, m_density);
  }
  refresh(true);
}

void ConsolePanelController::toggle_severity_all() {
  if (m_severity_filter_all) {
    sync_filter_ui();
    return;
  }

  m_severity_filter_all = true;
  m_severity_filter_info = false;
  m_severity_filter_warning = false;
  m_severity_filter_error = false;
  m_severity_filter_debug = false;
  sync_filter_ui();
  refresh(true);
}

void ConsolePanelController::toggle_severity_option(size_t index) {
  bool *target = nullptr;
  switch (index) {
    case 1u:
      target = &m_severity_filter_info;
      break;
    case 2u:
      target = &m_severity_filter_warning;
      break;
    case 3u:
      target = &m_severity_filter_error;
      break;
    case 4u:
      target = &m_severity_filter_debug;
      break;
    default:
      return;
  }

  if (m_severity_filter_all) {
    m_severity_filter_all = false;
    m_severity_filter_info = false;
    m_severity_filter_warning = false;
    m_severity_filter_error = false;
    m_severity_filter_debug = false;
    *target = true;
  } else {
    *target = !(*target);
    if (!m_severity_filter_info && !m_severity_filter_warning &&
        !m_severity_filter_error && !m_severity_filter_debug) {
      m_severity_filter_all = true;
    }
  }

  sync_filter_ui();
  refresh(true);
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
  sync_filter_ui();
  refresh(true);
}

void ConsolePanelController::sync_filter_ui() {
  if (m_document == nullptr) {
    return;
  }

  const ConsolePanelTheme theme;

  if (m_follow_tail_toggle_node != ui::k_invalid_node_id) {
    m_document->set_checked(m_follow_tail_toggle_node, m_follow_tail);
  }

  if (m_expand_details_toggle_node != ui::k_invalid_node_id) {
    m_document->set_checked(
        m_expand_details_toggle_node, m_expand_all_details
    );
  }

  if (m_density_node != ui::k_invalid_node_id) {
    m_document->set_slider_value(m_density_node, m_density);
  }

  if (m_severity_node != ui::k_invalid_node_id) {
    m_document->mutate_style(
        m_severity_node,
        [accent = theme.accent](ui::UIStyle &style) {
          style.accent_color = accent;
        }
    );
  }

  for (size_t index = 0u; index < m_severity_filter_option_nodes.size();
       ++index) {
    if (m_severity_filter_option_nodes[index] != ui::k_invalid_node_id) {
      m_document->set_checked(
          m_severity_filter_option_nodes[index], severity_filter_enabled(index)
      );
    }
  }

  if (m_source_filters_node != ui::k_invalid_node_id) {
    m_document->set_chip_selected(
        m_source_filters_node, 0u, m_show_log_entries
    );
    m_document->set_chip_selected(
        m_source_filters_node, 1u, m_show_command_entries
    );
    m_document->set_chip_selected(
        m_source_filters_node, 2u, m_show_output_entries
    );
  }

  for (size_t index = 0u; index < m_source_filter_option_nodes.size(); ++index) {
    if (m_source_filter_option_nodes[index] != ui::k_invalid_node_id) {
      m_document->set_checked(
          m_source_filter_option_nodes[index], source_filter_enabled(index)
      );
    }
  }

  if (m_source_chip_summary_node != ui::k_invalid_node_id) {
    m_document->set_text(m_source_chip_summary_node, source_filter_summary());
  }

  if (m_severity_chip_summary_node != ui::k_invalid_node_id) {
    m_document->set_text(
        m_severity_chip_summary_node, severity_filter_summary()
    );
  }

  apply_filter_chip_style(
      m_source_chip_trigger_node,
      m_source_chip_summary_node,
      theme.accent,
      !source_filter_is_default()
  );
  apply_filter_chip_style(
      m_severity_chip_trigger_node,
      m_severity_chip_summary_node,
      theme.accent,
      !severity_filter_is_default()
  );
}

void ConsolePanelController::apply_filter_chip_style(
    ui::UINodeId trigger_node,
    ui::UINodeId summary_node,
    const glm::vec4 &accent,
    bool active
) {
  if (m_document == nullptr) {
    return;
  }

  const ConsolePanelTheme theme;

  if (trigger_node != ui::k_invalid_node_id) {
    m_document->mutate_style(
        trigger_node,
        [theme, accent, active](ui::UIStyle &style) {
          style.background_color =
              active ? panel::alpha(accent, 0.16f)
                     : theme_alpha(theme.handle, 0.72f);
          style.border_color =
              active ? panel::alpha(accent, 0.62f)
                     : theme_alpha(theme.panel_border, 0.70f);
          style.hovered_style.background_color =
              active ? panel::alpha(accent, 0.22f) : theme.handle;
          style.pressed_style.background_color =
              active ? panel::alpha(accent, 0.28f) : k_theme.bunker_1000;
          style.focused_style.border_color = accent;
          style.focused_style.border_width = 2.0f;
        }
    );
  }

  if (summary_node != ui::k_invalid_node_id) {
    m_document->mutate_style(
        summary_node,
        [theme, accent, active](ui::UIStyle &style) {
          style.text_color = active ? accent : theme.text_primary;
        }
    );
  }
}

std::string ConsolePanelController::source_filter_summary() const {
  if (source_filter_is_default()) {
    return "All sources";
  }

  std::vector<std::string> enabled_sources;
  if (m_show_log_entries) {
    enabled_sources.emplace_back("Logs");
  }
  if (m_show_command_entries) {
    enabled_sources.emplace_back("Commands");
  }
  if (m_show_output_entries) {
    enabled_sources.emplace_back("Output");
  }

  if (enabled_sources.empty()) {
    return "No sources";
  }

  if (enabled_sources.size() == 1u) {
    return enabled_sources.front();
  }

  return enabled_sources[0] + " + " + enabled_sources[1];
}

std::string ConsolePanelController::severity_filter_summary() const {
  if (m_severity_filter_all) {
    return "All severities";
  }

  std::vector<std::string> enabled_severities;
  if (m_severity_filter_info) {
    enabled_severities.emplace_back("Info");
  }
  if (m_severity_filter_warning) {
    enabled_severities.emplace_back("Warn");
  }
  if (m_severity_filter_error) {
    enabled_severities.emplace_back("Error");
  }
  if (m_severity_filter_debug) {
    enabled_severities.emplace_back("Debug");
  }

  if (enabled_severities.empty()) {
    return "All severities";
  }

  std::string result = enabled_severities[0];
  for (size_t index = 1u; index < enabled_severities.size(); ++index) {
    result += " + " + enabled_severities[index];
  }

  return result;
}

bool ConsolePanelController::source_filter_is_default() const {
  return m_show_log_entries && m_show_command_entries && m_show_output_entries;
}

bool ConsolePanelController::severity_filter_is_default() const {
  return m_severity_filter_all;
}

bool ConsolePanelController::severity_filter_enabled(size_t index) const {
  switch (index) {
    case 0u:
      return m_severity_filter_all;
    case 1u:
      return m_severity_filter_info;
    case 2u:
      return m_severity_filter_warning;
    case 3u:
      return m_severity_filter_error;
    case 4u:
      return m_severity_filter_debug;
    default:
      return false;
  }
}

bool ConsolePanelController::popover_open(ui::UINodeId node_id) const {
  if (m_document == nullptr || node_id == ui::k_invalid_node_id) {
    return false;
  }

  const auto *node = m_document->node(node_id);
  return node != nullptr && node->type == ui::NodeType::Popover &&
         node->popover.open;
}

void ConsolePanelController::close_filter_popovers() {
  if (m_document == nullptr) {
    return;
  }

  if (popover_open(m_source_popover_node)) {
    m_document->close_popover(m_source_popover_node);
  }

  if (popover_open(m_severity_popover_node)) {
    m_document->close_popover(m_severity_popover_node);
  }
}

void ConsolePanelController::toggle_source_filter_popover() {
  if (m_document == nullptr || m_source_popover_node == ui::k_invalid_node_id ||
      m_source_chip_trigger_node == ui::k_invalid_node_id) {
    return;
  }

  if (popover_open(m_source_popover_node)) {
    m_document->close_popover(m_source_popover_node);
    return;
  }

  m_document->open_popover_anchored_to(
      m_source_popover_node,
      m_source_chip_trigger_node,
      ui::UIPopupPlacement::BottomStart,
      0u
  );
}

void ConsolePanelController::toggle_severity_filter_popover() {
  if (m_document == nullptr ||
      m_severity_popover_node == ui::k_invalid_node_id ||
      m_severity_chip_trigger_node == ui::k_invalid_node_id) {
    return;
  }

  if (popover_open(m_severity_popover_node)) {
    m_document->close_popover(m_severity_popover_node);
    return;
  }

  m_document->open_popover_anchored_to(
      m_severity_popover_node,
      m_severity_chip_trigger_node,
      ui::UIPopupPlacement::BottomStart,
      0u
  );
}

void ConsolePanelController::set_input_capture(bool captures_input) {
  ConsoleManager::get().set_captures_input(captures_input);
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
