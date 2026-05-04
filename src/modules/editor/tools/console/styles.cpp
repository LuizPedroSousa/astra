#include "console-panel-controller.hpp"

#include "editor-theme.hpp"
#include "entry-presentation.hpp"
#include "trace.hpp"

#include <functional>
#include <limits>
#include <string>

namespace astralix::editor {
namespace panel = console_panel;

using namespace ui::dsl::styles;

namespace {
inline constexpr float k_console_shell_radius = 16.0f;
inline constexpr float k_console_surface_radius = 18.0f;
inline constexpr float k_console_popover_radius = 12.0f;
inline constexpr float k_console_input_pair_radius = 14.0f;

ui::dsl::StyleBuilder utility_toggle_style(
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

ui::dsl::StyleBuilder filter_chip_style(
    const ConsolePanelTheme &theme,
    const glm::vec4 &accent,
    bool active
) {
  return row()
      .items_center()
      .gap(8.0f)
      .padding_xy(10.0f, 7.0f)
      .radius(999.0f)
      .background(active ? panel::alpha(accent, 0.16f) : theme_alpha(theme.handle, 0.72f))
      .border(
          1.0f,
          active ? panel::alpha(accent, 0.62f)
                 : theme_alpha(theme.panel_border, 0.70f)
      )
      .cursor_pointer()
      .hover(
          state().background(active ? panel::alpha(accent, 0.22f) : theme.handle)
      )
      .pressed(
          state().background(
              active ? panel::alpha(accent, 0.28f) : k_theme.bunker_1000
          )
      )
      .focused(state().border(2.0f, accent));
}

ui::dsl::StyleBuilder filter_popover_style(
    const ConsolePanelTheme &theme,
    float width_value
) {
  return column()
      .items_start()
      .gap(8.0f)
      .width(px(width_value))
      .padding(10.0f)
      .background(theme_alpha(theme.handle, 0.96f))
      .border(1.0f, theme_alpha(theme.panel_border, 0.82f))
      .radius(k_console_popover_radius);
}

ui::dsl::StyleBuilder filter_popup_option_style(
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

ui::dsl::StyleBuilder log_scroll_style(const ConsolePanelTheme &theme) {
  return fill_x()
      .flex(1.0f)
      .padding(14.0f)
      .gap(10.0f)
      .radius(k_console_surface_radius)
      .border(1.0f, theme.panel_border)
      .background(theme.panel_background)
      .scroll_both()
      .scrollbar_auto()
      .scrollbar_thickness(8.0f)
      .scrollbar_track_color(
          glm::vec4(theme.handle.r, theme.handle.g, theme.handle.b, 0.92f)
      )
      .scrollbar_thumb_color(glm::vec4(theme.panel_border.r, theme.panel_border.g, theme.panel_border.b, 0.96f))
      .scrollbar_thumb_hovered_color(
          glm::vec4(theme.accent.r, theme.accent.g, theme.accent.b, 0.78f)
      )
      .scrollbar_thumb_active_color(
          glm::vec4(theme.accent.r, theme.accent.g, theme.accent.b, 0.86f)
      );
}

ui::dsl::StyleBuilder command_dock_style(const ConsolePanelTheme &theme) {
  return fill_x()
      .padding(14.0f)
      .gap(10.0f)
      .radius(k_console_surface_radius)
      .background(theme.panel_background)
      .border(1.0f, theme.panel_border);
}

ui::dsl::StyleBuilder prompt_chip_style(const ConsolePanelTheme &theme) {
  return items_center()
      .justify_center()
      .width(px(42.0f))
      .height(px(46.0f))
      .radius(0.0f)
      .background(theme.prompt_background)
      .border(1.0f, theme.accent_pressed);
}

ui::dsl::StyleBuilder command_input_style(const ConsolePanelTheme &theme) {
  return flex(1.0f)
      .height(px(46.0f))
      .padding_xy(14.0f, 11.0f)
      .radius(0.0f)
      .background(theme.panel_background)
      .border(1.0f, theme.panel_border)
      .text_color(theme.text_primary)
      .hover(state().border(1.0f, theme.accent))
      .focused(state().border(2.0f, theme.accent))
      .font_id(theme.mono_font)
      .placeholder_text_color(theme.placeholder_text)
      .selection_color(
          glm::vec4(theme.accent.r, theme.accent.g, theme.accent.b, 0.22f)
      )
      .caret_color(theme.text_primary);
}

ui::dsl::StyleBuilder command_input_pair_style() {
  return flex(1.0f)
      .items_center()
      .gap(0.0f)
      .radius(k_console_input_pair_radius)
      .overflow_hidden();
}

void render_section_label(
    ui::im::Children &parent,
    std::string_view local_name,
    const char *text_value,
    const glm::vec4 &text_muted
) {
  parent.text(local_name, text_value)
      .style(font_size(11.5f).text_color(text_muted));
}

} // namespace

void ConsolePanelController::render(ui::im::Frame &ui) {
  ASTRA_PROFILE_N("ConsolePanel::render");
  const ConsolePanelTheme theme;
  const bool source_filter_active = !source_filter_is_default();
  const bool severity_filter_active = !severity_filter_is_default();
  const size_t highlighted_index =
      !m_input_suggestions.empty() && m_runtime != nullptr && m_input_widget
          ? std::min(
                m_runtime->combobox_highlighted_index(m_input_widget),
                m_input_suggestions.size() - 1u
            )
          : 0u;

  auto root = ui.column("root").style(
      fill()
          .padding(12.0f)
          .gap(12.0f)
          .radius(k_console_shell_radius)
          .background(theme.panel_background)
  );

  if (auto header = root.row("header")
                        .frozen(m_header_frozen)
                        .style(fill_x().items_center().gap(12.0f))) {
    m_header_frozen = true;
    ASTRA_PROFILE_N("ConsolePanel::header");
    auto hint = header.row("hint").style(
        items_center()
            .padding_xy(12.0f, 8.0f)
            .radius(999.0f)
            .background(theme.handle)
            .border(1.0f, theme.panel_border)
    );
    hint.text(
            "text",
            "Ctrl+R commands | Up-Down history | Enter accept+run"
    )
        .style(font_size(12.5f).text_color(theme.text_muted));
  }

  {
    ASTRA_PROFILE_N("ConsolePanel::filters");
    auto filters = root.row("filters").style(fill_x().items_center().gap(8.0f));
    filters
        .checkbox("follow-tail", "Follow tail", follow_tail())
        .style(utility_toggle_style(theme, 12.0f))
        .on_toggle([this](bool checked) { set_follow_tail(checked); });
    filters
        .checkbox("expand-details", "Expand details", expand_all_details())
        .style(utility_toggle_style(theme, 12.0f))
        .on_toggle([this](bool checked) { set_expand_all_details(checked); });
    filters.spacer("spacer");

    auto source_chip = filters.pressable("source-chip")
                           .on_click([this]() { toggle_source_filter_popover(); })
                           .style(
                               filter_chip_style(
                                   theme, theme.accent, source_filter_active
                               )
                           );
    m_source_chip_widget = source_chip.widget_id();
    source_chip.text("label", "Source")
        .style(font_size(11.0f).text_color(theme.text_muted));
    source_chip.text("summary", source_filter_summary())
        .style(
            font_size(12.5f).text_color(
                source_filter_active ? theme.accent : theme.text_primary
            )
        );
    source_chip.text("chevron", "v")
        .style(font_size(10.5f).text_color(theme.text_muted));

    auto severity_chip = filters.pressable("severity-chip")
                             .on_click([this]() {
                               toggle_severity_filter_popover();
                             })
                             .style(
                                 filter_chip_style(
                                     theme, theme.accent, severity_filter_active
                                 )
                             );
    m_severity_chip_widget = severity_chip.widget_id();
    severity_chip.text("label", "Severity")
        .style(font_size(11.0f).text_color(theme.text_muted));
    severity_chip.text("summary", severity_filter_summary())
        .style(
            font_size(12.5f).text_color(
                severity_filter_active ? theme.accent : theme.text_primary
            )
        );
    severity_chip.text("chevron", "v")
        .style(font_size(10.5f).text_color(theme.text_muted));

    auto density_row = filters.row("density").style(items_center().gap(8.0f));
    density_row.text("label", "Density")
        .style(font_size(11.5f).text_color(theme.text_muted));
    density_row
        .slider("value", density(), 0.0f, 1.0f)
        .step(0.1f)
        .style(
            width(px(132.0f))
                .accent_color(theme.accent)
                .slider_track_thickness(5.0f)
                .slider_thumb_radius(6.0f)
        )
        .on_value_change([this](float value) { set_density(value); });
  }

  {
    ASTRA_PROFILE_N("ConsolePanel::log_entries");
    auto log_region = root.view("log-region").style(log_scroll_style(theme));
    render_visible_entries(log_region);
  }

  {
    ASTRA_PROFILE_N("ConsolePanel::command_dock");
    auto dock = root.column("command-dock").style(command_dock_style(theme));
    auto dock_header = dock.row("header").style(fill_x().items_center());
    dock_header.text("label", "Command")
        .style(font_size(13.0f).text_color(theme.text_primary));
    dock_header.spacer("spacer");

    auto dock_row = dock.row("row").style(fill_x().items_center().gap(0.0f));
    auto input_pair = dock_row.row("input-pair").style(command_input_pair_style());
    auto prompt = input_pair.row("prompt").style(prompt_chip_style(theme));
    prompt.text("label", ">")
        .style(
            font_size(20.0f)
                .text_color(theme.prompt_text)
                .font_id(theme.mono_font)
        );

    auto input = input_pair.combobox(
                               "input",
                               m_input_value,
                               "Run a command or accept a command suggestion"
    )
                     .options(m_input_suggestions)
                     .autocomplete_text(m_input_autocomplete)
                     .combobox_open(m_suggestions_open)
                     .highlighted_index(highlighted_index)
                     .open_on_arrow_keys(false)
                     .style(command_input_style(theme))
                     .on_focus([this]() { set_input_capture(true); })
                     .on_blur([this]() { set_input_capture(false); })
                     .on_change([this](const std::string &value) {
                       set_input_value(value);
                     })
                     .on_select([this](size_t, const std::string &value) {
                       accept_suggestion(value);
                     })
                     .on_submit([this](const std::string &value) {
                       submit_command(value);
                     })
                     .on_key_input([this](const ui::UIKeyInputEvent &event) {
                       if (event.key_code == input::KeyCode::R &&
                           event.modifiers.primary_shortcut()) {
                         if (!event.repeat) {
                           summon_suggestions();
                         }
                         return;
                       }

                       if (event.key_code == input::KeyCode::Up) {
                         navigate_history(-1);
                         return;
                       }

                       if (event.key_code == input::KeyCode::Down) {
                         navigate_history(1);
                       }
                     });
    m_input_widget = input.widget_id();
  }

  {
    ASTRA_PROFILE_N("ConsolePanel::filter_popovers");
    auto source_popover =
        static_cast<ui::im::Children &>(root).popover("source-popover").popover(ui::im::PopoverState{
            .open = m_source_filter_popover_open,
            .anchor_widget_id = m_source_chip_widget,
            .placement = ui::UIPopupPlacement::BottomStart,
            .depth = 0u,
        });
    source_popover.style(filter_popover_style(theme, 192.0f));
    render_section_label(source_popover, "title", "Source", theme.text_muted);
    source_popover
        .checkbox("logs", "Logs", source_filter_enabled(0u))
        .style(filter_popup_option_style(theme, theme.accent))
        .on_toggle([this](bool checked) { set_source_filter_enabled(0u, checked); });
    source_popover
        .checkbox("commands", "Commands", source_filter_enabled(1u))
        .style(filter_popup_option_style(theme, theme.accent))
        .on_toggle([this](bool checked) { set_source_filter_enabled(1u, checked); });
    source_popover
        .checkbox("output", "Output", source_filter_enabled(2u))
        .style(filter_popup_option_style(theme, theme.accent))
        .on_toggle([this](bool checked) { set_source_filter_enabled(2u, checked); });

    auto severity_popover =
        static_cast<ui::im::Children &>(root)
            .popover("severity-popover")
            .popover(ui::im::PopoverState{
                .open = m_severity_filter_popover_open,
                .anchor_widget_id = m_severity_chip_widget,
                .placement = ui::UIPopupPlacement::BottomStart,
                .depth = 0u,
            });
    severity_popover.style(filter_popover_style(theme, 192.0f));
    render_section_label(
        severity_popover, "title", "Severity", theme.text_muted
    );
    severity_popover
        .checkbox("all", "All", severity_filter_enabled(0u))
        .style(
            filter_popup_option_style(
                theme,
                panel::severity_accent_color(SeverityFilter::All)
            )
        )
        .on_toggle([this](bool) { toggle_severity_all(); });
    severity_popover
        .checkbox("info", "Info", severity_filter_enabled(1u))
        .style(
            filter_popup_option_style(
                theme,
                panel::severity_accent_color(SeverityFilter::Info)
            )
        )
        .on_toggle([this](bool) { toggle_severity_option(1u); });
    severity_popover
        .checkbox("warn", "Warn", severity_filter_enabled(2u))
        .style(
            filter_popup_option_style(
                theme,
                panel::severity_accent_color(SeverityFilter::Warning)
            )
        )
        .on_toggle([this](bool) { toggle_severity_option(2u); });
    severity_popover
        .checkbox("error", "Error", severity_filter_enabled(3u))
        .style(
            filter_popup_option_style(
                theme,
                panel::severity_accent_color(SeverityFilter::Error)
            )
        )
        .on_toggle([this](bool) { toggle_severity_option(3u); });
    severity_popover
        .checkbox("debug", "Debug", severity_filter_enabled(4u))
        .style(
            filter_popup_option_style(
                theme,
                panel::severity_accent_color(SeverityFilter::Debug)
            )
        )
        .on_toggle([this](bool) { toggle_severity_option(4u); });
  }

  if ((m_force_scroll_to_bottom_once ||
       (m_force_follow_on_next_refresh && m_follow_tail)) &&
      m_log_scroll_widget) {
    const auto scroll_state = ui.virtual_list_state(m_log_scroll_widget);
    ui.set_scroll_offset(
        m_log_scroll_widget,
        glm::vec2(scroll_state.scroll_offset.x, std::numeric_limits<float>::max())
    );
  }

  if (m_request_input_focus && m_input_widget) {
    ui.request_focus(m_input_widget);
  }

  if (m_request_input_select_to_end && m_input_widget) {
    ui.set_text_selection(
        m_input_widget,
        ui::UITextSelection{
            .anchor = m_input_value.size(),
            .focus = m_input_value.size(),
        }
    );
    ui.set_caret(m_input_widget, m_input_value.size(), true);
  }

  m_force_follow_on_next_refresh = false;
  m_force_scroll_to_bottom_once = false;
  m_request_input_focus = false;
  m_request_input_select_to_end = false;
}

} // namespace astralix::editor
