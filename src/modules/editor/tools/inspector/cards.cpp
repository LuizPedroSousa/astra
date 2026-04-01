#include "build.hpp"

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
  if (m_document == nullptr || m_component_stack_node == ui::k_invalid_node_id) {
    return;
  }

  const InspectorPanelTheme theme;
  m_document->clear_children(m_component_stack_node);
  m_scalar_drafts.clear();
  m_group_drafts.clear();

  if (!m_snapshot.entity_id.has_value()) {
    return;
  }

  size_t visible_index = 0u;
  for (const auto &component : m_snapshot.components) {
    const auto *descriptor = panel::find_component_descriptor(component.name);
    if (descriptor != nullptr && !descriptor->visible) {
      continue;
    }

    const auto field_groups = panel::build_field_groups(component);
    const bool read_only = std::none_of(
        field_groups.begin(),
        field_groups.end(),
        [](const panel::FieldGroup &group) {
          return group.mode != panel::FieldMode::ReadOnly;
        }
    );
    const bool expanded =
        m_component_expansion.find(component.name) != m_component_expansion.end()
            ? m_component_expansion[component.name]
            : false;

    auto card = ui::dsl::column().style(panel::component_card_style(theme));

    auto header_content = ui::dsl::row().style(fill_x().items_center().gap(8.0f));
    header_content.child(
        image(disclosure_indicator_texture(expanded))
            .style(
                width(px(12.0f))
                    .height(px(12.0f))
                    .raw([expanded, theme](ui::UIStyle &style) {
                      style.tint =
                          expanded ? theme.accent : theme.text_muted;
                    })
            )
    );
    header_content.child(
        ui::dsl::column()
            .style(items_start().gap(2.0f))
            .children(
                text(panel::humanize_token(component.name))
                    .style(font_size(14.0f).text_color(theme.text_primary)),
                text(
                    field_groups.empty()
                        ? std::string("Tag component")
                        : panel::component_count_label(field_groups.size())
                )
                    .style(font_size(11.0f).text_color(theme.text_muted))
            )
    );
    header_content.child(spacer());

    if (read_only) {
      header_content.child(
          ui::dsl::row()
              .style(
                  items_center()
                      .padding_xy(10.0f, 5.0f)
                      .background(theme.subdued_soft)
                      .border(1.0f, theme.subdued)
                      .radius(8.0f)
              )
              .child(
                  text("READ ONLY")
                      .style(font_size(10.5f).text_color(theme.subdued))
              )
      );
    }

    auto header = ui::dsl::row().style(fill_x().items_center().gap(8.0f));
    header.child(
        pressable()
            .style(component_header_style(theme, expanded).flex(1.0f))
            .on_click([this, component_name = component.name]() {
              const bool next =
                  m_component_expansion.find(component_name) ==
                          m_component_expansion.end()
                      ? true
                      : !m_component_expansion[component_name];
              m_component_expansion[component_name] = next;
              rebuild_component_cards();
            })
            .child(std::move(header_content))
    );
    if (descriptor == nullptr || descriptor->removable) {
      header.child(
          button(
              "Remove",
              [this, component_name = component.name]() {
                remove_component(component_name);
              }
          )
              .style(panel::remove_button_style(theme))
      );
    }

    card.child(std::move(header));

    if (!expanded) {
      ui::dsl::append(*m_document, m_component_stack_node, card);
      ++visible_index;
      continue;
    }

    if (field_groups.empty()) {
      card.child(
          text("No configurable fields.")
              .style(font_size(11.5f).text_color(theme.text_muted))
      );
    }

    for (size_t group_index = 0u; group_index < field_groups.size(); ++group_index) {
      const panel::FieldGroup &group = field_groups[group_index];
      const std::string component_name = component.name;

      switch (group.mode) {
        case panel::FieldMode::Toggle: {
          const bool current_value = std::get<bool>(group.values.front());
          card.child(
              checkbox(group.label, current_value)
                  .style(panel::checkbox_field_style(theme))
                  .on_toggle(
                      [this, component_name, field_name = group.field_names.front()](
                          bool value
                      ) { set_bool_field(component_name, field_name, value); }
                  )
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

          card.child(
              ui::dsl::column()
                  .style(fill_x().gap(4.0f))
                  .children(
                      text(group.label)
                          .style(font_size(11.5f).text_color(theme.text_muted)),
                      select(
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
                          .style(panel::input_field_style(theme))
                  )
          );
          break;
        }

        case panel::FieldMode::Text: {
          const std::string draft_key =
              panel::field_draft_key(component_name, group.field_names.front());
          m_scalar_drafts[draft_key] = std::get<std::string>(group.values.front());
          card.child(
              ui::dsl::column()
                  .style(fill_x().gap(4.0f))
                  .children(
                      text(group.label)
                          .style(font_size(11.5f).text_color(theme.text_muted)),
                      text_input(m_scalar_drafts[draft_key])
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
                          .style(panel::input_field_style(theme))
                  )
          );
          break;
        }

        case panel::FieldMode::Numeric: {
          if (group.field_names.size() == 1u) {
            const std::string draft_key =
                panel::field_draft_key(component_name, group.field_names.front());
            m_scalar_drafts[draft_key] = panel::format_value(group.values.front());
            card.child(
                ui::dsl::column()
                    .style(fill_x().gap(4.0f))
                    .children(
                        text(group.label)
                            .style(font_size(11.5f).text_color(theme.text_muted)),
                        text_input(m_scalar_drafts[draft_key])
                            .select_all_on_focus(true)
                            .on_change([this, draft_key](const std::string &value) {
                              m_scalar_drafts[draft_key] = value;
                            })
                            .on_submit(
                                [this,
                                 component_name,
                                 field_name = group.field_names.front(),
                                 draft_key](const std::string &) {
                                  commit_numeric_field(
                                      component_name, field_name, draft_key
                                  );
                                }
                            )
                            .on_blur([this,
                                      component_name,
                                      field_name = group.field_names.front(),
                                      draft_key]() {
                              commit_numeric_field(component_name, field_name, draft_key);
                            })
                            .style(panel::input_field_style(theme))
                    )
            );
          } else {
            const std::string draft_key =
                panel::group_draft_key(component_name, group.key);
            std::vector<std::string> drafts;
            drafts.reserve(group.values.size());
            for (const auto &value : group.values) {
              drafts.push_back(panel::format_value(value));
            }
            m_group_drafts[draft_key] = drafts;

            auto group_node = ui::dsl::column().style(fill_x().gap(4.0f));
            group_node.child(
                text(group.label)
                    .style(font_size(11.5f).text_color(theme.text_muted))
            );

            auto inputs = ui::dsl::row().style(fill_x().gap(6.0f));
            for (size_t value_index = 0u; value_index < drafts.size(); ++value_index) {
              auto axis_column = ui::dsl::column().style(flex(1.0f).gap(2.0f));
              axis_column.child(
                  text(group.axis_labels[value_index])
                      .style(font_size(10.5f).text_color(theme.text_muted))
              );
              axis_column.child(
                  text_input(drafts[value_index])
                      .select_all_on_focus(true)
                      .on_change(
                          [this, draft_key, value_index](const std::string &value) {
                            auto &draft_values = m_group_drafts[draft_key];
                            if (value_index < draft_values.size()) {
                              draft_values[value_index] = value;
                            }
                          }
                      )
                      .on_submit(
                          [this,
                           component_name,
                           field_names = group.field_names,
                           draft_key](const std::string &) {
                            commit_numeric_group(
                                component_name, field_names, draft_key
                            );
                          }
                      )
                      .on_blur(
                          [this,
                           component_name,
                           field_names = group.field_names,
                           draft_key]() {
                            commit_numeric_group(
                                component_name, field_names, draft_key
                            );
                          }
                      )
                      .style(panel::input_field_style(theme))
              );
              inputs.child(std::move(axis_column));
            }

            group_node.child(std::move(inputs));
            card.child(std::move(group_node));
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

          card.child(
              ui::dsl::column()
                  .style(fill_x().gap(1.0f))
                  .children(
                      text(group.label)
                          .style(font_size(11.5f).text_color(theme.text_muted)),
                      text(std::move(value_text))
                          .style(font_size(12.0f).text_color(theme.text_primary))
                  )
          );
          break;
        }
      }

      if (group_index + 1u < field_groups.size()) {
        card.child(
            ui::dsl::row().style(
                fill_x()
                    .height(px(1.0f))
                    .background(theme.separator)
            )
        );
      }
    }

    ui::dsl::append(*m_document, m_component_stack_node, card);
    ++visible_index;
  }

  if (visible_index == 0u) {
    ui::dsl::append(
        *m_document,
        m_component_stack_node,
        ui::dsl::column()
            .style(panel::component_card_style(theme).items_center())
            .children(
                text("No visible components")
                    .style(font_size(14.0f).text_color(theme.text_primary)),
                text("The selected entity only exposes internal tags right now.")
                    .style(font_size(11.5f).text_color(theme.text_muted))
            )
    );
  }
}

} // namespace astralix::editor
