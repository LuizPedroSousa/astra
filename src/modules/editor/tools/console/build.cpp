#include "console-panel-controller.hpp"

#include "editor-theme.hpp"
#include "entry-presentation.hpp"

#include <functional>
#include <string>

namespace astralix::editor {
namespace panel = console_panel;

using namespace ui::dsl;
using namespace ui::dsl::styles;

namespace {
inline constexpr float k_console_shell_radius = 16.0f;
inline constexpr float k_console_surface_radius = 18.0f;
inline constexpr float k_console_popover_radius = 12.0f;
inline constexpr float k_console_input_pair_radius = 14.0f;

StyleBuilder utility_toggle_style(
    const ConsolePanelTheme &theme,
    float radius_value
) {
  return accent_color(theme.accent)
      .control_gap(8.0f)
      .control_indicator_size(14.0f)
      .padding_xy(10.0f, 8.0f)
      .radius(radius_value)
      .background(theme_alpha(theme.handle, 0.76f))
      .border(1.0f, theme_alpha(theme.panel_border, 0.72f))
      .hover(state().background(theme.handle))
      .pressed(state().background(k_theme.bunker_1000))
      .focused(state().border(2.0f, theme.accent));
}

StyleBuilder filter_chip_style(const ConsolePanelTheme &theme) {
  return StyleBuilder{}
      .flex_row()
      .items_center()
      .gap(8.0f)
      .padding_xy(10.0f, 7.0f)
      .radius(999.0f)
      .background(theme_alpha(theme.handle, 0.72f))
      .border(1.0f, theme_alpha(theme.panel_border, 0.70f))
      .cursor_pointer()
      .hover(state().background(theme.handle))
      .pressed(state().background(k_theme.bunker_1000))
      .focused(state().border(2.0f, theme.accent));
}

StyleBuilder filter_popover_style(
    const ConsolePanelTheme &theme,
    float width_value
) {
  return StyleBuilder{}
      .column()
      .items_start()
      .gap(8.0f)
      .width(px(width_value))
      .padding(10.0f)
      .background(theme_alpha(theme.handle, 0.96f))
      .border(1.0f, theme_alpha(theme.panel_border, 0.82f))
      .radius(k_console_popover_radius);
}

StyleBuilder filter_popup_option_style(
    const ConsolePanelTheme &theme,
    const glm::vec4 &accent
) {
  return fill_x()
      .control_gap(10.0f)
      .control_indicator_size(14.0f)
      .padding_xy(10.0f, 8.0f)
      .radius(10.0f)
      .background(theme_alpha(theme.panel_background, 0.72f))
      .border(1.0f, theme_alpha(theme.panel_border, 0.76f))
      .accent_color(accent)
      .hover(state().background(theme.handle))
      .focused(state().border(2.0f, accent));
}

StyleBuilder log_scroll_style(const ConsolePanelTheme &theme) {
  return fill_x()
      .flex(1.0f)
      .padding(14.0f)
      .gap(10.0f)
      .radius(k_console_surface_radius)
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
      .radius(k_console_surface_radius)
      .background(theme.panel_background)
      .border(1.0f, theme.panel_border);
}

StyleBuilder prompt_chip_style(const ConsolePanelTheme &theme) {
  return items_center()
      .justify_center()
      .width(px(42.0f))
      .height(px(46.0f))
      .radius(0.0f)
      .background(theme.prompt_background)
      .border(1.0f, theme.accent_pressed);
}

StyleBuilder command_input_style(const ConsolePanelTheme &theme) {
  return flex(1.0f)
      .height(px(46.0f))
      .padding_xy(14.0f, 11.0f)
      .radius(0.0f)
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

StyleBuilder command_input_pair_style() {
  return flex(1.0f)
      .items_center()
      .gap(0.0f)
      .radius(k_console_input_pair_radius)
      .overflow_hidden();
}

NodeSpec section_label(
    const char *text_value,
    const glm::vec4 &text_muted
) {
  return text(text_value).style(font_size(11.5f).text_color(text_muted));
}

} // namespace

ui::dsl::NodeSpec ConsolePanelController::build() {
  const ConsolePanelTheme theme;

  auto build_filter_chip =
      [&](const char *label_text,
          const char *summary_text,
          ui::UINodeId &trigger_node,
          ui::UINodeId &summary_node,
          const std::function<void()> &on_click) -> NodeSpec {
    return pressable()
        .bind(trigger_node)
        .style(filter_chip_style(theme))
        .on_click(on_click)
        .children(
            text(label_text)
                .style(font_size(11.0f).text_color(theme.text_muted)),
            text(summary_text)
                .bind(summary_node)
                .style(font_size(12.5f).text_color(theme.text_primary)),
            text("v")
                .style(font_size(10.5f).text_color(theme.text_muted))
        );
  };

  auto build_header = [&]() -> NodeSpec {
    return ui::dsl::row()
        .style(fill_x().items_center().gap(12.0f))
        .children(
            ui::dsl::column()
                .style(items_start().gap(2.0f))
                .children(
                    text("Console")
                        .style(font_size(20.0f).text_color(theme.text_primary)),
                    text("Operator workbench for logs, commands, and runtime output")
                        .style(font_size(13.0f).text_color(theme.text_muted))
                ),
            spacer(),
            ui::dsl::row()
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
                        "Enter accept+run | Tab accept"
                    )
                        .style(font_size(12.5f).text_color(theme.text_muted))
                )
        );
  };

