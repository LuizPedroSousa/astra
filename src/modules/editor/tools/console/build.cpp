#include "console-panel-controller.hpp"

#include "editor-theme.hpp"
#include "entry-presentation.hpp"

namespace astralix::editor {
namespace panel = console_panel;

using namespace ui::dsl;
using namespace ui::dsl::styles;

namespace {
StyleBuilder icon_button_style(const ConsolePanelTheme &theme) {
  return width(px(36.0f))
      .height(px(36.0f))
      .padding(9.0f)
      .radius(12.0f)
      .background(theme.panel_background)
      .border(1.0f, theme.panel_border)
      .hover(state().background(theme.handle))
      .pressed(state().background(k_theme.bunker_1000))
      .focused(state().border(2.0f, theme.accent));
}

StyleBuilder utility_toggle_style(
    const ConsolePanelTheme &theme,
    float radius_value
) {
  return accent_color(theme.accent)
      .control_gap(10.0f)
      .control_indicator_size(16.0f)
      .padding_xy(12.0f, 9.0f)
      .radius(radius_value)
      .background(theme.handle)
      .border(1.0f, theme.panel_border)
      .hover(state().background(theme.panel_background))
      .pressed(state().background(k_theme.bunker_1000))
      .focused(state().border(2.0f, theme.accent));
}

StyleBuilder log_scroll_style(const ConsolePanelTheme &theme) {
  return fill_x()
      .flex(1.0f)
      .padding(14.0f)
      .gap(10.0f)
      .radius(18.0f)
      .border(1.0f, theme.panel_border)
      .background(theme.panel_background)
      .scroll_both()
      .scrollbar_auto()
      .raw([theme](ui::UIStyle &style) {
        style.scrollbar_thickness = 8.0f;
        style.scrollbar_track_color =
            glm::vec4(theme.handle.r, theme.handle.g, theme.handle.b, 0.92f);
        style.scrollbar_thumb_color = glm::vec4(
            theme.panel_border.r,
            theme.panel_border.g,
            theme.panel_border.b,
            0.96f
        );
        style.scrollbar_thumb_hovered_color =
            glm::vec4(theme.accent.r, theme.accent.g, theme.accent.b, 0.78f);
        style.scrollbar_thumb_active_color =
            glm::vec4(theme.accent.r, theme.accent.g, theme.accent.b, 0.86f);
      });
}

StyleBuilder command_dock_style(const ConsolePanelTheme &theme) {
  return fill_x()
      .padding(14.0f)
      .gap(10.0f)
      .radius(18.0f)
      .background(theme.panel_background)
      .border(1.0f, theme.panel_border);
}

StyleBuilder prompt_chip_style(const ConsolePanelTheme &theme) {
  return items_center()
      .justify_center()
      .width(px(42.0f))
      .height(px(46.0f))
      .radius(14.0f)
      .background(theme.prompt_background)
      .border(1.0f, theme.accent_pressed);
}

StyleBuilder command_input_style(const ConsolePanelTheme &theme) {
  return flex(1.0f)
      .height(px(46.0f))
      .padding_xy(14.0f, 11.0f)
      .radius(14.0f)
      .background(theme.panel_background)
      .border(1.0f, theme.panel_border)
      .text_color(theme.text_primary)
      .hover(state().border(1.0f, theme.accent))
      .focused(state().border(2.0f, theme.accent))
      .raw([theme](ui::UIStyle &style) {
        style.font_id = theme.mono_font;
        style.placeholder_text_color = theme.placeholder_text;
        style.selection_color =
            glm::vec4(theme.accent.r, theme.accent.g, theme.accent.b, 0.22f);
        style.caret_color = theme.text_primary;
      });
}

NodeSpec section_label(
    const char *text_value,
    const char *name,
    const glm::vec4 &text_muted
) {
  return text(text_value, name).style(font_size(12.5f).text_color(text_muted));
}

} // namespace

ui::dsl::NodeSpec ConsolePanelController::build() {
  const ConsolePanelTheme theme;

  auto build_filter_card =
      [&](const char *card_name,
          const char *label_name,
          const char *label_text,
          float width_value,
          NodeSpec control) -> NodeSpec {
    return column(card_name)
        .style(
            width(px(width_value))
                .max_width(max_content())
                .gap(8.0f)
                .border(2.0f, theme.panel_border)
                .padding(12.0f)
                .radius(16.0f)
                .items_start()
        )
        .children(
            section_label(label_text, label_name, theme.text_muted),
            std::move(control)
        );
  };

  auto build_header = [&]() -> NodeSpec {
    return row("console_header")
        .style(fill_x().items_center().gap(12.0f))
        .children(
            column("console_header_copy")
                .style(items_start().gap(2.0f))
                .children(
                    text("Console", "console_title")
                        .style(font_size(20.0f).text_color(theme.text_primary)),
                    text(
                        "Operator workbench for logs, commands, and runtime output",
                        "console_subtitle"
                    )
                        .style(font_size(13.0f).text_color(theme.text_muted))
                ),
            spacer("console_spacer"),
            row("console_hint_pill")
                .style(
                    items_center()
                        .padding_xy(12.0f, 8.0f)
                        .radius(999.0f)
                        .background(theme.handle)
                        .border(1.0f, theme.panel_border)
                )
                .children(
                    text(
                        "Ctrl+R commands | Up-Down history | Inline history | "
                        "Enter accept+run | Tab accept",
                        "console_hint"
                    )
                        .style(font_size(12.5f).text_color(theme.text_muted))
                ),
            icon_button(
                "icons::adjust",
                [this]() { toggle_filters_expanded(); },
                "console_filters_toggle"
            )
                .style(icon_button_style(theme))
        );
  };

  auto build_filters = [&]() -> NodeSpec {
    auto source_control =
        chip_group(
            {"Logs", "Commands", "Output"},
            {source_filter_enabled(0u),
             source_filter_enabled(1u),
             source_filter_enabled(2u)},
            "console_sources"
        )
            .bind(m_source_filters_node)
            .style(
                accent_color(theme.accent)
                    .radius(8.0f)
                    .cursor_pointer()
                    .control_gap(6.0f)
            )
            .on_chip_toggle(
                [this](size_t index, const std::string &, bool enabled) {
                  set_source_filter_enabled(index, enabled);
                }
            );

    auto severity_control =
        segmented_control(
            {"All", "Info", "Warn", "Error", "Debug"},
            severity_filter_index(),
            "console_severity"
        )
            .bind(m_severity_node)
            .style(
                cursor_pointer()
                    .radius(8.0f)
            )
            .on_select([this](size_t index, const std::string &) {
              set_severity_filter_index(index);
            });

    auto density_control = slider(density(), 0.0f, 1.0f, "console_density")
                               .step(0.1f)
                               .style(
                                   accent_color(theme.accent)
                                       .slider_track_thickness(7.0f)
                                       .slider_thumb_radius(8.0f)
                                       .padding_xy(6.0f, 8.0f)
                               )
                               .on_value_change([this](float value) {
                                 set_density(value);
                               });

    return column("console_settings")
        .bind(m_filters_row_node)
        .style(fill_x().gap(10.0f))
        .children(
            row("console_utility_row")
                .style(fill_x().items_center().gap(10.0f))
                .children(
                    checkbox(
                        "Follow tail", follow_tail(), "console_follow_tail"
                    )
                        .style(utility_toggle_style(theme, 12.0f))
                        .on_toggle([this](bool checked) {
                          set_follow_tail(checked);
                        }),
                    checkbox(
                        "Expand details",
                        expand_all_details(),
                        "console_expand_all_details"
                    )
                        .style(utility_toggle_style(theme, 12.0f))
                        .on_toggle([this](bool checked) {
                          set_expand_all_details(checked);
                        }),
                    spacer("console_settings_spacer"),
                    text("Workbench filters", "console_settings_meta")
                        .style(font_size(12.5f).text_color(theme.text_muted))
                ),
            row("console_filter_row")
                .style(
                    fill_x()
                        .items_center()
                        .justify_start()
                        .gap(8.0f)
                )
                .children(
                    build_filter_card(
                        "console_source_card",
                        "console_source_label",
                        "Source",
                        340.0f,
                        std::move(source_control)
                    ),
                    build_filter_card(
                        "console_severity_card",
                        "console_severity_label",
                        "Severity",
                        360.0f,
                        std::move(severity_control)
                    ),
                    build_filter_card(
                        "console_density_card",
                        "console_density_label",
                        "Density",
                        280.0f,
                        std::move(density_control)
                    )
                )
        );
  };

  auto build_divider = [&]() -> NodeSpec {
    return column("divider")
        .style(
            fill_x()
                .height(px(2.0f))
                .background(theme.handle * 1.5f)
        );
  };

  auto build_log_view = [&]() -> NodeSpec {
    return scroll_view("console_log_scroll")
        .bind(m_log_scroll_node)
        .style(log_scroll_style(theme));
  };

  auto build_command_dock = [&]() -> NodeSpec {
    return column("console_command_dock")
        .style(command_dock_style(theme))
        .children(
            row("console_command_meta")
                .style(fill_x().items_center())
                .children(
                    text("Command", "console_command_title")
                        .style(font_size(13.0f).text_color(theme.text_primary)),
                    spacer("console_command_meta_spacer")
                ),
            row("console_command_row")
                .style(fill_x().items_center().gap(10.0f))
                .children(
                    row("console_prompt_chip")
                        .style(prompt_chip_style(theme))
                        .children(
                            text(">", "console_prompt_text")
                                .style(
                                    font_size(20.0f)
                                        .text_color(theme.prompt_text)
                                        .raw([theme](ui::UIStyle &style) {
                                          style.font_id = theme.mono_font;
                                        })
                                )
                        ),
                    combobox(
                        {},
                        "Run a command or accept a command suggestion",
                        "console_input"
                    )
                        .bind(m_input_node)
                        .style(command_input_style(theme))
                        .on_change([this](const std::string &value) {
                          set_input_value(value);
                        })
                        .on_select([this](size_t, const std::string &value) {
                          accept_suggestion(value);
                        })
                        .on_submit([this](const std::string &value) {
                          submit_command(value);
                        })
                )
        );
  };

  return column("console_root")
      .bind(m_root_node)
      .style(
          fill()
              .padding(14.0f)
              .gap(14.0f)
              .background(theme.panel_background)
      )
      .children(
          build_header(),
          build_filters(),
          build_divider().bind(m_filters_divider_node),
          build_log_view(),
          build_command_dock()
      );
}

} // namespace astralix::editor
