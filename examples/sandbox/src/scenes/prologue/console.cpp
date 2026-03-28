#include "console.hpp"

#include <managers/window-manager.hpp>

#include "astralix/modules/ui/dsl.hpp"
#include "astralix/shared/foundation/console.hpp"

#include <algorithm>
#include <limits>
#include <sstream>
#include <string>

using namespace astralix;

namespace {

using namespace ui::dsl;
using namespace ui::dsl::styles;

struct ConsoleDensityMetrics {
  float row_padding = 8.0f;
  float row_gap = 4.0f;
  float primary_font_size = 15.0f;
  float secondary_font_size = 13.0f;
};

std::string log_level_label(LogLevel level) {
  switch (level) {
    case LogLevel::INFO:
      return "INFO";
    case LogLevel::WARNING:
      return "WARN";
    case LogLevel::ERROR:
      return "ERROR";
    case LogLevel::DEBUG:
      return "DEBUG";
  }

  return "INFO";
}

glm::vec4 primary_color_for_entry(const ConsoleEntry &entry) {
  switch (entry.source) {
    case ConsoleEntrySource::Command:
      return glm::vec4(0.65f, 0.85f, 1.0f, 1.0f);
    case ConsoleEntrySource::Output:
      return entry.level == LogLevel::ERROR
                 ? glm::vec4(1.0f, 0.69f, 0.69f, 1.0f)
                 : glm::vec4(0.86f, 0.92f, 1.0f, 1.0f);
    case ConsoleEntrySource::Logger:
    default:
      switch (entry.level) {
        case LogLevel::WARNING:
          return glm::vec4(1.0f, 0.87f, 0.58f, 1.0f);
        case LogLevel::ERROR:
          return glm::vec4(1.0f, 0.68f, 0.68f, 1.0f);
        case LogLevel::DEBUG:
          return glm::vec4(0.68f, 0.83f, 1.0f, 1.0f);
        case LogLevel::INFO:
        default:
          return glm::vec4(0.86f, 0.93f, 1.0f, 1.0f);
      }
  }
}

glm::vec4 background_color_for_entry(const ConsoleEntry &entry) {
  switch (entry.source) {
    case ConsoleEntrySource::Command:
      return glm::vec4(0.07f, 0.15f, 0.24f, 0.82f);
    case ConsoleEntrySource::Output:
      return entry.level == LogLevel::ERROR
                 ? glm::vec4(0.2f, 0.08f, 0.09f, 0.8f)
                 : glm::vec4(0.06f, 0.1f, 0.17f, 0.8f);
    case ConsoleEntrySource::Logger:
    default:
      switch (entry.level) {
        case LogLevel::WARNING:
          return glm::vec4(0.2f, 0.16f, 0.06f, 0.8f);
        case LogLevel::ERROR:
          return glm::vec4(0.22f, 0.08f, 0.09f, 0.8f);
        case LogLevel::DEBUG:
          return glm::vec4(0.06f, 0.11f, 0.2f, 0.8f);
        case LogLevel::INFO:
        default:
          return glm::vec4(0.05f, 0.09f, 0.15f, 0.78f);
      }
  }
}

std::string primary_text_for_entry(const ConsoleEntry &entry) {
  switch (entry.source) {
    case ConsoleEntrySource::Command:
      return "> " + entry.message;
    case ConsoleEntrySource::Output:
      return entry.message;
    case ConsoleEntrySource::Logger:
    default:
      return "[" + log_level_label(entry.level) + "] " + entry.timestamp + " " +
             entry.message;
  }
}

std::string secondary_text_for_entry(const ConsoleEntry &entry) {
  if (entry.source != ConsoleEntrySource::Logger ||
      entry.level == LogLevel::INFO || entry.file.empty()) {
    return {};
  }

  std::ostringstream stream;
  stream << entry.file;
  if (entry.line > 0) {
    stream << "::" << entry.line;
  }
  if (!entry.caller.empty()) {
    stream << " [" << entry.caller << "]";
  }

  return stream.str();
}

float clamp_density(float density) { return std::clamp(density, 0.0f, 1.0f); }

float lerp(float compact, float roomy, float t) {
  return compact + (roomy - compact) * clamp_density(t);
}

ConsoleDensityMetrics density_metrics(float density) {
  const float clamped_density = clamp_density(density);
  return ConsoleDensityMetrics{
      .row_padding = lerp(6.0f, 10.0f, clamped_density),
      .row_gap = lerp(2.0f, 5.0f, clamped_density),
      .primary_font_size = lerp(13.0f, 16.0f, clamped_density),
      .secondary_font_size = lerp(11.0f, 14.0f, clamped_density),
  };
}

ConsoleController::SeverityFilter severity_filter_from_index(size_t index) {
  switch (index) {
    case 1u:
      return ConsoleController::SeverityFilter::Info;
    case 2u:
      return ConsoleController::SeverityFilter::Warning;
    case 3u:
      return ConsoleController::SeverityFilter::Error;
    case 4u:
      return ConsoleController::SeverityFilter::Debug;
    case 0u:
    default:
      return ConsoleController::SeverityFilter::All;
  }
}

bool matches_severity_filter(const ConsoleEntry &entry,
                             ConsoleController::SeverityFilter filter) {
  switch (filter) {
    case ConsoleController::SeverityFilter::Info:
      return entry.level == LogLevel::INFO;
    case ConsoleController::SeverityFilter::Warning:
      return entry.level == LogLevel::WARNING;
    case ConsoleController::SeverityFilter::Error:
      return entry.level == LogLevel::ERROR;
    case ConsoleController::SeverityFilter::Debug:
      return entry.level == LogLevel::DEBUG;
    case ConsoleController::SeverityFilter::All:
    default:
      return true;
  }
}

bool matches_source_filter(const ConsoleEntry &entry, bool show_log_entries,
                           bool show_command_entries, bool show_output_entries) {
  switch (entry.source) {
    case ConsoleEntrySource::Logger:
      return show_log_entries;
    case ConsoleEntrySource::Command:
      return show_command_entries;
    case ConsoleEntrySource::Output:
      return show_output_entries;
    default:
      return true;
  }
}

NodeSpec build_console_row_spec(ConsoleRowNodes &row_nodes, size_t index) {
  const std::string index_suffix = std::to_string(index);
  return column("console_row_" + index_suffix)
      .bind(row_nodes.row)
      .style(padding(8.0f), gap(4.0f), radius(10.0f),
             border(1.0f, rgba(0.3f, 0.43f, 0.58f, 0.2f)), items_start(),
             background(rgba(0.05f, 0.09f, 0.15f, 0.78f)))
      .visible(false)
      .children(text({}, "console_row_primary_" + index_suffix)
                    .bind(row_nodes.primary)
                    .style(font_size(15.0f),
                           text_color(rgba(0.86f, 0.93f, 1.0f, 1.0f))),
                text({}, "console_row_secondary_" + index_suffix)
                    .bind(row_nodes.secondary)
                    .style(font_size(13.0f),
                           text_color(rgba(0.63f, 0.73f, 0.84f, 0.92f)))
                    .visible(false));
}

} // namespace

