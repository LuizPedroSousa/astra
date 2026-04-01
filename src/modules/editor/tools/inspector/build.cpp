#include "build.hpp"

#include <cctype>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <utility>

namespace astralix::editor {
namespace panel = inspector_panel;

using namespace ui::dsl;
using namespace ui::dsl::styles;

namespace {

bool editable_all_fields(std::string_view) { return true; }

const std::vector<std::string> *no_enum_options(std::string_view) {
  return nullptr;
}

std::string trim_numeric_suffix(std::string value) {
  if (value.find('.') == std::string::npos) {
    return value;
  }

  while (!value.empty() && value.back() == '0') {
    value.pop_back();
  }

  if (!value.empty() && value.back() == '.') {
    value.pop_back();
  }

  return value.empty() ? std::string("0") : value;
}

std::string capitalize_word(std::string value) {
  if (!value.empty()) {
    value.front() = static_cast<char>(std::toupper(value.front()));
  }
  return value;
}

std::optional<std::pair<std::string, std::string>>
vector_field_base_and_axis(std::string_view name) {
  const size_t dot = name.rfind('.');
  if (dot == std::string_view::npos || dot + 1u >= name.size()) {
    return std::nullopt;
  }

  const std::string_view axis = name.substr(dot + 1u);
  if (axis != "x" && axis != "y" && axis != "z" && axis != "w") {
    return std::nullopt;
  }

  return std::make_pair(
      std::string(name.substr(0u, dot)),
      std::string(axis)
  );
}

bool snapshot_fields_equal(
    const serialization::fields::FieldList &lhs,
    const serialization::fields::FieldList &rhs
) {
  if (lhs.size() != rhs.size()) {
    return false;
  }

  for (size_t index = 0u; index < lhs.size(); ++index) {
    if (lhs[index].name != rhs[index].name || lhs[index].value != rhs[index].value) {
      return false;
    }
  }

  return true;
}

} // namespace

bool panel::same_entity(EntityID lhs, EntityID rhs) {
  return static_cast<uint64_t>(lhs) == static_cast<uint64_t>(rhs);
}

bool panel::same_entity(
    const std::optional<EntityID> &lhs,
    const std::optional<EntityID> &rhs
) {
  if (lhs.has_value() != rhs.has_value()) {
    return false;
  }

  if (!lhs.has_value()) {
    return true;
  }

  return same_entity(*lhs, *rhs);
}

serialization::ComponentSnapshot *panel::find_component_snapshot(
    std::vector<serialization::ComponentSnapshot> &components,
    std::string_view name
) {
  for (auto &component : components) {
    if (component.name == name) {
      return &component;
    }
  }

  return nullptr;
}

const serialization::ComponentSnapshot *panel::find_component_snapshot(
    const std::vector<serialization::ComponentSnapshot> &components,
    std::string_view name
) {
  for (const auto &component : components) {
    if (component.name == name) {
      return &component;
    }
  }

  return nullptr;
}

serialization::fields::Field *panel::find_field(
    serialization::fields::FieldList &fields,
    std::string_view name
) {
  for (auto &field : fields) {
    if (field.name == name) {
      return &field;
    }
  }

  return nullptr;
}

std::string panel::format_value(const SerializableValue &value) {
  if (const auto *integer = std::get_if<int>(&value); integer != nullptr) {
    return std::to_string(*integer);
  }

  if (const auto *decimal = std::get_if<float>(&value); decimal != nullptr) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(4) << *decimal;
    return trim_numeric_suffix(stream.str());
  }

  if (const auto *string_value = std::get_if<std::string>(&value);
      string_value != nullptr) {
    return *string_value;
  }

  if (const auto *boolean = std::get_if<bool>(&value); boolean != nullptr) {
    return *boolean ? "true" : "false";
  }

  return {};
}

std::optional<int> panel::parse_int(std::string_view value) {
  if (value.empty()) {
    return std::nullopt;
  }

  char *end = nullptr;
  const long parsed = std::strtol(std::string(value).c_str(), &end, 10);
  if (end == nullptr || *end != '\0') {
    return std::nullopt;
  }

  return static_cast<int>(parsed);
}

