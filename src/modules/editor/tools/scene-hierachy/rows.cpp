#include "tools/scene-hierachy/scene-hierarchy-panel-controller.hpp"

#include "editor-theme.hpp"
#include "tools/scene-hierachy/helpers.hpp"

#include <algorithm>

namespace astralix::editor {

ui::UINodeId SceneHierarchyPanelController::create_row_slot(size_t slot_index) {
  if (m_document == nullptr) {
    return ui::k_invalid_node_id;
  }

  const SceneHierarchyPanelTheme theme;
  const std::string suffix = std::to_string(slot_index);

  RowNodes nodes;
  nodes.slot = m_document->create_view("scene_hierarchy_slot_" + suffix);
  nodes.button =
      m_document->create_pressable("scene_hierarchy_button_" + suffix);
  const ui::UINodeId icon_shell =
      m_document->create_view("scene_hierarchy_icon_shell_" + suffix);
  const ui::UINodeId icon =
      m_document->create_image("icons::cube", "scene_hierarchy_icon_" + suffix);
  const ui::UINodeId body =
      m_document->create_view("scene_hierarchy_body_" + suffix);
  const ui::UINodeId header =
      m_document->create_view("scene_hierarchy_header_" + suffix);
  const ui::UINodeId title_spacer =
      m_document->create_view("scene_hierarchy_spacer_" + suffix);
  const ui::UINodeId meta_row =
      m_document->create_view("scene_hierarchy_meta_row_" + suffix);
  nodes.title = m_document->create_text({}, "scene_hierarchy_title_" + suffix);
  nodes.status_badge =
      m_document->create_view("scene_hierarchy_status_" + suffix);
  nodes.status_badge_text =
      m_document->create_text({}, "scene_hierarchy_status_text_" + suffix);
  nodes.kind_badge =
      m_document->create_view("scene_hierarchy_kind_" + suffix);
  nodes.kind_badge_text =
      m_document->create_text({}, "scene_hierarchy_kind_text_" + suffix);
  nodes.meta = m_document->create_text({}, "scene_hierarchy_meta_" + suffix);

  m_document->append_child(nodes.slot, nodes.button);
  m_document->append_child(nodes.button, icon_shell);
  m_document->append_child(icon_shell, icon);
  m_document->append_child(nodes.button, body);
  m_document->append_child(body, header);
  m_document->append_child(body, meta_row);
  m_document->append_child(header, nodes.title);
  m_document->append_child(header, title_spacer);
  m_document->append_child(header, nodes.status_badge);
  m_document->append_child(nodes.status_badge, nodes.status_badge_text);
  m_document->append_child(meta_row, nodes.kind_badge);
  m_document->append_child(nodes.kind_badge, nodes.kind_badge_text);
  m_document->append_child(meta_row, nodes.meta);

  m_document->mutate_style(nodes.slot, [](ui::UIStyle &style) {
    style.flex_direction = ui::FlexDirection::Column;
    style.align_items = ui::AlignItems::Stretch;
    style.justify_content = ui::JustifyContent::Start;
    style.flex_shrink = 0.0f;
  });
  m_document->mutate_style(nodes.button, [](ui::UIStyle &style) {
    style.flex_direction = ui::FlexDirection::Row;
    style.align_items = ui::AlignItems::Center;
    style.justify_content = ui::JustifyContent::Start;
    style.width = ui::UILength::percent(1.0f);
    style.padding = ui::UIEdges::symmetric(12.0f, 10.0f);
    style.gap = 12.0f;
    style.border_width = 1.0f;
    style.border_radius = 14.0f;
    style.cursor = ui::CursorStyle::Pointer;
  });
  m_document->mutate_style(icon_shell, [theme](ui::UIStyle &style) {
    style.width = ui::UILength::pixels(34.0f);
    style.height = ui::UILength::pixels(34.0f);
    style.align_self = ui::AlignSelf::Center;
    style.align_items = ui::AlignItems::Center;
    style.justify_content = ui::JustifyContent::Center;
    style.background_color = theme.panel_background;
    style.border_color = theme.panel_border;
    style.border_width = 1.0f;
    style.border_radius = 10.0f;
    style.flex_shrink = 0.0f;
  });
  m_document->mutate_style(icon, [theme](ui::UIStyle &style) {
    style.width = ui::UILength::pixels(16.0f);
    style.height = ui::UILength::pixels(18.0f);
    style.tint = theme.text_primary;
    style.flex_shrink = 0.0f;
  });
  m_document->mutate_style(body, [](ui::UIStyle &style) {
    style.flex_direction = ui::FlexDirection::Column;
    style.align_items = ui::AlignItems::Stretch;
    style.justify_content = ui::JustifyContent::Center;
    style.flex_grow = 1.0f;
    style.flex_shrink = 1.0f;
    style.gap = 0.0f;
  });
  m_document->mutate_style(header, [](ui::UIStyle &style) {
    style.flex_direction = ui::FlexDirection::Row;
    style.align_items = ui::AlignItems::Center;
    style.justify_content = ui::JustifyContent::Start;
    style.gap = 8.0f;
  });
  m_document->mutate_style(title_spacer, [](ui::UIStyle &style) {
    style.flex_grow = 1.0f;
  });
  m_document->mutate_style(meta_row, [](ui::UIStyle &style) {
    style.flex_direction = ui::FlexDirection::Row;
    style.align_items = ui::AlignItems::Center;
    style.justify_content = ui::JustifyContent::Start;
    style.gap = 8.0f;
  });
  m_document->mutate_style(nodes.title, [this](ui::UIStyle &style) {
    style.font_id = m_default_font_id;
    style.font_size = std::max(14.0f, m_default_font_size * 0.92f);
  });
  m_document->mutate_style(nodes.status_badge, [](ui::UIStyle &style) {
    style.flex_direction = ui::FlexDirection::Row;
    style.align_items = ui::AlignItems::Center;
    style.justify_content = ui::JustifyContent::Center;
    style.padding = ui::UIEdges::symmetric(12.0f, 6.0f);
    style.border_radius = 999.0f;
    style.border_width = 1.0f;
    style.flex_shrink = 0.0f;
  });
  m_document->mutate_style(nodes.status_badge_text, [this](ui::UIStyle &style) {
    style.font_id = m_default_font_id;
    style.font_size = std::max(11.0f, m_default_font_size * 0.70f);
  });
  m_document->mutate_style(nodes.kind_badge, [](ui::UIStyle &style) {
    style.flex_direction = ui::FlexDirection::Row;
    style.align_items = ui::AlignItems::Center;
    style.justify_content = ui::JustifyContent::Center;
    style.padding = ui::UIEdges::symmetric(12.0f, 6.0f);
    style.border_radius = 999.0f;
    style.border_width = 1.0f;
    style.flex_shrink = 0.0f;
  });
  m_document->mutate_style(nodes.kind_badge_text, [this](ui::UIStyle &style) {
    style.font_id = m_default_font_id;
    style.font_size = std::max(11.0f, m_default_font_size * 0.70f);
  });
  m_document->mutate_style(nodes.meta, [this](ui::UIStyle &style) {
    style.font_id = m_default_font_id;
    style.font_size = std::max(11.5f, m_default_font_size * 0.76f);
  });

  m_row_slots.push_back(nodes);
  return nodes.slot;
}

void SceneHierarchyPanelController::bind_row_slot(
    size_t slot_index,
    size_t item_index
) {
  if (m_document == nullptr || slot_index >= m_row_slots.size() ||
      item_index >= m_entities.size()) {
    return;
  }

  const SceneHierarchyPanelTheme theme;
  const RowNodes &nodes = m_row_slots[slot_index];
  const EntityEntry &entry = m_entities[item_index];
  const bool selected = m_selected_entity_id.has_value() &&
                        scene_hierarchy_panel::same_entity(
                            *m_selected_entity_id, entry.id
                        );

  m_document->set_text(nodes.title, entry.name);
  m_document->set_text(
      nodes.status_badge_text, entry.active ? "ACTIVE" : "INACTIVE"
  );
  m_document->set_text(nodes.kind_badge_text, entry.kind_label);
  m_document->set_text(nodes.meta, entry.meta_label);
  m_document->set_on_click(
      nodes.button, [this, entity_id = entry.id]() { select_entity(entity_id); }
  );
  m_document->set_on_secondary_click(
      nodes.button,
      [this, entity_id = entry.id](const ui::UIPointerButtonEvent &event) {
        select_entity(entity_id);
        open_row_menu(entity_id, event.position);
      }
  );

  m_document->mutate_style(
      nodes.button, [selected, theme](ui::UIStyle &style) {
        style.background_color =
            selected ? theme.row_selected_background : theme.row_background;
        style.border_color =
            selected ? theme.row_selected_border : theme.row_border;
        style.hovered_style.background_color =
            selected ? theme.row_selected_background
                     : theme.card_background;
        style.hovered_style.border_color =
            selected ? theme.row_selected_border : theme.card_border;
        style.pressed_style.background_color = selected
                                                   ? theme.row_selected_background
                                                   : theme.panel_background;
        style.focused_style.border_color = theme.accent;
        style.focused_style.border_width = 2.0f;
      }
  );
  m_document->mutate_style(
      nodes.title, [selected, theme](ui::UIStyle &style) {
        style.text_color = selected ? theme.accent : theme.text_primary;
      }
  );
  m_document->mutate_style(
      nodes.status_badge, [active = entry.active, theme](ui::UIStyle &style) {
        style.background_color = active ? theme.success_soft : theme.subdued_soft;
        style.border_color = active ? theme.success : theme.subdued;
      }
  );
  m_document->mutate_style(
      nodes.status_badge_text,
      [active = entry.active, theme](ui::UIStyle &style) {
        style.text_color = active ? theme.success : theme.subdued;
      }
  );
  m_document->mutate_style(
      nodes.kind_badge,
      [scene_backed = entry.scene_backed, theme](ui::UIStyle &style) {
        style.background_color =
            scene_backed ? theme.accent_soft : theme.subdued_soft;
        style.border_color = scene_backed ? theme.accent : theme.subdued;
      }
  );
  m_document->mutate_style(
      nodes.kind_badge_text,
      [scene_backed = entry.scene_backed, theme](ui::UIStyle &style) {
        style.text_color = scene_backed ? theme.accent : theme.text_muted;
      }
  );
  m_document->mutate_style(nodes.meta, [theme](ui::UIStyle &style) {
    style.text_color = theme.text_muted;
  });
}

void SceneHierarchyPanelController::sync_virtual_list(bool force) {
  if (m_virtual_list == nullptr) {
    return;
  }

  m_virtual_list->set_item_count(m_entities.size());
  for (size_t index = 0u; index < m_entities.size(); ++index) {
    m_virtual_list->set_item_height(index, 76.0f);
  }

  m_virtual_list->set_content_width(0.0f);
  m_virtual_list->refresh(force);
}

} // namespace astralix::editor
