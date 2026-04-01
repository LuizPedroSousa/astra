#pragma once

#include "editor-theme.hpp"

#include "console-panel-controller.hpp"

#include "console.hpp"

#include <string>

namespace astralix::editor::console_panel {

struct ConsoleDensityStyleContraints {
  float row_padding_x = 14.0f;
  float row_padding_y = 8.0f;
  float row_gap = 8.0f;
  float inline_gap = 8.0f;
  float meta_font_size = 11.0f;
  float badge_font_size = 10.5f;
  float primary_font_size = 14.0f;
  float secondary_font_size = 12.0f;
  float badge_padding_x = 7.0f;
  float badge_padding_y = 3.0f;
  float secondary_padding_x = 10.0f;
  float secondary_padding_y = 8.0f;
};

inline constexpr float k_row_border_radius = 14.0f;

glm::vec4 alpha(const glm::vec4 &color, float value);

std::string log_level_label(LogLevel level);
std::string badge_text_for_entry(const ConsoleEntry &entry);
std::string meta_text_for_entry(const ConsoleEntry &entry);
glm::vec4 primary_color_for_entry(const ConsoleEntry &entry);
glm::vec4 background_color_for_entry(const ConsoleEntry &entry);
glm::vec4 border_color_for_entry(const ConsoleEntry &entry);
glm::vec4 badge_background_for_entry(const ConsoleEntry &entry);
glm::vec4 badge_text_color_for_entry(const ConsoleEntry &entry);
glm::vec4 meta_color_for_entry(const ConsoleEntry &entry);
glm::vec4 secondary_background_for_entry(const ConsoleEntry &entry);
glm::vec4 secondary_border_for_entry(const ConsoleEntry &entry);
std::string primary_text_for_entry(const ConsoleEntry &entry);
std::string secondary_text_for_entry(const ConsoleEntry &entry);
ResourceDescriptorID disclosure_indicator_texture(bool expandable, bool expanded);

ConsoleDensityStyleContraints density_style_constraints(float density);

glm::vec4 severity_accent_color(ConsolePanelController::SeverityFilter filter);
bool matches_severity_filter(
    const ConsoleEntry &entry,
    bool filter_all,
    bool filter_info,
    bool filter_warning,
    bool filter_error,
    bool filter_debug
);
bool matches_source_filter(
    const ConsoleEntry &entry,
    bool show_log_entries,
    bool show_command_entries,
    bool show_output_entries
);

} // namespace astralix::editor::console_panel
