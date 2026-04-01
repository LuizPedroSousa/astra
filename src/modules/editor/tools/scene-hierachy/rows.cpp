#include "tools/scene-hierachy/scene-hierarchy-panel-controller.hpp"

#include "editor-theme.hpp"
#include "managers/resource-manager.hpp"
#include "resources/svg.hpp"
#include "resources/texture.hpp"
#include "tools/scene-hierachy/helpers.hpp"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <string_view>

namespace astralix::editor {
namespace {

ResourceDescriptorID resolve_image(
    std::initializer_list<std::string_view> descriptor_ids
) {
  for (const std::string_view descriptor_id : descriptor_ids) {
    if (descriptor_id.empty()) {
      continue;
    }

    const ResourceDescriptorID candidate{descriptor_id};
    if (resource_manager()->get_by_descriptor_id<Texture2D>(candidate) != nullptr ||
        resource_manager()->get_by_descriptor_id<Svg>(candidate) != nullptr) {
      return candidate;
    }
  }

  return {};
}

ResourceDescriptorID resolved_header_icon_texture_id() {
  return resolve_image({"icons::directory", "icons::cube"});
}

ResourceDescriptorID resolved_chevron_texture_id(bool open) {
  return resolve_image({open ? "icons::right_arrow_down" : "icons::right_arrow"});
}

ResourceDescriptorID
resolved_entity_icon_texture_id(
    scene_hierarchy_panel::TypeBucket bucket
) {
  return resolve_image(
      {
          scene_hierarchy_panel::type_bucket_icon(bucket),
          "icons::cube",
      }
  );
}

void style_meta_badge(
    const Ref<ui::UIDocument> &document,
    ui::UINodeId badge_node,
    ui::UINodeId text_node,
    ResourceDescriptorID font_id,
    float font_size,
    const glm::vec4 &background,
    const glm::vec4 &text_color
) {
  const glm::vec4 border_color = glm::vec4(
      text_color.r, text_color.g, text_color.b, text_color.a * 0.38f
  );
  document->mutate_style(badge_node, [background, border_color](ui::UIStyle &style) {
    style.flex_direction = ui::FlexDirection::Row;
    style.align_items = ui::AlignItems::Center;
    style.justify_content = ui::JustifyContent::Center;
    style.padding = ui::UIEdges::symmetric(5.0f, 2.0f);
    style.border_radius = 4.0f;
    style.border_width = 1.0f;
    style.border_color = border_color;
    style.flex_shrink = 1.0f;
    style.overflow = ui::Overflow::Hidden;
    style.background_color = background;
  });
  document->mutate_style(text_node, [font_id, font_size, text_color](ui::UIStyle &style) {
    style.font_id = font_id;
    style.font_size = font_size;
    style.text_color = text_color;
  });
}

} // namespace

ui::UINodeId SceneHierarchyPanelController::create_row_slot(size_t slot_index) {
  static_cast<void>(slot_index);

  if (m_document == nullptr) {
    return ui::k_invalid_node_id;
  }

  const SceneHierarchyPanelTheme theme;

  RowNodes nodes;
  nodes.slot = m_document->create_view();
  nodes.button = m_document->create_pressable();
  nodes.selection_bar = m_document->create_view();
  nodes.guide_scope = m_document->create_view();
  nodes.guide_type = m_document->create_view();
  nodes.guide_branch = m_document->create_view();
  nodes.chevron = m_document->create_image(resolved_chevron_texture_id(false));
  nodes.icon_shell = m_document->create_view();
  nodes.icon = m_document->create_image("icons::cube");
  const ui::UINodeId body = m_document->create_view();
  const ui::UINodeId header = m_document->create_view();
  const ui::UINodeId header_spacer = m_document->create_view();
  nodes.meta_row = m_document->create_view();
  nodes.title = m_document->create_text();
  nodes.count = m_document->create_text();
  nodes.id_badge = m_document->create_view();
  nodes.id_badge_text = m_document->create_text();
  nodes.scope_badge = m_document->create_view();
  nodes.scope_badge_text = m_document->create_text();
  nodes.kind_badge = m_document->create_view();
  nodes.kind_badge_text = m_document->create_text();
  nodes.meta_spacer = m_document->create_view();
  nodes.status_badge = m_document->create_view();
  nodes.status_badge_text = m_document->create_text();

  m_document->append_child(nodes.slot, nodes.button);
  m_document->append_child(nodes.button, nodes.selection_bar);
  m_document->append_child(nodes.button, nodes.guide_scope);
  m_document->append_child(nodes.button, nodes.guide_type);
  m_document->append_child(nodes.button, nodes.guide_branch);
  m_document->append_child(nodes.button, nodes.chevron);
  m_document->append_child(nodes.button, nodes.icon_shell);
  m_document->append_child(nodes.icon_shell, nodes.icon);
  m_document->append_child(nodes.button, body);
  m_document->append_child(body, header);
  m_document->append_child(body, nodes.meta_row);
  m_document->append_child(header, nodes.title);
  m_document->append_child(header, header_spacer);
  m_document->append_child(header, nodes.count);
  m_document->append_child(nodes.meta_row, nodes.id_badge);
  m_document->append_child(nodes.id_badge, nodes.id_badge_text);
  m_document->append_child(nodes.meta_row, nodes.scope_badge);
  m_document->append_child(nodes.scope_badge, nodes.scope_badge_text);
  m_document->append_child(nodes.meta_row, nodes.kind_badge);
  m_document->append_child(nodes.kind_badge, nodes.kind_badge_text);
  m_document->append_child(nodes.meta_row, nodes.meta_spacer);
  m_document->append_child(nodes.meta_row, nodes.status_badge);
  m_document->append_child(nodes.status_badge, nodes.status_badge_text);

  m_document->mutate_style(nodes.slot, [](ui::UIStyle &style) {
    style.flex_direction = ui::FlexDirection::Column;
    style.align_items = ui::AlignItems::Stretch;
    style.justify_content = ui::JustifyContent::Start;
    style.width = ui::UILength::percent(1.0f);
    style.flex_shrink = 0.0f;
    style.overflow = ui::Overflow::Hidden;
  });
  m_document->mutate_style(nodes.button, [](ui::UIStyle &style) {
    style.flex_direction = ui::FlexDirection::Row;
    style.align_items = ui::AlignItems::Center;
    style.justify_content = ui::JustifyContent::Start;
    style.width = ui::UILength::percent(1.0f);
    style.height = ui::UILength::percent(1.0f);
    style.gap = 6.0f;
    style.background_color = glm::vec4(0.0f);
    style.border_color = glm::vec4(0.0f);
    style.border_width = 0.0f;
    style.border_radius = 8.0f;
    style.cursor = ui::CursorStyle::Pointer;
    style.overflow = ui::Overflow::Hidden;
  });
  m_document->mutate_style(
      nodes.selection_bar,
      [theme](ui::UIStyle &style) {
        style.position_type = ui::PositionType::Absolute;
        style.left = ui::UILength::pixels(6.0f);
        style.top = ui::UILength::pixels(5.0f);
        style.bottom = ui::UILength::pixels(5.0f);
        style.width = ui::UILength::pixels(2.0f);
        style.border_radius = 999.0f;
        style.background_color = theme.accent;
      }
  );
  m_document->mutate_style(
      nodes.guide_scope,
      [theme](ui::UIStyle &style) {
        style.position_type = ui::PositionType::Absolute;
        style.left = ui::UILength::pixels(18.0f);
        style.top = ui::UILength::pixels(0.0f);
        style.bottom = ui::UILength::pixels(0.0f);
        style.width = ui::UILength::pixels(1.0f);
        style.background_color = theme_alpha(theme.panel_border, 0.34f);
      }
  );
  m_document->mutate_style(
      nodes.guide_type,
      [theme](ui::UIStyle &style) {
        style.position_type = ui::PositionType::Absolute;
        style.left = ui::UILength::pixels(34.0f);
        style.top = ui::UILength::pixels(0.0f);
        style.bottom = ui::UILength::pixels(0.0f);
        style.width = ui::UILength::pixels(1.0f);
        style.background_color = theme_alpha(theme.panel_border, 0.34f);
      }
  );
  m_document->mutate_style(
      nodes.guide_branch,
      [theme](ui::UIStyle &style) {
        style.position_type = ui::PositionType::Absolute;
        style.left = ui::UILength::pixels(34.0f);
        style.top = ui::UILength::pixels(20.0f);
        style.width = ui::UILength::pixels(10.0f);
        style.height = ui::UILength::pixels(1.0f);
        style.background_color = theme_alpha(theme.panel_border, 0.34f);
      }
  );
  m_document->mutate_style(nodes.chevron, [this, theme](ui::UIStyle &style) {
    style.width = ui::UILength::pixels(12.0f);
    style.height = ui::UILength::pixels(12.0f);
    style.tint = theme.text_muted;
    style.flex_shrink = 0.0f;
  });
  m_document->mutate_style(nodes.icon_shell, [theme](ui::UIStyle &style) {
    style.width = ui::UILength::pixels(16.0f);
    style.height = ui::UILength::pixels(16.0f);
    style.align_self = ui::AlignSelf::Center;
    style.align_items = ui::AlignItems::Center;
    style.justify_content = ui::JustifyContent::Center;
    style.background_color = theme_alpha(theme.panel_background, 0.0f);
    style.border_color = theme_alpha(theme.panel_border, 0.0f);
    style.border_width = 0.0f;
    style.border_radius = 0.0f;
    style.flex_shrink = 0.0f;
  });
  m_document->mutate_style(nodes.icon, [theme](ui::UIStyle &style) {
    style.width = ui::UILength::pixels(14.0f);
    style.height = ui::UILength::pixels(14.0f);
    style.tint = theme.text_primary;
    style.flex_shrink = 0.0f;
  });
  m_document->mutate_style(body, [](ui::UIStyle &style) {
    style.flex_direction = ui::FlexDirection::Column;
    style.align_items = ui::AlignItems::Stretch;
    style.justify_content = ui::JustifyContent::Center;
    style.flex_grow = 1.0f;
    style.flex_shrink = 1.0f;
    style.gap = 1.0f;
    style.overflow = ui::Overflow::Hidden;
  });
  m_document->mutate_style(header, [](ui::UIStyle &style) {
    style.flex_direction = ui::FlexDirection::Row;
    style.align_items = ui::AlignItems::Center;
    style.justify_content = ui::JustifyContent::Start;
    style.gap = 6.0f;
  });
  m_document->mutate_style(header_spacer, [](ui::UIStyle &style) {
    style.flex_grow = 1.0f;
  });
  m_document->mutate_style(nodes.meta_row, [](ui::UIStyle &style) {
    style.flex_direction = ui::FlexDirection::Row;
    style.align_items = ui::AlignItems::Center;
    style.justify_content = ui::JustifyContent::Start;
    style.gap = 4.0f;
    style.overflow = ui::Overflow::Hidden;
  });
  m_document->mutate_style(nodes.title, [this](ui::UIStyle &style) {
    style.font_id = m_default_font_id;
    style.font_size = std::max(12.5f, m_default_font_size * 0.80f);
  });
  m_document->mutate_style(nodes.count, [this](ui::UIStyle &style) {
    style.font_id = m_default_font_id;
    style.font_size = std::max(10.5f, m_default_font_size * 0.68f);
  });

  const float badge_font_size = std::max(10.0f, m_default_font_size * 0.65f);
  style_meta_badge(
      m_document, nodes.id_badge, nodes.id_badge_text, m_default_font_id, badge_font_size, glm::vec4(0.08f, 0.16f, 0.22f, 0.52f), glm::vec4(0.45f, 0.72f, 0.82f, 0.84f)
  );
  style_meta_badge(
      m_document, nodes.scope_badge, nodes.scope_badge_text, m_default_font_id, badge_font_size, theme_alpha(k_theme.sunset_950, 0.38f), theme_alpha(k_theme.sunset_400, 0.82f)
  );
  style_meta_badge(
      m_document, nodes.kind_badge, nodes.kind_badge_text, m_default_font_id, badge_font_size, glm::vec4(0.16f, 0.10f, 0.24f, 0.48f), glm::vec4(0.70f, 0.58f, 0.88f, 0.86f)
  );
  m_document->mutate_style(nodes.meta_spacer, [](ui::UIStyle &style) {
    style.flex_grow = 1.0f;
  });
  m_document->mutate_style(nodes.status_badge, [](ui::UIStyle &style) {
    style.flex_direction = ui::FlexDirection::Row;
    style.align_items = ui::AlignItems::Center;
    style.justify_content = ui::JustifyContent::Center;
    style.padding = ui::UIEdges::symmetric(5.0f, 2.0f);
    style.border_radius = 4.0f;
    style.flex_shrink = 0.0f;
  });
  m_document->mutate_style(nodes.status_badge_text, [this](ui::UIStyle &style) {
    style.font_id = m_default_font_id;
    style.font_size = std::max(9.5f, m_default_font_size * 0.60f);
  });

  m_row_slots.push_back(nodes);
  return nodes.slot;
}

void SceneHierarchyPanelController::bind_row_slot(
    size_t slot_index,
    size_t item_index
) {
  if (m_document == nullptr || slot_index >= m_row_slots.size() ||
      item_index >= m_visible_rows.size()) {
    return;
  }

  const SceneHierarchyPanelTheme theme;
  const RowNodes &nodes = m_row_slots[slot_index];
  const VisibleRow &row = m_visible_rows[item_index];
  const bool is_scope_header = row.kind == VisibleRow::Kind::ScopeHeader;
  const bool is_type_header = row.kind == VisibleRow::Kind::TypeHeader;
  const bool is_entity = row.kind == VisibleRow::Kind::Entity;
  const bool show_entity_meta = is_entity && row.selected;
  const VisibleRow *next_row =
      item_index + 1u < m_visible_rows.size() ? &m_visible_rows[item_index + 1u]
                                              : nullptr;
  const bool next_in_scope =
      next_row != nullptr && next_row->scope_bucket == row.scope_bucket;
  const bool next_in_type =
      next_row != nullptr &&
      next_row->scope_bucket == row.scope_bucket &&
      next_row->type_bucket == row.type_bucket &&
      next_row->kind == VisibleRow::Kind::Entity;
  const float branch_y = std::floor(row.height * 0.5f);
  const float scope_guide_bottom =
      is_entity && next_in_scope ? 0.0f : branch_y;
  const float type_guide_bottom =
      (is_type_header && row.open && next_in_type) ||
              (is_entity && next_in_type)
          ? 0.0f
          : branch_y;

  m_document->set_texture(nodes.chevron, resolved_chevron_texture_id(row.open));
  m_document->set_text(nodes.title, row.title);
  m_document->set_text(nodes.count, row.count_label);
  m_document->set_text(nodes.id_badge_text, row.id_label);
  m_document->set_text(nodes.scope_badge_text, row.scope_label);
  m_document->set_text(nodes.kind_badge_text, row.kind_label);
  m_document->set_text(nodes.status_badge_text, "ACTIVE");

  m_document->set_visible(nodes.chevron, !is_entity);
  m_document->set_visible(nodes.selection_bar, is_entity && row.selected);
  m_document->set_visible(nodes.guide_scope, is_entity);
  m_document->set_visible(nodes.guide_type, is_type_header || is_entity);
  m_document->set_visible(nodes.guide_branch, is_type_header || is_entity);
  m_document->set_visible(nodes.count, !is_entity);
  m_document->set_visible(nodes.meta_row, show_entity_meta);
  m_document->set_visible(nodes.id_badge, show_entity_meta);
  m_document->set_visible(nodes.scope_badge, show_entity_meta);
  m_document->set_visible(nodes.kind_badge, show_entity_meta);
  m_document->set_visible(nodes.meta_spacer, show_entity_meta && row.active);
  m_document->set_visible(nodes.status_badge, show_entity_meta && row.active);

  if (is_entity) {
    const ResourceDescriptorID icon_id =
        resolved_entity_icon_texture_id(row.type_bucket);
    m_document->set_texture(nodes.icon, icon_id);
    m_document->set_visible(nodes.icon_shell, !icon_id.empty());
  } else {
    const ResourceDescriptorID icon_id = resolved_header_icon_texture_id();
    m_document->set_texture(nodes.icon, icon_id);
    m_document->set_visible(nodes.icon_shell, !icon_id.empty());
  }

  if (is_entity) {
    m_document->set_on_click(
        nodes.button,
        [this, entity_id = row.entity_id]() { handle_entity_click(entity_id); }
    );
    m_document->set_on_secondary_click(
        nodes.button,
        [this, entity_id = row.entity_id](const ui::UIPointerButtonEvent &event) {
          select_entity(entity_id);
          open_row_menu(entity_id, event.position);
        }
    );
  } else {
    m_document->set_on_click(
        nodes.button,
        [this, key = row.group_key]() { toggle_group(key); }
    );
    m_document->set_on_secondary_click(nodes.button, {});
  }

  m_document->mutate_style(nodes.button, [is_scope_header, is_type_header, row, theme](ui::UIStyle &style) {
    style.flex_direction = ui::FlexDirection::Row;
    style.align_items = ui::AlignItems::Center;
    style.justify_content = ui::JustifyContent::Start;
    style.width = ui::UILength::percent(1.0f);
    style.height = ui::UILength::percent(1.0f);
    style.gap = 6.0f;
    style.background_color = glm::vec4(0.0f);
    style.border_color = glm::vec4(0.0f);
    style.border_width = 0.0f;
    style.border_radius = 8.0f;
    style.cursor = ui::CursorStyle::Pointer;
    style.overflow = ui::Overflow::Hidden;
    style.focused_style.border_color = theme.accent;
    style.focused_style.border_width = 1.0f;

    if (is_scope_header) {
      style.padding = ui::UIEdges{
          .left = 6.0f,
          .top = 5.0f,
          .right = 18.0f,
          .bottom = 5.0f,
      };
      style.background_color = theme_alpha(theme.panel_background, 0.0f);
      style.border_color = theme_alpha(theme.panel_border, 0.0f);
      style.hovered_style.background_color = theme_alpha(theme.card_background, 0.16f);
      style.hovered_style.border_color = theme_alpha(theme.panel_border, 0.18f);
      style.pressed_style.background_color = theme_alpha(theme.card_background, 0.24f);
      style.pressed_style.border_color = theme_alpha(theme.panel_border, 0.24f);
      return;
    }

    if (is_type_header) {
      style.padding = ui::UIEdges{
          .left = 22.0f,
          .top = 4.0f,
          .right = 18.0f,
          .bottom = 4.0f,
      };
      style.background_color = theme_alpha(theme.panel_background, 0.0f);
      style.border_color = theme_alpha(theme.panel_border, 0.0f);
      style.hovered_style.background_color = theme_alpha(theme.card_background, 0.12f);
      style.hovered_style.border_color = theme_alpha(theme.panel_border, 0.14f);
      style.pressed_style.background_color = theme_alpha(theme.card_background, 0.18f);
      style.pressed_style.border_color = theme_alpha(theme.panel_border, 0.20f);
      return;
    }

    style.padding = ui::UIEdges{
        .left = 38.0f,
        .top = 4.0f,
        .right = 18.0f,
        .bottom = 4.0f,
    };
    style.background_color =
        row.selected ? theme_alpha(theme.row_selected_background, 0.28f)
                     : theme_alpha(theme.panel_background, 0.0f);
    style.border_color =
        row.selected ? theme_alpha(theme.row_selected_border, 0.52f)
                     : theme_alpha(theme.row_border, 0.0f);
    style.border_width = row.selected ? 1.0f : 0.0f;
    style.hovered_style.background_color =
        row.selected ? theme_alpha(theme.row_selected_background, 0.34f)
                     : theme_alpha(theme.card_background, 0.16f);
    style.hovered_style.border_color =
        row.selected ? theme_alpha(theme.row_selected_border, 0.56f)
                     : theme_alpha(theme.card_border, 0.0f);
    style.pressed_style.background_color =
        row.selected ? theme_alpha(theme.row_selected_background, 0.42f)
                     : theme_alpha(theme.card_background, 0.22f);
    style.pressed_style.border_color =
        row.selected ? theme_alpha(theme.row_selected_border, 0.62f)
                     : theme_alpha(theme.card_border, 0.0f);
  });
  m_document->mutate_style(nodes.guide_scope, [scope_guide_bottom, theme](ui::UIStyle &style) {
    style.position_type = ui::PositionType::Absolute;
    style.left = ui::UILength::pixels(18.0f);
    style.top = ui::UILength::pixels(0.0f);
    style.bottom = ui::UILength::pixels(scope_guide_bottom);
    style.width = ui::UILength::pixels(1.0f);
    style.background_color = theme_alpha(theme.panel_border, 0.34f);
  });
  m_document->mutate_style(
      nodes.guide_type,
      [is_type_header, type_guide_bottom, theme](ui::UIStyle &style) {
        style.position_type = ui::PositionType::Absolute;
        style.left = ui::UILength::pixels(is_type_header ? 18.0f : 34.0f);
        style.top = ui::UILength::pixels(0.0f);
        style.bottom = ui::UILength::pixels(type_guide_bottom);
        style.width = ui::UILength::pixels(1.0f);
        style.background_color = theme_alpha(theme.panel_border, 0.34f);
      }
  );
  m_document->mutate_style(
      nodes.guide_branch,
      [is_type_header, branch_y, theme](ui::UIStyle &style) {
        style.position_type = ui::PositionType::Absolute;
        style.left = ui::UILength::pixels(is_type_header ? 18.0f : 34.0f);
        style.top = ui::UILength::pixels(branch_y);
        style.width = ui::UILength::pixels(10.0f);
        style.height = ui::UILength::pixels(1.0f);
        style.background_color = theme_alpha(theme.panel_border, 0.34f);
      }
  );
  m_document->mutate_style(nodes.icon, [is_entity, row, theme](ui::UIStyle &style) {
    style.width = ui::UILength::pixels(14.0f);
    style.height = ui::UILength::pixels(14.0f);
    style.tint = row.selected ? theme.accent
                              : (is_entity ? theme_alpha(theme.text_muted, 0.94f)
                                           : theme.text_muted);
    style.flex_shrink = 0.0f;
  });
  m_document->mutate_style(
      nodes.title,
      [this, is_scope_header, is_type_header, row, theme](ui::UIStyle &style) {
        style.font_id = m_default_font_id;
        style.font_size = is_scope_header
                              ? std::max(13.0f, m_default_font_size * 0.82f)
                              : std::max(12.5f, m_default_font_size * 0.80f);
        if (row.selected) {
          style.text_color = theme.accent;
        } else if (is_type_header) {
          style.text_color = theme_alpha(theme.text_primary, 0.92f);
        } else {
          style.text_color = theme.text_primary;
        }
      }
  );
  m_document->mutate_style(nodes.count, [this, theme](ui::UIStyle &style) {
    style.font_id = m_default_font_id;
    style.font_size = std::max(10.5f, m_default_font_size * 0.68f);
    style.text_color = theme.text_muted;
  });

  const float badge_font_size = std::max(10.0f, m_default_font_size * 0.65f);
  style_meta_badge(
      m_document, nodes.id_badge, nodes.id_badge_text, m_default_font_id, badge_font_size, glm::vec4(0.08f, 0.16f, 0.22f, 0.52f), glm::vec4(0.45f, 0.72f, 0.82f, 0.84f)
  );
  style_meta_badge(
      m_document, nodes.scope_badge, nodes.scope_badge_text, m_default_font_id, badge_font_size, theme_alpha(k_theme.sunset_950, 0.38f), theme_alpha(k_theme.sunset_400, 0.82f)
  );
  style_meta_badge(
      m_document, nodes.kind_badge, nodes.kind_badge_text, m_default_font_id, badge_font_size, glm::vec4(0.16f, 0.10f, 0.24f, 0.48f), glm::vec4(0.70f, 0.58f, 0.88f, 0.86f)
  );
  m_document->mutate_style(nodes.status_badge, [theme](ui::UIStyle &style) {
    style.flex_direction = ui::FlexDirection::Row;
    style.align_items = ui::AlignItems::Center;
    style.justify_content = ui::JustifyContent::Center;
    style.padding = ui::UIEdges::symmetric(5.0f, 2.0f);
    style.border_radius = 4.0f;
    style.flex_shrink = 0.0f;
    style.background_color = theme_alpha(theme.success_soft, 0.90f);
    style.border_color = theme_alpha(theme.success, 0.72f);
    style.border_width = 1.0f;
  });
  m_document->mutate_style(nodes.status_badge_text, [this, theme](ui::UIStyle &style) {
    style.font_id = m_default_font_id;
    style.font_size = std::max(9.5f, m_default_font_size * 0.60f);
    style.text_color = theme.success;
  });
}

void SceneHierarchyPanelController::sync_virtual_list(bool force) {
  if (m_virtual_list == nullptr) {
    return;
  }

  if (force || m_virtual_list_layout_dirty) {
    m_virtual_list->set_item_count(m_visible_rows.size());
    for (size_t index = 0u; index < m_visible_rows.size(); ++index) {
      m_virtual_list->set_item_height(index, m_visible_rows[index].height);
    }

    m_virtual_list->set_content_width(0.0f);
    m_virtual_list_layout_dirty = false;
  }

  m_virtual_list->refresh(force);
}

} // namespace astralix::editor