std::optional<float> panel::parse_float(std::string_view value) {
  if (value.empty()) {
    return std::nullopt;
  }

  char *end = nullptr;
  const float parsed = std::strtof(std::string(value).c_str(), &end);
  if (end == nullptr || *end != '\0') {
    return std::nullopt;
  }

  return parsed;
}

std::string panel::humanize_token(std::string_view token) {
  std::string result;
  result.reserve(token.size() + 8u);

  for (size_t index = 0u; index < token.size(); ++index) {
    const char ch = token[index];
    const bool uppercase = std::isupper(static_cast<unsigned char>(ch)) != 0;
    const bool needs_space =
        index > 0u && uppercase &&
        (std::islower(static_cast<unsigned char>(token[index - 1])) != 0);

    if (ch == '_' || ch == '.') {
      result.push_back(' ');
    } else {
      if (needs_space) {
        result.push_back(' ');
      }
      result.push_back(ch);
    }
  }

  std::string collapsed;
  collapsed.reserve(result.size());
  bool last_space = true;
  for (char ch : result) {
    const bool is_space = std::isspace(static_cast<unsigned char>(ch)) != 0;
    if (is_space) {
      if (!last_space) {
        collapsed.push_back(' ');
      }
    } else {
      collapsed.push_back(ch);
    }
    last_space = is_space;
  }

  while (!collapsed.empty() && collapsed.back() == ' ') {
    collapsed.pop_back();
  }

  return capitalize_word(collapsed);
}

std::string panel::node_token(std::string_view value) {
  std::string token;
  token.reserve(value.size());
  for (char ch : value) {
    const unsigned char raw = static_cast<unsigned char>(ch);
    token.push_back(
        std::isalnum(raw) != 0 ? static_cast<char>(std::tolower(raw)) : '_'
    );
  }
  return token;
}

std::string panel::component_count_label(size_t count) {
  return std::to_string(count) + (count == 1u ? " component" : " components");
}

std::string panel::field_draft_key(
    std::string_view component_name,
    std::string_view field_name
) {
  return std::string(component_name) + "::" + std::string(field_name);
}

std::string panel::group_draft_key(
    std::string_view component_name,
    std::string_view field_name
) {
  return std::string(component_name) + "::group::" + std::string(field_name);
}

std::vector<panel::FieldGroup> panel::build_field_groups(
    const serialization::ComponentSnapshot &component
) {
  std::vector<FieldGroup> groups;
  const auto *descriptor = find_component_descriptor(component.name);
  const auto editable = descriptor != nullptr && descriptor->field_editable != nullptr
                            ? descriptor->field_editable
                            : &editable_all_fields;
  const auto enum_options =
      descriptor != nullptr && descriptor->enum_options != nullptr
          ? descriptor->enum_options
          : &no_enum_options;

  for (size_t index = 0u; index < component.fields.size(); ++index) {
    const auto &field = component.fields[index];
    if (auto base_axis = vector_field_base_and_axis(field.name);
        base_axis.has_value()) {
      const std::string &base = base_axis->first;
      bool already_grouped = false;
      for (const auto &group : groups) {
        if (group.key == base) {
          already_grouped = true;
          break;
        }
      }

      if (already_grouped) {
        continue;
      }

      FieldGroup group;
      group.key = base;
      group.label = humanize_token(base);
      const bool group_is_editable = editable(base);

      for (size_t cursor = index; cursor < component.fields.size(); ++cursor) {
        const auto next = vector_field_base_and_axis(component.fields[cursor].name);
        if (!next.has_value() || next->first != base) {
          break;
        }

        group.field_names.push_back(component.fields[cursor].name);
        group.axis_labels.push_back(capitalize_word(next->second));
        group.values.push_back(component.fields[cursor].value);
      }

      group.mode = group_is_editable ? FieldMode::Numeric : FieldMode::ReadOnly;
      groups.push_back(std::move(group));
      continue;
    }

    FieldGroup group;
    group.key = field.name;
    group.label = humanize_token(field.name);
    group.field_names.push_back(field.name);
    group.values.push_back(field.value);
    group.options = enum_options(field.name);

    if (group.options != nullptr && std::holds_alternative<std::string>(field.value)) {
      group.mode = FieldMode::Enum;
    } else if (!editable(field.name)) {
      group.mode = FieldMode::ReadOnly;
    } else if (std::holds_alternative<bool>(field.value)) {
      group.mode = FieldMode::Toggle;
    } else if (std::holds_alternative<std::string>(field.value)) {
      group.mode = FieldMode::Text;
    } else {
      group.mode = FieldMode::Numeric;
    }

    groups.push_back(std::move(group));
  }

  return groups;
}

