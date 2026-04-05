#include "console-panel-controller.hpp"

#include "entry-presentation.hpp"
#include "text-metrics.hpp"

#include <algorithm>
#include <cmath>

namespace astralix::editor {
namespace panel = console_panel;

using namespace ui::dsl::styles;

namespace {

ui::dsl::StyleBuilder row_shell_style(
    const ConsolePanelController::VisibleEntry &entry,
    const panel::ConsoleDensityStyleContraints &style_constraints
) {
  return fill_x()
      .min_width(px(entry.width))
      .padding(
          ui::UIEdges::symmetric(
              style_constraints.row_padding_x, style_constraints.row_padding_y
          )
      )
      .gap(style_constraints.row_gap)
      .radius(panel::k_row_border_radius)
      .background(entry.background)
      .border(1.0f, entry.border);
}

ui::dsl::StyleBuilder row_header_style(
    bool expandable,
    const panel::ConsoleDensityStyleContraints &style_constraints
) {
  auto style =
      row().fill_x().items_center().gap(style_constraints.inline_gap);
  if (expandable) {
    style.cursor_pointer();
  }
  return style;
}

} // namespace

void ConsolePanelController::refresh(bool force) {
  auto &console = ConsoleManager::get();
  const uint64_t next_entries_version = console.entries_version();
  const bool entries_changed = m_entries_version != next_entries_version;
  const bool list_dirty = force || entries_changed;

  if (!list_dirty) {
    return;
  }

  const auto &entries = console.entries();
  if (entries.empty()) {
    m_collapsed_source_indices.clear();
  } else {
    std::erase_if(
        m_collapsed_source_indices,
        [entry_count = entries.size()](size_t source_index) {
          return source_index >= entry_count;
        }
    );
  }

  const panel::ConsoleDensityStyleContraints style_constraints =
      panel::density_style_constraints(m_density);
  const float meta_line_height =
      measure_line_height(style_constraints.meta_font_size);
  const float badge_line_height =
      measure_line_height(style_constraints.badge_font_size) +
      style_constraints.badge_padding_y * 2.0f;
  const float collapsed_line_height =
      measure_line_height(style_constraints.primary_font_size);
  const float expanded_line_height =
      measure_line_height(style_constraints.secondary_font_size) +
      style_constraints.secondary_padding_y * 2.0f;
  const float indicator_width =
      std::ceil(style_constraints.meta_font_size + 1.0f);
  const float collapsed_height = std::max(
                                     {meta_line_height,
                                      badge_line_height,
                                      collapsed_line_height}
                                 ) +
                                 style_constraints.row_padding_y * 2.0f;

  std::vector<VisibleEntry> next_visible_entries;
  next_visible_entries.reserve(entries.size());

  bool expanded_source_still_visible = false;
  float content_width = 0.0f;
  for (size_t source_index = 0u; source_index < entries.size(); ++source_index) {
    const ConsoleEntry &entry = entries[source_index];
    if (!panel::matches_severity_filter(
            entry,
            m_severity_filter_all,
            m_severity_filter_info,
            m_severity_filter_warning,
            m_severity_filter_error,
            m_severity_filter_debug
        ) ||
        !panel::matches_source_filter(
            entry,
            m_show_log_entries,
            m_show_command_entries,
            m_show_output_entries
        )) {
      continue;
    }

    VisibleEntry visible_entry;
    visible_entry.source_index = source_index;
    visible_entry.meta_text = panel::meta_text_for_entry(entry);
    visible_entry.badge_text = panel::badge_text_for_entry(entry);
    visible_entry.primary_text = panel::primary_text_for_entry(entry);
    visible_entry.secondary_text = panel::secondary_text_for_entry(entry);
    visible_entry.background = panel::background_color_for_entry(entry);
    visible_entry.border = panel::border_color_for_entry(entry);
    visible_entry.meta_color = panel::meta_color_for_entry(entry);
    visible_entry.badge_background = panel::badge_background_for_entry(entry);
    visible_entry.badge_text_color = panel::badge_text_color_for_entry(entry);
    visible_entry.primary_color = panel::primary_color_for_entry(entry);
    visible_entry.secondary_background =
        panel::secondary_background_for_entry(entry);
    visible_entry.secondary_border = panel::secondary_border_for_entry(entry);
    visible_entry.secondary_color = k_theme.bunker_300;
    visible_entry.expandable = !visible_entry.secondary_text.empty();
    visible_entry.expanded =
        visible_entry.expandable &&
        ((m_expand_all_details &&
          !m_collapsed_source_indices.contains(source_index)) ||
         (!m_expand_all_details && m_expanded_source_index.has_value() &&
          *m_expanded_source_index == source_index));
    visible_entry.collapsed_height = collapsed_height;
    visible_entry.expanded_height =
        visible_entry.expandable
            ? collapsed_height + style_constraints.row_gap + expanded_line_height
            : collapsed_height;

    const float meta_width =
        measure_text_width(style_constraints.meta_font_size, visible_entry.meta_text);
    const float badge_width =
        measure_text_width(
            style_constraints.badge_font_size, visible_entry.badge_text
        ) +
        style_constraints.badge_padding_x * 2.0f;
    const float primary_width = measure_text_width(
        style_constraints.primary_font_size, visible_entry.primary_text
    );
    const float collapsed_width =
        style_constraints.row_padding_x * 2.0f +
        (visible_entry.expandable ? indicator_width + style_constraints.inline_gap
                                  : 0.0f) +
        (meta_width > 0.0f ? meta_width + style_constraints.inline_gap : 0.0f) +
        badge_width + style_constraints.inline_gap + primary_width;
    const float expanded_width =
        visible_entry.expanded
            ? measure_text_width(
                  style_constraints.secondary_font_size,
                  visible_entry.secondary_text
              ) +
                  style_constraints.secondary_padding_x * 2.0f +
                  style_constraints.row_padding_x * 2.0f
            : 0.0f;
    visible_entry.width = std::max(collapsed_width, expanded_width);
    content_width = std::max(content_width, visible_entry.width);

    if (visible_entry.expandable && m_expanded_source_index.has_value() &&
        *m_expanded_source_index == source_index) {
      expanded_source_still_visible = true;
    }

    next_visible_entries.push_back(std::move(visible_entry));
  }

  if (!m_expand_all_details && m_expanded_source_index.has_value() &&
      !expanded_source_still_visible) {
    m_expanded_source_index.reset();
  }

  m_visible_entries = std::move(next_visible_entries);
  m_visible_entries_content_width = content_width;
  m_entries_version = next_entries_version;
  mark_render_dirty();

  if (entries_changed && m_follow_tail) {
    m_force_follow_on_next_refresh = true;
  }
}

void ConsolePanelController::render_visible_entries(ui::im::Children &parent) {
  const ConsolePanelTheme theme;
  const auto style_constraints = panel::density_style_constraints(m_density);

  auto scroll = parent.virtual_list(
      "log-scroll",
      m_visible_entries.size(),
      [this](size_t index) {
        const auto &entry = m_visible_entries[index];
        return entry.expanded ? entry.expanded_height : entry.collapsed_height;
      },
      [this, &theme, style_constraints](
          ui::im::Children &visible_scope,
          size_t start,
          size_t end
      ) {
        for (size_t index = start; index <= end; ++index) {
          const auto &entry = m_visible_entries[index];
          auto row_scope = visible_scope.item_scope("entry", entry.source_index);
          auto card = row_scope.column("card").style(
              row_shell_style(entry, style_constraints)
          );

          auto header = card.pressable("header")
                            .enabled(entry.expandable)
                            .on_click(
                                entry.expandable
                                    ? [this, source_index = entry.source_index]() {
                                        toggle_row_expanded(source_index);
                                      }
                                    : std::function<void()>{}
                            )
                            .style(
                                row_header_style(entry.expandable, style_constraints)
                            );

          if (entry.expandable) {
            header
                .image(
                    "indicator",
                    panel::disclosure_indicator_texture(
                        entry.expandable, entry.expanded
                    )
                )
                .style(
                    width(px(std::ceil(style_constraints.meta_font_size + 1.0f)))
                        .height(px(std::ceil(style_constraints.meta_font_size + 1.0f)))
                        .shrink(0.0f)
                        .tint(
                            entry.expanded
                                ? k_theme.sunset_500
                                : panel::alpha(k_theme.bunker_300, 0.94f)
                        )
                );
          }

          if (!entry.meta_text.empty()) {
            header.text("meta", entry.meta_text)
                .style(
                    font_size(style_constraints.meta_font_size)
                        .text_color(entry.meta_color)
                        .font_id(m_default_font_id)
                );
          }

          auto badge = header.view("badge").style(
              padding_xy(
                  style_constraints.badge_padding_x,
                  style_constraints.badge_padding_y
              )
                  .radius(999.0f)
                  .background(entry.badge_background)
                  .shrink(0.0f)
          );
          badge.text("label", entry.badge_text)
              .style(
                  font_size(style_constraints.badge_font_size)
                      .text_color(entry.badge_text_color)
                      .font_id(m_default_font_id)
              );

          header.text("primary", entry.primary_text)
              .style(
                  font_size(style_constraints.primary_font_size)
                      .text_color(entry.primary_color)
                      .font_id(m_default_font_id)
              );

          if (entry.expandable && entry.expanded) {
            card.text("secondary", entry.secondary_text)
                .style(
                    font_size(style_constraints.secondary_font_size)
                        .text_color(entry.secondary_color)
                        .padding(
                            ui::UIEdges::symmetric(
                                style_constraints.secondary_padding_x,
                                style_constraints.secondary_padding_y
                            )
                        )
                        .radius(10.0f)
                        .background(entry.secondary_background)
                        .border(1.0f, entry.secondary_border)
                        .font_id(m_default_font_id)
                );
          }
        }
      },
      m_visible_entries_content_width
  );

  scroll.style(fill_x().flex(1.0f).min_height(px(0.0f)));
  m_log_scroll_widget = scroll.widget_id();
}

void ConsolePanelController::toggle_row_expanded(size_t source_index) {
  if (m_expand_all_details) {
    if (m_collapsed_source_indices.contains(source_index)) {
      m_collapsed_source_indices.erase(source_index);
    } else {
      m_collapsed_source_indices.insert(source_index);
    }

    refresh(true);
    return;
  }

  if (m_expanded_source_index.has_value() &&
      *m_expanded_source_index == source_index) {
    m_expanded_source_index.reset();
  } else {
    m_expanded_source_index = source_index;
  }

  refresh(true);
}

float ConsolePanelController::measure_text_width(
    float font_size,
    std::string_view text
) const {
  return ui::measure_ui_text_width(m_default_font_id, font_size, text);
}

float ConsolePanelController::measure_line_height(float font_size) const {
  return ui::measure_ui_line_height(m_default_font_id, font_size);
}

std::string ConsolePanelController::text_metric_cache_key(
    uint32_t pixel_size,
    std::string_view text
) {
  std::string key = std::to_string(pixel_size);
  key.push_back('\n');
  key.append(text);
  return key;
}

} // namespace astralix::editor
