#include "tools/scene-hierachy/scene-hierarchy-panel-controller.hpp"

#include "editor-theme.hpp"
#include "managers/resource-manager.hpp"
#include "resources/svg.hpp"
#include "resources/texture.hpp"
#include "tools/scene-hierachy/helpers.hpp"

#include <initializer_list>
#include <string_view>

namespace astralix::editor {
namespace {

using namespace ui::dsl::styles;

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

ui::dsl::StyleBuilder text_style(
    ResourceDescriptorID font_id,
    float size,
    const glm::vec4 &color
) {
  return font_size(size).text_color(color).font_id(std::move(font_id));
}

ui::dsl::StyleBuilder meta_badge_style(
    const glm::vec4 &background,
    const glm::vec4 &border,
    const glm::vec4 &text_color
) {
  return items_center()
      .justify_center()
      .padding_xy(5.0f, 2.0f)
      .radius(4.0f)
      .background(background)
      .border(1.0f, border)
      .text_color(text_color);
}

ui::dsl::StyleBuilder row_style(
    const SceneHierarchyPanelTheme &theme,
    const SceneHierarchyPanelController::VisibleRow &row_data
) {
  const bool is_scope_header =
      row_data.kind ==
      SceneHierarchyPanelController::VisibleRow::Kind::ScopeHeader;
  const bool is_type_header =
      row_data.kind ==
      SceneHierarchyPanelController::VisibleRow::Kind::TypeHeader;

  auto style = row()
                   .fill_x()
                   .items_center()
                   .gap(8.0f)
                   .min_height(px(row_data.height))
                   .padding(
                       ui::UIEdges{
                           .left = is_scope_header ? 8.0f
                                                   : (is_type_header ? 24.0f : 40.0f),
                           .top = row_data.selected ? 8.0f : 6.0f,
                           .right = 14.0f,
                           .bottom = row_data.selected ? 8.0f : 6.0f,
                       }
                   )
                   .radius(10.0f)
                   .cursor_pointer()
                   .overflow_hidden();

  if (is_scope_header) {
    return style.background(theme_alpha(theme.panel_background, 0.0f))
        .border(1.0f, theme_alpha(theme.panel_border, 0.0f))
        .hover(
            state()
                .background(theme_alpha(theme.card_background, 0.16f))
                .border(1.0f, theme_alpha(theme.panel_border, 0.18f))
        )
        .pressed(
            state()
                .background(theme_alpha(theme.card_background, 0.24f))
                .border(1.0f, theme_alpha(theme.panel_border, 0.24f))
        )
        .focused(state().border(1.0f, theme.accent));
  }

  if (is_type_header) {
    return style.background(theme_alpha(theme.panel_background, 0.0f))
        .border(1.0f, theme_alpha(theme.panel_border, 0.0f))
        .hover(
            state()
                .background(theme_alpha(theme.card_background, 0.12f))
                .border(1.0f, theme_alpha(theme.panel_border, 0.14f))
        )
        .pressed(
            state()
                .background(theme_alpha(theme.card_background, 0.18f))
                .border(1.0f, theme_alpha(theme.panel_border, 0.20f))
        )
        .focused(state().border(1.0f, theme.accent));
  }

  return style
      .background(
          row_data.selected ? theme_alpha(theme.row_selected_background, 0.28f)
                            : theme_alpha(theme.panel_background, 0.0f)
      )
      .border(
          row_data.selected ? 1.0f : 0.0f,
          row_data.selected ? theme_alpha(theme.row_selected_border, 0.52f)
                            : theme_alpha(theme.row_border, 0.0f)
      )
      .hover(
          state()
              .background(
                  row_data.selected
                      ? theme_alpha(theme.row_selected_background, 0.34f)
                      : theme_alpha(theme.card_background, 0.16f)
              )
              .border(
                  1.0f,
                  row_data.selected
                      ? theme_alpha(theme.row_selected_border, 0.56f)
                      : theme_alpha(theme.card_border, 0.0f)
              )
      )
      .pressed(
          state()
              .background(
                  row_data.selected
                      ? theme_alpha(theme.row_selected_background, 0.42f)
                      : theme_alpha(theme.card_background, 0.22f)
              )
              .border(
                  1.0f,
                  row_data.selected
                      ? theme_alpha(theme.row_selected_border, 0.62f)
                      : theme_alpha(theme.card_border, 0.0f)
              )
      )
      .focused(state().border(1.0f, theme.accent));
}

} // namespace

void SceneHierarchyPanelController::render_visible_rows(ui::im::Children &parent) {
  const SceneHierarchyPanelTheme theme;
  const float title_font_size = std::max(12.5f, m_default_font_size * 0.80f);
  const float count_font_size = std::max(10.5f, m_default_font_size * 0.68f);
  const float badge_font_size = std::max(10.0f, m_default_font_size * 0.65f);

  auto list = parent.virtual_list(
      "rows",
      m_visible_rows.size(),
      [this](size_t index) { return m_visible_rows[index].height; },
      [this,
       &theme,
       title_font_size,
       count_font_size,
       badge_font_size](ui::im::Children &visible_scope, size_t start, size_t end) {
        for (size_t index = start; index <= end; ++index) {
          const VisibleRow &row = m_visible_rows[index];
          const bool is_scope_header = row.kind == VisibleRow::Kind::ScopeHeader;
          const bool is_type_header = row.kind == VisibleRow::Kind::TypeHeader;
          const bool is_entity = row.kind == VisibleRow::Kind::Entity;
          const ResourceDescriptorID icon_texture =
              is_scope_header ? resolved_header_icon_texture_id()
                              : resolved_entity_icon_texture_id(row.type_bucket);

          auto row_scope =
              is_entity
                  ? visible_scope.item_scope(
                        "entity",
                        static_cast<uint64_t>(row.entity_id)
                    )
                  : visible_scope.item_scope("group", row.group_key);
          auto item = row_scope.pressable("row").style(row_style(theme, row));
          if (is_entity) {
            item.on_click([this, entity_id = row.entity_id]() {
              handle_entity_click(entity_id);
            });
            item.on_secondary_click(
                [this, entity_id = row.entity_id](const ui::UIPointerButtonEvent &event) {
                  select_entity(entity_id);
                  open_row_menu(entity_id, event.position);
                }
            );
          } else {
            item.on_click([this, key = row.group_key]() { toggle_group(key); });
          }

          if (is_entity && row.selected) {
            item.view("selection-bar")
                .style(
                    absolute()
                        .left(px(10.0f))
                        .top(px(8.0f))
                        .bottom(px(8.0f))
                        .width(px(2.0f))
                        .background(theme.accent)
                        .radius(999.0f)
                );
          }

          if (!is_entity) {
            auto chevron = item.image("chevron", resolved_chevron_texture_id(row.open))
                               .style(
                                   width(px(12.0f))
                                       .height(px(12.0f))
                                       .shrink(0.0f)
                                       .tint(theme.text_muted)
                               );
            static_cast<void>(chevron);
          } else {
            item.spacer("chevron-gap").style(width(px(12.0f)).shrink(0.0f));
          }

          auto icon_shell = item.view("icon-shell").visible(!icon_texture.empty()).style(
              width(px(18.0f))
                  .height(px(18.0f))
                  .shrink(0.0f)
                  .items_center()
                  .justify_center()
                  .radius(6.0f)
                  .background(
                      is_entity ? theme_alpha(theme.panel_background, 0.0f)
                                : theme_alpha(theme.card_background, 0.18f)
                  )
          );
          icon_shell.image("icon", icon_texture)
              .style(
                  width(px(14.0f))
                      .height(px(14.0f))
                      .tint(
                          row.selected
                              ? theme.accent
                              : (is_entity ? theme_alpha(theme.text_muted, 0.94f)
                                           : theme.text_muted)
                      )
              );

          auto body = item.column("body").style(grow(1.0f).min_width(px(0.0f)).gap(2.0f));
          auto header = body.row("header").style(fill_x().items_center().gap(6.0f));
          header.text("title", row.title)
              .style(
                  text_style(
                      m_default_font_id,
                      is_scope_header
                          ? std::max(13.0f, m_default_font_size * 0.82f)
                          : title_font_size,
                      row.selected ? theme.accent
                                   : (is_type_header ? theme_alpha(theme.text_primary, 0.92f)
                                                     : theme.text_primary)
                  )
              );
          header.spacer("spacer");
          header.text("count", row.count_label)
              .visible(!is_entity)
              .style(text_style(m_default_font_id, count_font_size, theme.text_muted));

          if (is_entity && row.selected) {
            auto meta = body.row("meta").style(fill_x().items_center().gap(4.0f));
            meta.text("id", row.id_label)
                .style(
                    meta_badge_style(
                        glm::vec4(0.08f, 0.16f, 0.22f, 0.52f),
                        glm::vec4(0.45f, 0.72f, 0.82f, 0.32f),
                        glm::vec4(0.45f, 0.72f, 0.82f, 0.84f)
                    ),
                    text_style(
                        m_default_font_id,
                        badge_font_size,
                        glm::vec4(0.45f, 0.72f, 0.82f, 0.84f)
                    )
                );
            meta.text("scope", row.scope_label)
                .style(
                    meta_badge_style(
                        theme_alpha(k_theme.sunset_950, 0.38f),
                        theme_alpha(k_theme.sunset_400, 0.24f),
                        theme_alpha(k_theme.sunset_400, 0.82f)
                    ),
                    text_style(
                        m_default_font_id,
                        badge_font_size,
                        theme_alpha(k_theme.sunset_400, 0.82f)
                    )
                );
            meta.text("kind", row.kind_label)
                .style(
                    meta_badge_style(
                        glm::vec4(0.16f, 0.10f, 0.24f, 0.48f),
                        glm::vec4(0.70f, 0.58f, 0.88f, 0.26f),
                        glm::vec4(0.70f, 0.58f, 0.88f, 0.86f)
                    ),
                    text_style(
                        m_default_font_id,
                        badge_font_size,
                        glm::vec4(0.70f, 0.58f, 0.88f, 0.86f)
                    )
                );
            meta.spacer("spacer");
            meta.text("status", "ACTIVE")
                .visible(row.active)
                .style(
                    meta_badge_style(
                        theme_alpha(theme.success_soft, 0.90f),
                        theme_alpha(theme.success, 0.72f),
                        theme.success
                    ),
                    text_style(m_default_font_id, badge_font_size, theme.success)
                );
          }
        }
      }
  );

  m_rows_widget = list.widget_id();

  list.style(
          fill_x()
              .flex(1.0f)
              .min_height(px(0.0f))
              .padding(8.0f)
              .gap(4.0f)
              .background(theme.panel_background)
              .border(1.0f, theme.panel_border)
              .radius(18.0f)
              .overflow_hidden()
              .scroll_vertical()
              .scrollbar_thickness(8.0f)
      )
      .on_secondary_click([this](const ui::UIPointerButtonEvent &event) {
        open_create_menu_at(event.position);
      });
}

} // namespace astralix::editor