void ConsoleController::init(Nodes nodes) {
  m_document = nodes.document;
  m_root_node = nodes.root;
  m_filters_row_node = nodes.filters_row;
  m_severity_node = nodes.severity;
  m_source_filters_node = nodes.source_filters;
  m_log_scroll_node = nodes.log_scroll;
  m_input_node = nodes.input;
}

void ConsoleController::reset() {
  m_entries_version = 0u;
  m_force_follow_on_next_refresh = false;
  m_history_draft.clear();
  m_history_index.reset();
  m_rows.clear();

  if (m_document != nullptr) {
    m_document->set_visible(m_root_node, ConsoleManager::get().is_open());
    m_document->set_visible(m_filters_row_node, m_filters_expanded);
    m_document->set_segmented_selected_index(m_severity_node,
                                             severity_filter_index());
    m_document->set_chip_selected(m_source_filters_node, 0u, m_show_log_entries);
    m_document->set_chip_selected(m_source_filters_node, 1u,
                                  m_show_command_entries);
    m_document->set_chip_selected(m_source_filters_node, 2u,
                                  m_show_output_entries);
    m_document->set_text(m_input_node, m_input_value);
    m_document->set_caret(m_input_node, m_input_value.size(),
                          ConsoleManager::get().is_open());
    if (ConsoleManager::get().is_open()) {
      m_document->request_focus(m_input_node);
    }
  }

  refresh(true);
}