  auto density_control = slider(density(), 0.0f, 1.0f)
                             .bind(m_density_node)
                             .step(0.1f)
                             .style(
                                 width(px(132.0f))
                                     .accent_color(theme.accent)
                                     .slider_track_thickness(5.0f)
                                     .slider_thumb_radius(6.0f)
                             )
                             .on_value_change([this](float value) {
                               set_density(value);
                             });

  auto filter_toolbar = ui::dsl::row()
                            .bind(m_filters_row_node)
                            .style(fill_x().items_center().gap(8.0f))
                            .children(
                                checkbox("Follow tail", follow_tail())
                                    .bind(m_follow_tail_toggle_node)
                                    .style(utility_toggle_style(theme, 12.0f))
                                    .on_toggle([this](bool checked) { set_follow_tail(checked); }),
                                checkbox("Expand details", expand_all_details())
                                    .bind(m_expand_details_toggle_node)
                                    .style(utility_toggle_style(theme, 12.0f))
                                    .on_toggle(
                                        [this](bool checked) { set_expand_all_details(checked); }
                                    ),
                                spacer(),
                                build_filter_chip(
                                    "Source",
                                    "All sources",
                                    m_source_chip_trigger_node,
                                    m_source_chip_summary_node,
                                    [this]() { toggle_source_filter_popover(); }
                                ),
                                build_filter_chip(
                                    "Severity",
                                    "All severities",
                                    m_severity_chip_trigger_node,
                                    m_severity_chip_summary_node,
                                    [this]() { toggle_severity_filter_popover(); }
                                ),
                                ui::dsl::row()
                                    .style(items_center().gap(8.0f))
                                    .children(
                                        text("Density")
                                            .style(font_size(11.5f).text_color(theme.text_muted)),
                                        std::move(density_control)
                                    )
                            );

  auto source_popover = popover()
                            .bind(m_source_popover_node)
                            .style(filter_popover_style(theme, 192.0f))
                            .children(
                                section_label(
                                    "Source", theme.text_muted
                                ),
                                checkbox("Logs", source_filter_enabled(0u))
                                    .bind(m_source_filter_option_nodes[0])
                                    .style(
                                        filter_popup_option_style(
                                            theme, theme.accent
                                        )
                                    )
                                    .on_toggle([this](bool checked) {
                                      set_source_filter_enabled(0u, checked);
                                    }),
                                checkbox("Commands", source_filter_enabled(1u))
                                    .bind(m_source_filter_option_nodes[1])
                                    .style(
                                        filter_popup_option_style(
                                            theme, theme.accent
                                        )
                                    )
                                    .on_toggle([this](bool checked) {
                                      set_source_filter_enabled(1u, checked);
                                    }),
                                checkbox("Output", source_filter_enabled(2u))
                                    .bind(m_source_filter_option_nodes[2])
                                    .style(
                                        filter_popup_option_style(
                                            theme, theme.accent
                                        )
                                    )
                                    .on_toggle([this](bool checked) {
                                      set_source_filter_enabled(2u, checked);
                                    })
                            );

