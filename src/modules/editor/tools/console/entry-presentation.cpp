#include "entry-presentation.hpp"
#include "math.hpp"

#include <sstream>

namespace astralix::editor::console_panel {

glm::vec4 alpha(const glm::vec4 &color, float value) {
  return glm::vec4(color.r, color.g, color.b, value);
}

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

std::string badge_text_for_entry(const ConsoleEntry &entry) {
  std::string label;
  switch (entry.source) {
    case ConsoleEntrySource::Command:
      label = "CMD";
      break;
    case ConsoleEntrySource::Output:
      switch (entry.level) {
        case LogLevel::ERROR:
          label = "ERR";
          break;
        case LogLevel::WARNING:
          label = "WARN";
          break;
        case LogLevel::DEBUG:
          label = "DBG";
          break;
        case LogLevel::INFO:
        default:
          label = "OUT";
          break;
      }
      break;
    case ConsoleEntrySource::Logger:
    default:
      label = log_level_label(entry.level);
      break;
  }

  if (entry.repeat_count > 1u) {
    label += " x" + std::to_string(entry.repeat_count);
  }

  return label;
}

std::string meta_text_for_entry(const ConsoleEntry &entry) {
  if (!entry.timestamp.empty()) {
    return entry.timestamp;
  }

  switch (entry.source) {
    case ConsoleEntrySource::Command:
      return "COMMAND";
    case ConsoleEntrySource::Output:
      return "OUTPUT";
    case ConsoleEntrySource::Logger:
    default:
      return "LOGGER";
  }
}

glm::vec4 primary_color_for_entry(const ConsoleEntry &entry) {
  switch (entry.source) {
    case ConsoleEntrySource::Command:
    case ConsoleEntrySource::Output:
      return k_theme.bunker_50;
    case ConsoleEntrySource::Logger:
    default:
      switch (entry.level) {
        case LogLevel::DEBUG:
          return k_theme.bunker_100;
        case LogLevel::WARNING:
        case LogLevel::ERROR:
        case LogLevel::INFO:
        default:
          return k_theme.bunker_50;
      }
  }
}

glm::vec4 background_color_for_entry(const ConsoleEntry &entry) {
  switch (entry.source) {
    case ConsoleEntrySource::Command:
      return alpha(k_theme.sunset_950, 0.88f);
    case ConsoleEntrySource::Output:
      return entry.level == LogLevel::ERROR ? alpha(k_theme.cabaret_950, 0.94f)
                                            : alpha(k_theme.bunker_950, 0.98f);
    case ConsoleEntrySource::Logger:
    default:
      switch (entry.level) {
        case LogLevel::WARNING:
          return alpha(k_theme.sunset_950, 0.72f);
        case LogLevel::ERROR:
          return alpha(k_theme.cabaret_950, 0.94f);
        case LogLevel::DEBUG:
        case LogLevel::INFO:
        default:
          return alpha(k_theme.bunker_950, 0.98f);
      }
  }
}

glm::vec4 border_color_for_entry(const ConsoleEntry &entry) {
  switch (entry.source) {
    case ConsoleEntrySource::Command:
      return alpha(k_theme.sunset_700, 0.94f);
    case ConsoleEntrySource::Output:
      return entry.level == LogLevel::ERROR ? alpha(k_theme.cabaret_600, 0.92f)
                                            : alpha(k_theme.bunker_800, 1.0f);
    case ConsoleEntrySource::Logger:
    default:
      switch (entry.level) {
        case LogLevel::WARNING:
          return alpha(k_theme.sunset_600, 0.82f);
        case LogLevel::ERROR:
          return alpha(k_theme.cabaret_600, 0.84f);
        case LogLevel::DEBUG:
          return alpha(k_theme.bunker_700, 0.84f);
        case LogLevel::INFO:
        default:
          return alpha(k_theme.bunker_800, 1.0f);
      }
  }
}

glm::vec4 badge_background_for_entry(const ConsoleEntry &entry) {
  switch (entry.source) {
    case ConsoleEntrySource::Command:
      return alpha(k_theme.sunset_900, 0.94f);
    case ConsoleEntrySource::Output:
      return entry.level == LogLevel::ERROR ? alpha(k_theme.cabaret_700, 0.94f)
                                            : alpha(k_theme.bunker_800, 0.94f);
    case ConsoleEntrySource::Logger:
    default:
      switch (entry.level) {
        case LogLevel::WARNING:
          return alpha(k_theme.sunset_900, 0.94f);
        case LogLevel::ERROR:
          return alpha(k_theme.cabaret_700, 0.94f);
        case LogLevel::DEBUG:
          return alpha(k_theme.bunker_700, 0.94f);
        case LogLevel::INFO:
        default:
          return alpha(k_theme.bunker_800, 0.94f);
      }
  }
}

glm::vec4 badge_text_color_for_entry(const ConsoleEntry &entry) {
  switch (entry.source) {
    case ConsoleEntrySource::Command:
      return k_theme.sunset_200;
    case ConsoleEntrySource::Output:
      return entry.level == LogLevel::ERROR ? k_theme.cabaret_300
                                            : k_theme.bunker_100;
    case ConsoleEntrySource::Logger:
    default:
      switch (entry.level) {
        case LogLevel::WARNING:
          return k_theme.sunset_200;
        case LogLevel::ERROR:
          return k_theme.cabaret_300;
        case LogLevel::DEBUG:
        case LogLevel::INFO:
        default:
          return k_theme.bunker_100;
      }
  }
}

glm::vec4 meta_color_for_entry(const ConsoleEntry &entry) {
  return entry.source == ConsoleEntrySource::Command
             ? alpha(k_theme.sunset_300, 0.90f)
             : k_theme.bunker_300;
}

glm::vec4 secondary_background_for_entry(const ConsoleEntry &entry) {
  switch (entry.source) {
    case ConsoleEntrySource::Command:
      return alpha(k_theme.bunker_1000, 0.96f);
    case ConsoleEntrySource::Output:
      return entry.level == LogLevel::ERROR ? alpha(k_theme.cabaret_950, 0.84f)
                                            : alpha(k_theme.bunker_1000, 0.94f);
    case ConsoleEntrySource::Logger:
    default:
      switch (entry.level) {
        case LogLevel::WARNING:
          return alpha(k_theme.sunset_950, 0.82f);
        case LogLevel::ERROR:
          return alpha(k_theme.cabaret_950, 0.84f);
        case LogLevel::DEBUG:
        case LogLevel::INFO:
        default:
          return alpha(k_theme.bunker_1000, 0.94f);
      }
  }
}

glm::vec4 secondary_border_for_entry(const ConsoleEntry &entry) {
  return border_color_for_entry(entry) * glm::vec4(1.0f, 1.0f, 1.0f, 0.65f);
}

std::string primary_text_for_entry(const ConsoleEntry &entry) { return entry.message; }

std::string secondary_text_for_entry(const ConsoleEntry &entry) {
  if (entry.source != ConsoleEntrySource::Logger || entry.level == LogLevel::INFO ||
      entry.file.empty()) {
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

ConsoleDensityStyleContraints density_style_constraints(float density) {
  const float clamped_density = saturate(density);
  return ConsoleDensityStyleContraints{
      .row_padding_x = lerp(12.0f, 16.0f, clamped_density),
      .row_padding_y = lerp(7.0f, 10.0f, clamped_density),
      .row_gap = lerp(6.0f, 9.0f, clamped_density),
      .inline_gap = lerp(7.0f, 10.0f, clamped_density),
      .meta_font_size = lerp(10.0f, 11.5f, clamped_density),
      .badge_font_size = lerp(10.0f, 11.5f, clamped_density),
      .primary_font_size = lerp(12.5f, 18.5f, clamped_density),
      .secondary_font_size = lerp(11.0f, 16.75f, clamped_density),
      .badge_padding_x = lerp(6.0f, 8.0f, clamped_density),
      .badge_padding_y = lerp(3.0f, 4.0f, clamped_density),
      .secondary_padding_x = lerp(9.0f, 11.0f, clamped_density),
      .secondary_padding_y = lerp(7.0f, 9.0f, clamped_density),
  };
}

glm::vec4 severity_accent_color(ConsolePanelController::SeverityFilter filter) {
  switch (filter) {
    case ConsolePanelController::SeverityFilter::Info:
      return k_theme.bunker_300;
    case ConsolePanelController::SeverityFilter::Warning:
      return k_theme.sunset_500;
    case ConsolePanelController::SeverityFilter::Error:
      return k_theme.cabaret_500;
    case ConsolePanelController::SeverityFilter::Debug:
      return k_theme.bunker_500;
    case ConsolePanelController::SeverityFilter::All:
    default:
      return k_theme.sunset_500;
  }
}

ConsolePanelController::SeverityFilter severity_filter_from_index(size_t index) {
  switch (index) {
    case 1u:
      return ConsolePanelController::SeverityFilter::Info;
    case 2u:
      return ConsolePanelController::SeverityFilter::Warning;
    case 3u:
      return ConsolePanelController::SeverityFilter::Error;
    case 4u:
      return ConsolePanelController::SeverityFilter::Debug;
    case 0u:
    default:
      return ConsolePanelController::SeverityFilter::All;
  }
}

bool matches_severity_filter(
    const ConsoleEntry &entry,
    ConsolePanelController::SeverityFilter filter
) {
  switch (filter) {
    case ConsolePanelController::SeverityFilter::Info:
      return entry.level == LogLevel::INFO;
    case ConsolePanelController::SeverityFilter::Warning:
      return entry.level == LogLevel::WARNING;
    case ConsolePanelController::SeverityFilter::Error:
      return entry.level == LogLevel::ERROR;
    case ConsolePanelController::SeverityFilter::Debug:
      return entry.level == LogLevel::DEBUG;
    case ConsolePanelController::SeverityFilter::All:
    default:
      return true;
  }
}

bool matches_source_filter(
    const ConsoleEntry &entry,
    bool show_log_entries,
    bool show_command_entries,
    bool show_output_entries
) {
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

std::string disclosure_indicator(bool expandable, bool expanded) {
  if (!expandable) {
    return {};
  }

  return expanded ? "v" : ">";
}

} // namespace astralix::editor::console_panel