void ConsoleController::set_open(bool open) {
  auto &console = ConsoleManager::get();
  if (console.is_open() == open) {
    return;
  }

  console.set_open(open);
  if (auto window = window_manager()->active_window(); window != nullptr) {
    window->capture_cursor(!open);
  }

  if (m_document != nullptr) {
    m_document->set_visible(m_root_node, open);
    if (open) {
      m_document->suppress_next_character_input(static_cast<uint32_t>('`'));
      m_document->request_focus(m_input_node);
      m_document->set_caret(m_input_node, m_input_value.size(), true);
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

void ConsoleController::update() {
  if (m_document == nullptr || m_log_scroll_node == ui::k_invalid_node_id) {
    return;
  }

  refresh();

  if (m_force_follow_on_next_refresh && m_follow_tail) {
    scroll_to_bottom();
  }

  m_force_follow_on_next_refresh = false;
}

void ConsoleController::set_input_value(std::string value) {
  m_input_value = std::move(value);

  if (m_history_index.has_value()) {
    m_history_draft = m_input_value;
    m_history_index.reset();
  }
}

void ConsoleController::submit_command(std::string value) {
  m_input_value = std::move(value);

  auto result = ConsoleManager::get().execute(m_input_value);
  if (!result.executed) {
    return;
  }

  m_history_index.reset();
  m_history_draft.clear();
  m_force_follow_on_next_refresh = true;
  set_input_text({});
}

void ConsoleController::handle_input_key(const ui::UIKeyInputEvent &event) {
  if (event.repeat) {
    return;
  }

  switch (event.key_code) {
    case input::KeyCode::Up:
      navigate_history(-1);
      break;
    case input::KeyCode::Down:
      navigate_history(1);
      break;
    default:
      break;
  }
}

void ConsoleController::clear_entries() {
  ConsoleManager::get().clear_entries();
  m_entries_version = 0u;
  m_force_follow_on_next_refresh = true;
}

void ConsoleController::set_follow_tail(bool follow_tail) {
  if (m_follow_tail == follow_tail) {
    return;
  }

  m_follow_tail = follow_tail;
  if (m_follow_tail) {
    m_force_follow_on_next_refresh = true;
    scroll_to_bottom();
  }
}

void ConsoleController::set_show_details(bool show_details) {
  if (m_show_details == show_details) {
    return;
  }

  m_show_details = show_details;
  refresh(true);
}

void ConsoleController::set_density(float density) {
  const float clamped_density = clamp_density(density);
  if (m_density == clamped_density) {
    return;
  }

  m_density = clamped_density;
  refresh(true);
}

size_t ConsoleController::severity_filter_index() const {
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

void ConsoleController::set_severity_filter_index(size_t index) {
  const SeverityFilter next_filter = severity_filter_from_index(index);
  if (m_severity_filter == next_filter) {
    return;
  }

  m_severity_filter = next_filter;
  if (m_document != nullptr) {
    m_document->set_segmented_selected_index(m_severity_node,
                                             severity_filter_index());
  }
  refresh(true);
}

void ConsoleController::toggle_filters_expanded() {
  m_filters_expanded = !m_filters_expanded;
  if (m_document != nullptr) {
    m_document->set_visible(m_filters_row_node, m_filters_expanded);
  }
}

bool ConsoleController::source_filter_enabled(size_t index) const {
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

void ConsoleController::set_source_filter_enabled(size_t index, bool enabled) {
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

void ConsoleController::refresh(bool force) {
  if (m_document == nullptr || m_log_scroll_node == ui::k_invalid_node_id) {
    return;
  }

  auto &console = ConsoleManager::get();
  const uint64_t next_entries_version = console.entries_version();
  const bool entries_changed = m_entries_version != next_entries_version;
  if (!force && !entries_changed) {
    return;
  }

  const auto &entries = console.entries();
  std::vector<const ConsoleEntry *> visible_entries;
  visible_entries.reserve(entries.size());
  for (const ConsoleEntry &entry : entries) {
    if (matches_severity_filter(entry, m_severity_filter) &&
        matches_source_filter(entry, m_show_log_entries, m_show_command_entries,
                              m_show_output_entries)) {
      visible_entries.push_back(&entry);
    }
  }

  const ConsoleDensityMetrics metrics = density_metrics(m_density);
  ensure_row_capacity(visible_entries.size());

  for (size_t index = 0u; index < m_rows.size(); ++index) {
    const auto &row_nodes = m_rows[index];
    const bool visible = index < visible_entries.size();
    m_document->set_visible(row_nodes.row, visible);

    if (!visible) {
      continue;
    }

    const ConsoleEntry &entry = *visible_entries[index];
    const std::string primary_text = primary_text_for_entry(entry);
    const std::string secondary_text = secondary_text_for_entry(entry);
    const bool show_secondary = m_show_details && !secondary_text.empty();

    m_document->set_text(row_nodes.primary, primary_text);
    m_document->set_text(row_nodes.secondary, secondary_text);
    m_document->set_visible(row_nodes.secondary, show_secondary);

    const glm::vec4 background = background_color_for_entry(entry);
    const glm::vec4 primary_color = primary_color_for_entry(entry);
    m_document->mutate_style(
        row_nodes.row, [background, metrics](ui::UIStyle &style) {
          style.background_color = background;
          style.border_color = glm::vec4(0.3f, 0.43f, 0.58f, 0.2f);
          style.padding = ui::UIEdges::all(metrics.row_padding);
          style.gap = metrics.row_gap;
        });
    m_document->mutate_style(row_nodes.primary,
                             [primary_color, metrics](ui::UIStyle &style) {
                               style.text_color = primary_color;
                               style.font_size = metrics.primary_font_size;
                             });
    m_document->mutate_style(row_nodes.secondary,
                             [metrics](ui::UIStyle &style) {
                               style.font_size = metrics.secondary_font_size;
                             });
  }

  m_entries_version = next_entries_version;
  if (entries_changed && m_follow_tail) {
    m_force_follow_on_next_refresh = true;
  }
}

void ConsoleController::ensure_row_capacity(size_t count) {
  if (m_document == nullptr) {
    return;
  }

  while (m_rows.size() < count) {
    const size_t index = m_rows.size();
    ConsoleRowNodes row_nodes;
    append(*m_document, m_log_scroll_node,
           build_console_row_spec(row_nodes, index));
    m_rows.push_back(row_nodes);
  }
}

void ConsoleController::scroll_to_bottom() {
  if (m_document == nullptr || m_log_scroll_node == ui::k_invalid_node_id) {
    return;
  }

  const auto *scroll_state = m_document->scroll_state(m_log_scroll_node);
  const float current_x =
      scroll_state != nullptr ? scroll_state->offset.x : 0.0f;
  m_document->set_scroll_offset(
      m_log_scroll_node,
      glm::vec2(current_x, std::numeric_limits<float>::max()));
}

void ConsoleController::set_input_text(const std::string &value) {
  m_input_value = value;

  if (m_document == nullptr || m_input_node == ui::k_invalid_node_id) {
    return;
  }

  m_document->set_text(m_input_node, value);
  m_document->set_text_selection(
      m_input_node,
      ui::UITextSelection{.anchor = value.size(), .focus = value.size()});
  m_document->set_caret(m_input_node, value.size(), true);
  m_document->request_focus(m_input_node);
}

void ConsoleController::navigate_history(int direction) {
  const auto &history = ConsoleManager::get().history();
  if (history.empty()) {
    return;
  }

  if (direction < 0) {
    if (!m_history_index.has_value()) {
      m_history_draft = m_input_value;
      m_history_index = history.size() - 1u;
    } else if (*m_history_index > 0u) {
      --(*m_history_index);
    }

    set_input_text(history[*m_history_index]);
    return;
  }

  if (!m_history_index.has_value()) {
    return;
  }

  if (*m_history_index + 1u < history.size()) {
    ++(*m_history_index);
    set_input_text(history[*m_history_index]);
    return;
  }

  m_history_index.reset();
  set_input_text(m_history_draft);
}