  auto severity_popover = popover()
                              .bind(m_severity_popover_node)
                              .style(filter_popover_style(theme, 192.0f))
                              .children(
                                  section_label(
                                      "Severity", theme.text_muted
                                  ),
                                  checkbox("All", severity_filter_enabled(0u))
                                      .bind(m_severity_filter_option_nodes[0])
                                      .style(
                                          filter_popup_option_style(
                                              theme,
                                              panel::severity_accent_color(
                                                  SeverityFilter::All
                                              )
                                          )
                                      )
                                      .on_toggle([this](bool) {
                                        toggle_severity_all();
                                      }),
                                  checkbox("Info", severity_filter_enabled(1u))
                                      .bind(m_severity_filter_option_nodes[1])
                                      .style(
                                          filter_popup_option_style(
                                              theme,
                                              panel::severity_accent_color(
                                                  SeverityFilter::Info
                                              )
                                          )
                                      )
                                      .on_toggle([this](bool) {
                                        toggle_severity_option(1u);
                                      }),
                                  checkbox("Warn", severity_filter_enabled(2u))
                                      .bind(m_severity_filter_option_nodes[2])
                                      .style(
                                          filter_popup_option_style(
                                              theme,
                                              panel::severity_accent_color(
                                                  SeverityFilter::Warning
                                              )
                                          )
                                      )
                                      .on_toggle([this](bool) {
                                        toggle_severity_option(2u);
                                      }),
                                  checkbox("Error", severity_filter_enabled(3u))
                                      .bind(m_severity_filter_option_nodes[3])
                                      .style(
                                          filter_popup_option_style(
                                              theme,
                                              panel::severity_accent_color(
                                                  SeverityFilter::Error
                                              )
                                          )
                                      )
                                      .on_toggle([this](bool) {
                                        toggle_severity_option(3u);
                                      }),
                                  checkbox("Debug", severity_filter_enabled(4u))
                                      .bind(m_severity_filter_option_nodes[4])
                                      .style(
                                          filter_popup_option_style(
                                              theme,
                                              panel::severity_accent_color(
                                                  SeverityFilter::Debug
                                              )
                                          )
                                      )
                                      .on_toggle([this](bool) {
                                        toggle_severity_option(4u);
                                      })
                              );

  auto build_log_view = [&]() -> NodeSpec {
    return scroll_view()
        .bind(m_log_scroll_node)
        .style(log_scroll_style(theme));
  };

  auto build_command_dock = [&]() -> NodeSpec {
    return ui::dsl::column()
        .style(command_dock_style(theme))
        .children(
            ui::dsl::row()
                .style(fill_x().items_center())
                .children(
                    text("Command")
                        .style(font_size(13.0f).text_color(theme.text_primary)),
                    spacer()
                ),
            ui::dsl::row()
                .style(
                    fill_x()
                        .items_center()
                        .gap(0.0f)
                )
                .children(
                    ui::dsl::row()
                        .style(command_input_pair_style())
                        .children(
                            ui::dsl::row()
                                .style(prompt_chip_style(theme))
                                .children(
                                    text(">")
                                        .style(
                                            font_size(20.0f)
                                                .text_color(theme.prompt_text)
                                                .raw([theme](ui::UIStyle &style) {
                                                  style.font_id =
                                                      theme.mono_font;
                                                })
                                        )
                                ),
                            combobox(
                                {}, "Run a command or accept a command suggestion"
                            )
                                .bind(m_input_node)
                                .style(command_input_style(theme))
                                .on_focus([this]() { set_input_capture(true); })
                                .on_blur([this]() { set_input_capture(false); })
                                .on_change([this](const std::string &value) {
                                  set_input_value(value);
                                })
                                .on_select(
                                    [this](size_t, const std::string &value) {
                                      accept_suggestion(value);
                                    }
                                )
                                .on_submit([this](const std::string &value) {
                                  submit_command(value);
                                })
                        )
                )
        );
  };

  return ui::dsl::column()
      .bind(m_root_node)
      .style(
          fill()
              .padding(12.0f)
              .gap(12.0f)
              .radius(k_console_shell_radius)
              .background(theme.panel_background)
      )
      .children(
          build_header(),
          std::move(filter_toolbar),
          build_log_view(),
          build_command_dock(),
          std::move(source_popover),
          std::move(severity_popover)
      );
}

} // namespace astralix::editor
