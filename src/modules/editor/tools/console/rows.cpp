#include "console-panel-controller.hpp"

#include "entry-presentation.hpp"

#include "text-metrics.hpp"

#include <algorithm>
#include <cmath>

namespace astralix::editor {
namespace panel = console_panel;

void ConsolePanelController::refresh(bool force) {
  if (m_document == nullptr || m_log_scroll_node == ui::k_invalid_node_id) {
    return;
  }

  auto &console = ConsoleManager::get();
  const uint64_t next_entries_version = console.entries_version();
  const bool entries_changed = m_entries_version != next_entries_version;
  const bool list_dirty = force || entries_changed;

  if (list_dirty) {
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
    for (size_t source_index = 0u; source_index < entries.size();
         ++source_index) {
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
      visible_entry.badge_background =
          panel::badge_background_for_entry(entry);
      visible_entry.badge_text_color =
          panel::badge_text_color_for_entry(entry);
      visible_entry.primary_color = panel::primary_color_for_entry(entry);
      visible_entry.secondary_background =
          panel::secondary_background_for_entry(entry);
      visible_entry.secondary_border =
          panel::secondary_border_for_entry(entry);
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
          (visible_entry.expandable
               ? indicator_width + style_constraints.inline_gap
               : 0.0f) +
          (meta_width > 0.0f ? meta_width + style_constraints.inline_gap : 0.0f) +
          badge_width + style_constraints.inline_gap + primary_width;
      const float expanded_width =
          visible_entry.expanded
              ? measure_text_width(
                    style_constraints.secondary_font_size, visible_entry.secondary_text
                ) +
                    style_constraints.secondary_padding_x * 2.0f +
                    style_constraints.row_padding_x * 2.0f
              : 0.0f;
      visible_entry.width = std::max(collapsed_width, expanded_width);

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
    m_entries_version = next_entries_version;
  }

  sync_virtual_list(list_dirty);

  if (entries_changed && m_follow_tail) {
    m_force_follow_on_next_refresh = true;
  }
}

ui::UINodeId ConsolePanelController::create_row_slot(size_t slot_index) {
  if (m_document == nullptr) {
    return ui::k_invalid_node_id;
  }

  const std::string suffix = std::to_string(slot_index);

  RowNodes row_nodes;
  row_nodes.slot = m_document->create_view();
  m_document->mutate_style(row_nodes.slot, [](ui::UIStyle &style) {
    style.flex_direction = ui::FlexDirection::Column;
    style.align_items = ui::AlignItems::Stretch;
    style.justify_content = ui::JustifyContent::Start;
    style.flex_shrink = 0.0f;
  });

  row_nodes.disclosure = ui::create_disclosure(
      *m_document,
      row_nodes.slot,
      ui::UIDisclosureOptions{
          .open = false,
      }
  );

  m_document->mutate_style(
      row_nodes.disclosure.root, [](ui::UIStyle &style) {
        style.flex_direction = ui::FlexDirection::Column;
        style.align_items = ui::AlignItems::Stretch;
        style.justify_content = ui::JustifyContent::Start;
        style.flex_shrink = 0.0f;
      }
  );
  m_document->mutate_style(
      row_nodes.disclosure.header_button, [](ui::UIStyle &style) {
        style.flex_direction = ui::FlexDirection::Row;
        style.align_items = ui::AlignItems::Center;
        style.justify_content = ui::JustifyContent::Start;
        style.width = ui::UILength::percent(1.0f);
        style.flex_shrink = 0.0f;
        style.hovered_style.background_color = panel::alpha(
            k_theme.bunker_700, 0.18f
        );
        style.hovered_style.border_radius = 10.0f;
        style.pressed_style.background_color = panel::alpha(
            k_theme.bunker_600, 0.24f
        );
        style.pressed_style.border_radius = 10.0f;
      }
  );
  m_document->mutate_style(
      row_nodes.disclosure.header_content, [](ui::UIStyle &style) {
        style.gap = 8.0f;
        style.align_items = ui::AlignItems::Center;
        style.justify_content = ui::JustifyContent::Start;
        style.width = ui::UILength::percent(1.0f);
      }
  );
  m_document->mutate_style(
      row_nodes.disclosure.body, [](ui::UIStyle &style) {
        style.flex_direction = ui::FlexDirection::Column;
        style.align_items = ui::AlignItems::Start;
        style.justify_content = ui::JustifyContent::Start;
        style.width = ui::UILength::percent(1.0f);
      }
  );

  row_nodes.indicator = m_document->create_image("icons::right_arrow");
  row_nodes.meta = m_document->create_text();
  row_nodes.badge = m_document->create_text();
  row_nodes.primary = m_document->create_text();
  row_nodes.secondary = m_document->create_text();

  m_document->append_child(
      row_nodes.disclosure.header_content, row_nodes.indicator
  );
  m_document->append_child(row_nodes.disclosure.header_content, row_nodes.meta);
  m_document->append_child(
      row_nodes.disclosure.header_content, row_nodes.badge
  );
  m_document->append_child(
      row_nodes.disclosure.header_content, row_nodes.primary
  );
  m_document->append_child(row_nodes.disclosure.body, row_nodes.secondary);

  m_document->mutate_style(row_nodes.indicator, [this](ui::UIStyle &style) {
    style.width = ui::UILength::pixels(12.0f);
    style.height = ui::UILength::pixels(12.0f);
    style.tint = panel::alpha(k_theme.bunker_300, 0.92f);
    style.flex_shrink = 0.0f;
  });
  m_document->mutate_style(row_nodes.meta, [this](ui::UIStyle &style) {
    style.font_id = m_default_font_id;
    style.text_color = k_theme.bunker_300;
  });
  m_document->mutate_style(row_nodes.badge, [this](ui::UIStyle &style) {
    style.font_id = m_default_font_id;
    style.text_color = k_theme.bunker_100;
    style.border_radius = 999.0f;
    style.background_color = panel::alpha(k_theme.bunker_800, 0.94f);
  });
  m_document->mutate_style(row_nodes.primary, [this](ui::UIStyle &style) {
    style.font_id = m_default_font_id;
    style.text_color = k_theme.bunker_50;
  });
  m_document->mutate_style(row_nodes.secondary, [this](ui::UIStyle &style) {
    style.font_id = m_default_font_id;
    style.text_color = k_theme.bunker_300;
    style.border_radius = 10.0f;
  });

  m_row_slots.push_back(row_nodes);
  return row_nodes.slot;
}

void ConsolePanelController::bind_row_slot(
    size_t slot_index,
    size_t visible_index
) {
  if (m_document == nullptr || slot_index >= m_row_slots.size() ||
      visible_index >= m_visible_entries.size()) {
    return;
  }

  const panel::ConsoleDensityStyleContraints style_constraints =
      panel::density_style_constraints(m_density);
  const RowNodes &row_nodes = m_row_slots[slot_index];
  const VisibleEntry &entry = m_visible_entries[visible_index];
  const bool expandable = entry.expandable;
  const bool expanded = entry.expandable && entry.expanded;

  m_document->set_texture(
      row_nodes.indicator,
      panel::disclosure_indicator_texture(expandable, expanded)
  );
  m_document->set_text(row_nodes.meta, entry.meta_text);
  m_document->set_text(row_nodes.badge, entry.badge_text);
  m_document->set_text(row_nodes.primary, entry.primary_text);
  m_document->set_text(row_nodes.secondary, entry.secondary_text);
  m_document->set_visible(row_nodes.indicator, expandable);
  m_document->set_visible(row_nodes.meta, !entry.meta_text.empty());
  m_document->set_visible(row_nodes.secondary, expandable);
  m_document->set_enabled(row_nodes.disclosure.header_button, true);
  m_document->set_on_click(
      row_nodes.disclosure.header_button,
      expandable
          ? [this, source_index = entry.source_index]() {
              toggle_row_expanded(source_index);
            }
          : std::function<void()>{}
  );
  ui::set_disclosure_open(*m_document, row_nodes.disclosure, expanded);

  m_document->mutate_style(
      row_nodes.disclosure.root,
      [background = entry.background,
       border = entry.border,
       style_constraints](ui::UIStyle &style) {
        style.background_color = background;
        style.border_color = border;
        style.border_width = 1.0f;
        style.border_radius = panel::k_row_border_radius;
        style.padding =
            ui::UIEdges::symmetric(style_constraints.row_padding_x, style_constraints.row_padding_y);
        style.gap = style_constraints.row_gap;
      }
  );
  m_document->mutate_style(
      row_nodes.disclosure.header_content, [style_constraints](ui::UIStyle &style) {
        style.gap = style_constraints.inline_gap;
      }
  );
  m_document->mutate_style(
      row_nodes.indicator, [style_constraints, expanded](ui::UIStyle &style) {
        const float indicator_size =
            std::ceil(style_constraints.meta_font_size + 1.0f);
        style.width = ui::UILength::pixels(indicator_size);
        style.height = ui::UILength::pixels(indicator_size);
        style.tint =
            expanded ? k_theme.sunset_500
                     : panel::alpha(k_theme.bunker_300, 0.94f);
      }
  );
  m_document->mutate_style(
      row_nodes.meta,
      [meta_color = entry.meta_color, style_constraints](ui::UIStyle &style) {
        style.font_size = style_constraints.meta_font_size;
        style.text_color = meta_color;
      }
  );
  m_document->mutate_style(
      row_nodes.badge,
      [badge_background = entry.badge_background,
       badge_text_color = entry.badge_text_color,
       style_constraints](ui::UIStyle &style) {
        style.font_size = style_constraints.badge_font_size;
        style.text_color = badge_text_color;
        style.background_color = badge_background;
        style.padding = ui::UIEdges::symmetric(
            style_constraints.badge_padding_x, style_constraints.badge_padding_y
        );
      }
  );
  m_document->mutate_style(
      row_nodes.primary,
      [primary_color = entry.primary_color, style_constraints](ui::UIStyle &style) {
        style.text_color = primary_color;
        style.font_size = style_constraints.primary_font_size;
      }
  );
  m_document->mutate_style(
      row_nodes.secondary,
      [secondary_background = entry.secondary_background,
       secondary_border = entry.secondary_border,
       secondary_color = entry.secondary_color,
       style_constraints](ui::UIStyle &style) {
        style.font_size = style_constraints.secondary_font_size;
        style.text_color = secondary_color;
        style.background_color = secondary_background;
        style.border_color = secondary_border;
        style.border_width = 1.0f;
        style.padding = ui::UIEdges::symmetric(
            style_constraints.secondary_padding_x, style_constraints.secondary_padding_y
        );
      }
  );
}

void ConsolePanelController::sync_virtual_list(bool force) {
  if (m_virtual_list == nullptr) {
    return;
  }

  if (force) {
    m_virtual_list->set_item_count(m_visible_entries.size());

    float content_width = 0.0f;
    for (size_t index = 0u; index < m_visible_entries.size(); ++index) {
      const VisibleEntry &entry = m_visible_entries[index];
      const float row_height =
          entry.expanded ? entry.expanded_height : entry.collapsed_height;
      m_virtual_list->set_item_height(index, row_height);
      content_width = std::max(content_width, entry.width);
    }

    m_virtual_list->set_content_width(content_width);
  }

  m_virtual_list->refresh(force);
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
  const uint32_t pixel_size = ui::resolve_ui_font_pixel_size(font_size);
  const std::string cache_key = text_metric_cache_key(pixel_size, text);
  if (const auto it = m_text_width_cache.find(cache_key);
      it != m_text_width_cache.end()) {
    return it->second;
  }

  const float width =
      ui::measure_ui_text_width(m_default_font_id, font_size, text);
  m_text_width_cache.emplace(cache_key, width);
  return width;
}

float ConsolePanelController::measure_line_height(float font_size) const {
  const uint32_t pixel_size = ui::resolve_ui_font_pixel_size(font_size);
  if (const auto it = m_line_height_cache.find(pixel_size);
      it != m_line_height_cache.end()) {
    return it->second;
  }

  const float line_height =
      ui::measure_ui_line_height(m_default_font_id, font_size);
  m_line_height_cache.emplace(pixel_size, line_height);
  return line_height;
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
