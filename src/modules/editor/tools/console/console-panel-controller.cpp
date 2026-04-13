#include "console-panel-controller.hpp"

#include "entry-presentation.hpp"
#include "fnv1a.hpp"
#include "math.hpp"
#include "serialization-context-readers.hpp"

#include <managers/window-manager.hpp>

#include <algorithm>

namespace astralix::editor {

void ConsolePanelController::invalidate_runtime_handles() {
  m_log_scroll_widget = ui::im::k_invalid_widget_id;
  m_input_widget = ui::im::k_invalid_widget_id;
  m_source_chip_widget = ui::im::k_invalid_widget_id;
  m_severity_chip_widget = ui::im::k_invalid_widget_id;
  m_header_frozen = false;
}

void ConsolePanelController::mount(const PanelMountContext &context) {
  m_runtime = context.runtime;
  m_default_font_id = "fonts::noto_sans_mono";
  m_default_font_size = context.default_font_size;
  invalidate_runtime_handles();

  ConsoleManager::get().set_open(true);
  if (m_follow_tail) {
    m_force_follow_on_next_refresh = true;
  }
  refresh_suggestions(m_suggestions_open);
  refresh(true);
}

void ConsolePanelController::unmount() {
  ConsoleManager::get().set_open(false);
  set_input_capture(false);
  close_filter_popovers();
  m_runtime = nullptr;
  invalidate_runtime_handles();
  m_force_scroll_to_bottom_once = false;
  m_request_input_focus = false;
  m_request_input_select_to_end = false;
  mark_render_dirty();
}

void ConsolePanelController::update(const PanelUpdateContext &) { update(); }

std::optional<uint64_t> ConsolePanelController::render_version() const {
  uint64_t hash = k_fnv1a64_offset_basis;
  hash = fnv1a64_append_value(hash, m_render_revision);
  hash = fnv1a64_append_value(hash, m_log_scroll_widget.value);
  hash = fnv1a64_append_value(hash, m_input_widget.value);

  if (m_runtime != nullptr && m_log_scroll_widget != ui::im::k_invalid_widget_id) {
    const auto scroll_state = m_runtime->virtual_list_state(m_log_scroll_widget);
    hash = fnv1a64_append_value(hash, scroll_state.scroll_offset.x);
    hash = fnv1a64_append_value(hash, scroll_state.scroll_offset.y);
    hash = fnv1a64_append_value(hash, scroll_state.viewport_width);
    hash = fnv1a64_append_value(hash, scroll_state.viewport_height);
  }

  if (m_runtime != nullptr && m_input_widget != ui::im::k_invalid_widget_id) {
    const bool combobox_open = m_runtime->combobox_open(m_input_widget);
    const size_t highlighted_index =
        m_runtime->combobox_highlighted_index(m_input_widget);
    hash = fnv1a64_append_value(hash, combobox_open);
    hash = fnv1a64_append_value(hash, highlighted_index);
  }

  return hash;
}

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

  refresh_suggestions(false);
  refresh(true);
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
  m_visible_entries_content_width = 0.0f;
  m_collapsed_source_indices.clear();
  clear_history_navigation();
  m_expanded_source_index.reset();
  close_filter_popovers();

  refresh_suggestions(false);
  refresh(true);
  mark_render_dirty();
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

  if (open) {
    m_request_input_focus = true;
    m_request_input_select_to_end = true;
    refresh_suggestions(false);
    m_force_follow_on_next_refresh = true;
    mark_render_dirty();
    refresh(true);
    if (m_follow_tail) {
      scroll_to_bottom();
    }
    return;
  }

  set_input_capture(false);
  close_filter_popovers();
  m_suggestions_open = false;
  mark_render_dirty();
}

void ConsolePanelController::update() { refresh(); }

void ConsolePanelController::set_input_value(std::string value) {
  clear_history_navigation();
  m_input_value = std::move(value);
  refresh_suggestions(suggestions_open());
  mark_render_dirty();
}

void ConsolePanelController::accept_suggestion(std::string value) {
  clear_history_navigation();
  m_input_value = std::move(value);
  m_request_input_focus = true;
  m_request_input_select_to_end = true;
  refresh_suggestions(false);
  mark_render_dirty();
}

