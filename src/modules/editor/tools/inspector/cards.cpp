#include "fields.hpp"
#include "styles.hpp"

#include <algorithm>
#include <utility>

namespace astralix::editor {
namespace panel = inspector_panel;

using namespace ui::dsl;
using namespace ui::dsl::styles;

namespace {

ResourceDescriptorID disclosure_indicator_texture(bool expanded) {
  return expanded ? ResourceDescriptorID{"icons::right_arrow_down"}
                  : ResourceDescriptorID{"icons::right_arrow"};
}

StyleBuilder component_header_style(
    const InspectorPanelTheme &theme,
    bool expanded
) {
  return fill_x()
      .items_center()
      .gap(8.0f)
      .padding(expanded ? 8.0f : 5.0f)
      .radius(8.0f)
      .cursor_pointer()
      .background(expanded ? theme.input_background : glm::vec4(0.0f))
      .border(expanded ? 1.0f : 0.0f, expanded ? theme.input_border : glm::vec4(0.0f))
      .hover(state().background(theme.input_background))
      .pressed(state().background(theme.accent_soft))
      .focused(state().border(2.0f, theme.accent));
}

} // namespace

void InspectorPanelController::rebuild_component_cards() {
  m_scalar_drafts.clear();
  m_group_drafts.clear();

  if (!m_snapshot.entity_id.has_value()) {
    m_component_expansion.clear();
    return;
  }

  std::vector<std::string> live_components;
  for (const auto &component : m_snapshot.components) {
    const auto *descriptor = panel::find_component_descriptor(component.name);
    if (descriptor != nullptr && !descriptor->visible) {
      continue;
    }

    live_components.push_back(component.name);
    const auto field_groups = panel::build_field_groups(component);
    for (const auto &group : field_groups) {
      switch (group.mode) {
        case panel::FieldMode::Text: {
          m_scalar_drafts[panel::field_draft_key(
              component.name, group.field_names.front()
          )] = std::get<std::string>(group.values.front());
          break;
        }

        case panel::FieldMode::Numeric: {
          if (group.field_names.size() == 1u) {
            m_scalar_drafts[panel::field_draft_key(
                component.name, group.field_names.front()
            )] = panel::format_value(group.values.front());
            break;
          }

          std::vector<std::string> drafts;
          drafts.reserve(group.values.size());
          for (const auto &value : group.values) {
            drafts.push_back(panel::format_value(value));
          }
          m_group_drafts[panel::group_draft_key(component.name, group.key)] =
              std::move(drafts);
          break;
        }

        case panel::FieldMode::Toggle:
        case panel::FieldMode::Enum:
        case panel::FieldMode::ReadOnly:
        default:
          break;
      }
    }
  }

  for (auto it = m_component_expansion.begin(); it != m_component_expansion.end();) {
    if (std::find(live_components.begin(), live_components.end(), it->first) ==
        live_components.end()) {
      it = m_component_expansion.erase(it);
    } else {
      ++it;
    }
  }
}

void InspectorPanelController::render_component_cards(ui::im::Children &parent) {
  const InspectorPanelTheme theme;
  size_t visible_index = 0u;

  for (const auto &component : m_snapshot.components) {
    const auto *descriptor = panel::find_component_descriptor(component.name);
    if (descriptor != nullptr && !descriptor->visible) {
      continue;
    }

    ++visible_index;
    const auto field_groups = panel::build_field_groups(component);
    const bool read_only = std::none_of(
        field_groups.begin(),
        field_groups.end(),
        [](const panel::FieldGroup &group) {
          return group.mode != panel::FieldMode::ReadOnly;
        }
    );
    const auto expansion_it = m_component_expansion.find(component.name);
    const bool expanded =
        expansion_it != m_component_expansion.end() && expansion_it->second;

    auto component_scope = parent.item_scope("component", component.name);
    auto card = component_scope.column("card").style(panel::component_card_style(theme));
    auto header = card.row("header").style(fill_x().items_center().gap(8.0f));
    auto toggle = header.pressable("toggle")
                      .on_click([this, component_name = component.name]() {
                        const auto current = m_component_expansion.find(component_name);
                        const bool next =
                            current == m_component_expansion.end() || !current->second;
                        m_component_expansion[component_name] = next;
                        mark_render_dirty();
                      })
                      .style(component_header_style(theme, expanded).flex(1.0f));
    auto toggle_content = toggle.row("content").style(fill_x().items_center().gap(8.0f));
    toggle_content
        .image("icon", disclosure_indicator_texture(expanded))
        .style(
            width(px(12.0f))
                .height(px(12.0f))
                .tint(expanded ? theme.accent : theme.text_muted)
        );
    auto toggle_text = toggle_content.column("text").style(items_start().gap(2.0f));
    toggle_text.text("title", panel::humanize_token(component.name))
        .style(font_size(14.0f).text_color(theme.text_primary));
    toggle_text
        .text(
            "caption",
            field_groups.empty() ? std::string("Tag component")
                                 : panel::component_count_label(field_groups.size())
        )
        .style(font_size(11.0f).text_color(theme.text_muted));
    toggle_content.spacer("spacer");

    if (read_only) {
      auto badge = toggle_content.row("readonly").style(
          items_center()
              .padding_xy(10.0f, 5.0f)
              .background(theme.subdued_soft)
              .border(1.0f, theme.subdued)
              .radius(8.0f)
      );
      badge.text("label", "READ ONLY")
          .style(font_size(10.5f).text_color(theme.subdued));
    }

    if (descriptor == nullptr || descriptor->removable) {
      auto remove = header.pressable("remove")
                        .on_click([this, component_name = component.name]() {
                          remove_component(component_name);
                        })
                        .style(
                            panel::remove_button_style(theme)
                                .items_center()
                                .justify_center()
                                .cursor_pointer()
                        );
      remove.text("label", "Remove")
          .style(font_size(12.5f).text_color(theme.text_primary));
    }

    if (!expanded) {
      continue;
    }

    if (field_groups.empty()) {
      card.text("empty-fields", "No configurable fields.")
          .style(font_size(11.5f).text_color(theme.text_muted));
      continue;
    }

    for (size_t group_index = 0u; group_index < field_groups.size(); ++group_index) {
      const auto &group = field_groups[group_index];
      auto group_scope = card.item_scope("group", group.key);
      const std::string component_name = component.name;

      switch (group.mode) {
        case panel::FieldMode::Toggle: {
          group_scope
              .checkbox(
                  "toggle", group.label, std::get<bool>(group.values.front())
              )
              .style(panel::checkbox_field_style(theme))
              .on_toggle(
                  [this, component_name, field_name = group.field_names.front()](
                      bool value
                  ) { set_bool_field(component_name, field_name, value); }
              );
          break;
        }

        case panel::FieldMode::Enum: {
          const std::string current_value =
              std::get<std::string>(group.values.front());
          size_t selected_index = 0u;
          if (group.options != nullptr) {
            const auto it = std::find(
                group.options->begin(), group.options->end(), current_value
            );
            if (it != group.options->end()) {
              selected_index = static_cast<size_t>(
                  std::distance(group.options->begin(), it)
              );
            }
          }

          auto field = group_scope.column("enum").style(fill_x().gap(4.0f));
          field.text("label", group.label)
              .style(font_size(11.5f).text_color(theme.text_muted));
          field
              .select(
                  "input",
                  group.options != nullptr ? *group.options
                                           : std::vector<std::string>{},
                  selected_index,
                  group.label
              )
              .on_select(
                  [this,
                   component_name,
                   field_name = group.field_names.front()](
                      size_t,
                      const std::string &value
                  ) { set_enum_field(component_name, field_name, value); }
              )
              .style(panel::input_field_style(theme));
          break;
        }

        case panel::FieldMode::Text: {
          const std::string draft_key =
              panel::field_draft_key(component_name, group.field_names.front());
          const auto draft_it = m_scalar_drafts.find(draft_key);
          const std::string draft_value =
              draft_it != m_scalar_drafts.end() ? draft_it->second
                                                : std::get<std::string>(
                                                      group.values.front()
                                                  );

          auto field = group_scope.column("text").style(fill_x().gap(4.0f));
          field.text("label", group.label)
              .style(font_size(11.5f).text_color(theme.text_muted));
          field
              .text_input("input", draft_value)
              .select_all_on_focus(true)
              .on_change(
                  [this,
                   draft_key,
                   component_name,
                   field_name = group.field_names.front()](
                      const std::string &value
                  ) {
                    m_scalar_drafts[draft_key] = value;
                    set_string_field(component_name, field_name, value);
                  }
              )
              .style(panel::input_field_style(theme));
          break;
        }

        case panel::FieldMode::Numeric: {
          if (group.field_names.size() == 1u) {
            const std::string draft_key =
                panel::field_draft_key(component_name, group.field_names.front());
            const auto draft_it = m_scalar_drafts.find(draft_key);
            const std::string draft_value =
                draft_it != m_scalar_drafts.end()
                    ? draft_it->second
                    : panel::format_value(group.values.front());

            auto field = group_scope.column("numeric").style(fill_x().gap(4.0f));
            field.text("label", group.label)
                .style(font_size(11.5f).text_color(theme.text_muted));
            field
                .text_input("input", draft_value)
                .select_all_on_focus(true)
                .on_change([this, draft_key](const std::string &value) {
                  m_scalar_drafts[draft_key] = value;
                })
                .on_submit(
                    [this,
                     component_name,
                     field_name = group.field_names.front(),
                     draft_key](const std::string &) {
                      commit_numeric_field(component_name, field_name, draft_key);
                    }
                )
                .on_blur(
                    [this,
                     component_name,
                     field_name = group.field_names.front(),
                     draft_key]() {
                      commit_numeric_field(component_name, field_name, draft_key);
                    }
                )
                .style(panel::input_field_style(theme));
            break;
          }

          const std::string draft_key =
              panel::group_draft_key(component_name, group.key);
          auto draft_it = m_group_drafts.find(draft_key);
          std::vector<std::string> draft_values =
              draft_it != m_group_drafts.end() ? draft_it->second : std::vector<std::string>{};
          if (draft_values.empty()) {
            draft_values.reserve(group.values.size());
            for (const auto &value : group.values) {
              draft_values.push_back(panel::format_value(value));
            }
          }

          auto group_node = group_scope.column("numeric-group").style(fill_x().gap(4.0f));
          group_node.text("label", group.label)
              .style(font_size(11.5f).text_color(theme.text_muted));

          auto inputs = group_node.row("inputs").style(fill_x().gap(6.0f));
          for (size_t value_index = 0u; value_index < draft_values.size();
               ++value_index) {
            auto axis_scope = inputs.item_scope("axis", group.field_names[value_index]);
            auto axis = axis_scope.column("column").style(flex(1.0f).gap(2.0f));
            axis.text("label", group.axis_labels[value_index])
                .style(font_size(10.5f).text_color(theme.text_muted));
            axis
                .text_input("input", draft_values[value_index])
                .select_all_on_focus(true)
                .on_change(
                    [this, draft_key, value_index](const std::string &value) {
                      auto &drafts = m_group_drafts[draft_key];
                      if (value_index < drafts.size()) {
                        drafts[value_index] = value;
                      }
                    }
                )
                .on_submit(
                    [this,
                     component_name,
                     field_names = group.field_names,
                     draft_key](const std::string &) {
                      commit_numeric_group(component_name, field_names, draft_key);
                    }
                )
                .on_blur(
                    [this,
                     component_name,
                     field_names = group.field_names,
                     draft_key]() {
                      commit_numeric_group(component_name, field_names, draft_key);
                    }
                )
                .style(panel::input_field_style(theme));
          }
          break;
        }

        case panel::FieldMode::ReadOnly:
        default: {
          std::string value_text;
          if (group.field_names.size() == 1u) {
            value_text = panel::format_value(group.values.front());
          } else {
            for (size_t value_index = 0u; value_index < group.values.size();
                 ++value_index) {
              if (!value_text.empty()) {
                value_text += " | ";
              }
              value_text += group.axis_labels[value_index] + ": " +
                            panel::format_value(group.values[value_index]);
            }
          }

          auto field = group_scope.column("readonly").style(fill_x().gap(1.0f));
          field.text("label", group.label)
              .style(font_size(11.5f).text_color(theme.text_muted));
          field.text("value", std::move(value_text))
              .style(font_size(12.0f).text_color(theme.text_primary));
          break;
        }
      }

      if (group_index + 1u < field_groups.size()) {
        group_scope.view("separator")
            .style(fill_x().height(px(1.0f)).background(theme.separator));
      }
    }
  }

  if (visible_index == 0u) {
    auto empty = parent.column("empty-visible-components")
                     .style(panel::component_card_style(theme).items_center());
    empty.text("title", "No visible components")
        .style(font_size(14.0f).text_color(theme.text_primary));
    empty.text("body", "The selected entity only exposes internal tags right now.")
        .style(font_size(11.5f).text_color(theme.text_muted));
  }
}

} // namespace astralix::editor
