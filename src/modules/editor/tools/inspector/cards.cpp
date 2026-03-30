#include "build.hpp"

#include <algorithm>
#include <utility>

namespace astralix::editor {
namespace panel = inspector_panel;

using namespace ui::dsl;
using namespace ui::dsl::styles;

namespace {

std::string disclosure_indicator(bool expanded) { return expanded ? "v" : ">"; }

StyleBuilder component_header_style(
    const InspectorPanelTheme &theme,
    bool expanded
) {
  return fill_x()
      .items_center()
      .gap(10.0f)
      .padding(12.0f)
      .radius(14.0f)
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

    auto card = column(
                    "inspector_component_" + std::to_string(visible_index) + "_" +
                    panel::node_token(component.name)
    )
                    .style(panel::component_card_style(theme));

    auto header_content = row("header_toggle_content")
                              .style(fill_x().items_center().gap(10.0f));
    header_content.child(
        text(disclosure_indicator(expanded), "disclosure_indicator")
            .style(
                font_size(13.0f)
                    .text_color(expanded ? theme.accent : theme.text_muted)
            )
    );
    header_content.child(
        column("copy")
            .style(items_start().gap(3.0f))
            .children(
                text(panel::humanize_token(component.name), "title")
                    .style(font_size(15.0f).text_color(theme.text_primary)),
                text(
                    field_groups.empty()
                        ? std::string("Tag component")
                        : panel::component_count_label(field_groups.size()),
                    "meta"
                )
                    .style(font_size(12.0f).text_color(theme.text_muted))
            )
    );
    header_content.child(spacer("header_spacer"));

    if (read_only) {
      header_content.child(
          text("READ ONLY", "readonly_badge")
              .style(
                  font_size(11.0f)
                      .text_color(theme.subdued)
                      .padding_xy(10.0f, 7.0f)
                      .background(theme.subdued_soft)
                      .border(1.0f, theme.subdued)
                      .radius(999.0f)
              )
      );
    }

    auto header = row("header").style(fill_x().items_center().gap(10.0f));
    header.child(
        pressable("header_toggle")
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
              },
              "remove_button"
          )
              .style(panel::compact_button_style(theme))
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
          text("No configurable fields.", "empty_body")
              .style(font_size(12.5f).text_color(theme.text_muted))
      );
    }

    for (size_t group_index = 0u; group_index < field_groups.size(); ++group_index) {
      const panel::FieldGroup &group = field_groups[group_index];
      const std::string component_name = component.name;
      const std::string label_name =
          "field_label_" + std::to_string(group_index);
      const std::string body_name =
          "field_body_" + std::to_string(group_index);

      switch (group.mode) {
        case panel::FieldMode::Toggle: {
          const bool current_value = std::get<bool>(group.values.front());
          card.child(
              checkbox(group.label, current_value, body_name)
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
              column(label_name)
                  .style(fill_x().gap(6.0f))
                  .children(
                      text(group.label, "label")
                          .style(font_size(12.5f).text_color(theme.text_muted)),
                      select(
                          group.options != nullptr ? *group.options
                                                   : std::vector<std::string>{},
                          selected_index,
                          group.label,
                          body_name
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
              column(label_name)
                  .style(fill_x().gap(6.0f))
                  .children(
                      text(group.label, "label")
                          .style(font_size(12.5f).text_color(theme.text_muted)),
                      text_input(m_scalar_drafts[draft_key], {}, body_name)
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
                column(label_name)
                    .style(fill_x().gap(6.0f))
                    .children(
                        text(group.label, "label")
                            .style(font_size(12.5f).text_color(theme.text_muted)),
                        text_input(m_scalar_drafts[draft_key], {}, body_name)
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

            auto group_node = column(label_name).style(fill_x().gap(6.0f));
            group_node.child(
                text(group.label, "label")
                    .style(font_size(12.5f).text_color(theme.text_muted))
            );

            auto inputs = row(body_name).style(fill_x().gap(8.0f));
            for (size_t value_index = 0u; value_index < drafts.size(); ++value_index) {
              auto axis_column =
                  column("axis_" + std::to_string(value_index))
                      .style(flex(1.0f).gap(4.0f));
              axis_column.child(
                  text(group.axis_labels[value_index], "axis_label")
                      .style(font_size(11.0f).text_color(theme.text_muted))
              );
              axis_column.child(
                  text_input(drafts[value_index], {}, "axis_input")
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
              column(label_name)
                  .style(fill_x().gap(4.0f))
                  .children(
                      text(group.label, "label")
                          .style(font_size(12.5f).text_color(theme.text_muted)),
                      text(std::move(value_text), "value")
                          .style(font_size(13.0f).text_color(theme.text_primary))
                  )
          );
          break;
        }
      }
    }

    ui::dsl::append(*m_document, m_component_stack_node, card);
    ++visible_index;
  }

  if (visible_index == 0u) {
    ui::dsl::append(
        *m_document,
        m_component_stack_node,
        column("inspector_component_empty")
            .style(panel::component_card_style(theme).items_center())
            .children(
                text("No visible components", "title")
                    .style(font_size(16.0f).text_color(theme.text_primary)),
                text(
                    "The selected entity only exposes internal tags right now.",
                    "body"
                )
                    .style(font_size(12.5f).text_color(theme.text_muted))
            )
    );
  }
}

} // namespace astralix::editor