void ConsolePanelController::summon_suggestions() {
  m_request_input_focus = true;
  m_request_input_select_to_end = true;
  refresh_suggestions(true);
  mark_render_dirty();
}

void ConsolePanelController::submit_command(std::string value) {
  bool accepted_highlighted_suggestion = false;
  if (m_suggestions_open && !m_input_suggestions.empty()) {
    size_t highlighted_index = 0u;
    if (m_runtime != nullptr && m_input_widget) {
      highlighted_index = std::min(
          m_runtime->combobox_highlighted_index(m_input_widget),
          m_input_suggestions.size() - 1u
      );
    }

    value = m_input_suggestions[highlighted_index];
    accepted_highlighted_suggestion = true;
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
  mark_render_dirty();
}

void ConsolePanelController::clear_entries() {
  ConsoleManager::get().clear_entries();
  m_entries_version = 0u;
  m_collapsed_source_indices.clear();
  m_force_follow_on_next_refresh = true;
  m_force_scroll_to_bottom_once = false;
  mark_render_dirty();
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
  mark_render_dirty();
}

void ConsolePanelController::set_expand_all_details(bool expand_all_details) {
  if (m_expand_all_details == expand_all_details) {
    return;
  }

  m_expand_all_details = expand_all_details;
  m_collapsed_source_indices.clear();
  if (!m_expand_all_details) {
    m_expanded_source_index.reset();
  }

  refresh(true);
  mark_render_dirty();
}

void ConsolePanelController::set_density(float density) {
  const float clamped_density = saturate(density);
  if (m_density == clamped_density) {
    return;
  }

  m_density = clamped_density;
  refresh(true);
  mark_render_dirty();
}

void ConsolePanelController::toggle_severity_all() {
  if (m_severity_filter_all) {
    return;
  }

  m_severity_filter_all = true;
  m_severity_filter_info = false;
  m_severity_filter_warning = false;
  m_severity_filter_error = false;
  m_severity_filter_debug = false;
  refresh(true);
  mark_render_dirty();
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

  refresh(true);
  mark_render_dirty();
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
  refresh(true);
  mark_render_dirty();
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

void ConsolePanelController::close_filter_popovers() {
  if (!m_source_filter_popover_open && !m_severity_filter_popover_open) {
    return;
  }

  m_source_filter_popover_open = false;
  m_severity_filter_popover_open = false;
  mark_render_dirty();
}

void ConsolePanelController::toggle_source_filter_popover() {
  const bool next_open = !m_source_filter_popover_open;
  close_filter_popovers();
  m_source_filter_popover_open = next_open;
  mark_render_dirty();
}

void ConsolePanelController::toggle_severity_filter_popover() {
  const bool next_open = !m_severity_filter_popover_open;
  close_filter_popovers();
  m_severity_filter_popover_open = next_open;
  mark_render_dirty();
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
  m_force_scroll_to_bottom_once = true;
  mark_render_dirty();
}

void ConsolePanelController::set_input_text(const std::string &value) {
  m_input_value = value;
  m_request_input_focus = true;
  m_request_input_select_to_end = true;
  mark_render_dirty();
}

void ConsolePanelController::refresh_suggestions(bool open_popup) {
  const auto previous_suggestions = m_input_suggestions;
  const std::string previous_autocomplete = m_input_autocomplete;
  const bool previous_open = m_suggestions_open;
  m_input_suggestions = build_console_command_suggestions(
      m_input_value,
      ConsoleManager::get().commands(),
      ConsoleManager::get().history()
  );
  m_input_autocomplete = build_console_history_autocomplete(
                             m_input_value, ConsoleManager::get().history()
  )
                             .value_or(std::string{});
  m_suggestions_open = open_popup && !m_input_suggestions.empty();

  if (m_input_suggestions != previous_suggestions ||
      m_input_autocomplete != previous_autocomplete ||
      m_suggestions_open != previous_open) {
    mark_render_dirty();
  }
}

bool ConsolePanelController::suggestions_open() const { return m_suggestions_open; }

} // namespace astralix::editor