size_t panel::visible_component_count(
    const std::vector<serialization::ComponentSnapshot> &components
) {
  size_t count = 0u;
  for (const auto &component : components) {
    const auto *descriptor = find_component_descriptor(component.name);
    if (descriptor == nullptr || descriptor->visible) {
      ++count;
    }
  }
  return count;
}

bool panel::snapshots_equal(
    const std::vector<serialization::ComponentSnapshot> &lhs,
    const std::vector<serialization::ComponentSnapshot> &rhs
) {
  if (lhs.size() != rhs.size()) {
    return false;
  }

  for (size_t index = 0u; index < lhs.size(); ++index) {
    if (lhs[index].name != rhs[index].name ||
        !snapshot_fields_equal(lhs[index].fields, rhs[index].fields)) {
      return false;
    }
  }

  return true;
}

bool panel::snapshots_equal(
    const InspectorPanelController::InspectedEntitySnapshot &lhs,
    const InspectorPanelController::InspectedEntitySnapshot &rhs
) {
  return lhs.has_scene == rhs.has_scene && lhs.scene_name == rhs.scene_name &&
         same_entity(lhs.entity_id, rhs.entity_id) &&
         lhs.entity_name == rhs.entity_name &&
         lhs.entity_active == rhs.entity_active &&
         snapshots_equal(lhs.components, rhs.components);
}

ui::dsl::StyleBuilder panel::component_card_style(
    const InspectorPanelTheme &theme
) {
  return fill_x()
      .padding(4.0f)
      .gap(8.0f)
      .radius(12.0f)
      .background(theme.card_background)
      .border(1.0f, theme.card_border);
}

ui::dsl::StyleBuilder panel::input_field_style(
    const InspectorPanelTheme &theme
) {
  return fill_x()
      .padding_xy(8.0f, 6.0f)
      .font_size(12.5f)
      .control_gap(5.0f)
      .control_indicator_size(14.0f)
      .background(theme.input_background)
      .border(1.0f, theme.input_border)
      .radius(8.0f);
}

ui::dsl::StyleBuilder panel::compact_button_style(
    const InspectorPanelTheme &theme
) {
  return padding_xy(10.0f, 5.0f)
      .background(theme.accent_soft)
      .border(1.0f, theme.accent)
      .radius(8.0f);
}

ui::dsl::StyleBuilder panel::remove_button_style(
    const InspectorPanelTheme &theme
) {
  return padding_xy(10.0f, 5.0f)
      .background(theme.remove_background)
      .border(1.0f, theme.remove_border)
      .radius(8.0f);
}

ui::dsl::StyleBuilder panel::checkbox_field_style(
    const InspectorPanelTheme &theme
) {
  return fill_x()
      .padding_xy(0.0f, 4.0f)
      .font_size(12.5f)
      .text_color(theme.text_primary)
      .control_gap(5.0f)
      .control_indicator_size(14.0f);
}

ui::dsl::NodeSpec InspectorPanelController::build() {
  const InspectorPanelTheme theme;

  auto header_row = ui::dsl::row().style(fill_x().items_center().gap(8.0f));
  header_row.child(
      ui::dsl::column()
          .style(items_start().gap(3.0f))
          .children(
              text("Inspector")
                  .style(font_size(18.0f).text_color(theme.text_primary)),
              text("Inspect and edit the selected scene entity.")
                  .style(font_size(12.0f).text_color(theme.text_muted))
          )
  );
  header_row.child(spacer());
  header_row.child(
      ui::dsl::row()
          .style(
              items_center()
                  .padding_xy(12.0f, 6.0f)
                  .background(theme.accent_soft)
                  .border(1.0f, theme.accent)
                  .radius(8.0f)
          )
          .child(
              text("No active scene")
                  .bind(m_scene_name_node)
                  .style(font_size(12.0f).text_color(theme.accent))
          )
  );

  auto add_row = ui::dsl::row().style(fill_x().items_center().gap(8.0f));
  add_row.child(
      select({}, 0u, "Add component")
          .bind(m_add_component_select_node)
          .enabled(false)
          .on_select([this](size_t, const std::string &value) {
            const auto it = m_add_component_lookup.find(value);
            m_pending_add_component_name =
                it != m_add_component_lookup.end() ? it->second : std::string{};
          })
          .style(panel::input_field_style(theme).flex(1.0f))
  );
  add_row.child(
      button(
          "Add", [this]() { add_component(m_pending_add_component_name); }
      )
          .bind(m_add_component_button_node)
          .enabled(false)
          .style(panel::compact_button_style(theme))
  );

  return ui::dsl::column()
      .style(fill().background(theme.shell_background).padding(12.0f).gap(10.0f))
      .children(
          ui::dsl::column()
              .style(
                  fill_x()
                      .padding(14.0f)
                      .gap(8.0f)
                      .radius(12.0f)
                      .background(theme.panel_background)
                      .border(1.0f, theme.panel_border)
              )
              .children(
                  std::move(header_row),
                  text("Nothing selected")
                      .bind(m_selection_title_node)
                      .style(font_size(16.0f).text_color(theme.text_primary)),
                  text("0 components")
                      .bind(m_component_count_node)
                      .style(font_size(12.0f).text_color(theme.text_muted)),
                  text("Entity ID --")
                      .bind(m_entity_id_node)
                      .style(font_size(12.0f).text_color(theme.text_muted)),
                  text_input({}, "Entity name")
                      .bind(m_entity_name_input_node)
                      .enabled(false)
                      .select_all_on_focus(true)
                      .on_change([this](const std::string &value) {
                        set_entity_name(value);
                      })
                      .style(panel::input_field_style(theme)),
                  checkbox("Entity is active", false)
                      .bind(m_entity_active_node)
                      .enabled(false)
                      .style(panel::checkbox_field_style(theme))
                      .on_toggle([this](bool active) { set_entity_active(active); })
              ),
          std::move(add_row),
          scroll_view()
              .bind(m_component_scroll_node)
              .style(
                  fill_x()
                      .flex(1.0f)
                      .padding(ui::UIEdges{
                          .left = 6.0f,
                          .top = 6.0f,
                          .right = 6.0f,
                          .bottom = 10.0f,
                      })
                      .gap(8.0f)
                      .background(theme.panel_background)
                      .border(1.0f, theme.panel_border)
                      .radius(12.0f)
              )
              .child(
                  ui::dsl::column()
                      .bind(m_component_stack_node)
                      .style(fill_x().gap(4.0f))
              ),
          ui::dsl::column()
              .bind(m_empty_state_node)
              .style(
                  fill_x()
                      .flex(1.0f)
                      .justify_center()
                      .items_center()
                      .padding(18.0f)
                      .gap(8.0f)
                      .radius(12.0f)
                      .background(theme.panel_background)
                      .border(1.0f, theme.panel_border)
              )
              .visible(false)
              .children(
                  text("Nothing selected")
                      .bind(m_empty_title_node)
                      .style(font_size(16.0f).text_color(theme.text_primary)),
                  text("Select an entity in Scene Hierarchy to inspect it.")
                      .bind(m_empty_body_node)
                      .style(font_size(12.0f).text_color(theme.text_muted))
              )
      );
}

} // namespace astralix::editor
