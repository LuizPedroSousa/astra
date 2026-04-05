#include "fields.hpp"

#include "entities/serializers/scene-snapshot.hpp"
#include "managers/scene-manager.hpp"

#include <utility>

namespace astralix::editor {
namespace panel = inspector_panel;

namespace {

Scene *active_scene() {
  auto scene_manager = SceneManager::get();
  return scene_manager != nullptr ? scene_manager->get_active_scene() : nullptr;
}

} // namespace

void InspectorPanelController::set_entity_name(std::string value) {
  Scene *scene = active_scene();
  if (scene == nullptr || !m_snapshot.entity_id.has_value() ||
      !scene->world().contains(*m_snapshot.entity_id)) {
    return;
  }

  scene->world().set_name(*m_snapshot.entity_id, value);
  m_snapshot.entity_name = std::move(value);
  mark_render_dirty();
}

void InspectorPanelController::set_entity_active(bool active) {
  Scene *scene = active_scene();
  if (scene == nullptr || !m_snapshot.entity_id.has_value() ||
      !scene->world().contains(*m_snapshot.entity_id)) {
    return;
  }

  scene->world().set_active(*m_snapshot.entity_id, active);
  m_snapshot.entity_active = active;
  mark_render_dirty();
}

void InspectorPanelController::set_string_field(
    std::string component_name,
    std::string field_name,
    std::string value
) {
  Scene *scene = active_scene();
  if (scene == nullptr || !m_snapshot.entity_id.has_value() ||
      !scene->world().contains(*m_snapshot.entity_id)) {
    return;
  }

  auto *component = panel::find_component_snapshot(m_snapshot.components, component_name);
  if (component == nullptr) {
    return;
  }

  auto *field = panel::find_field(component->fields, field_name);
  if (field == nullptr) {
    return;
  }

  field->value = std::move(value);
  serialization::apply_component_snapshot(
      scene->world().entity(*m_snapshot.entity_id), *component
  );
  mark_render_dirty();
}

void InspectorPanelController::set_bool_field(
    std::string component_name,
    std::string field_name,
    bool value
) {
  Scene *scene = active_scene();
  if (scene == nullptr || !m_snapshot.entity_id.has_value() ||
      !scene->world().contains(*m_snapshot.entity_id)) {
    return;
  }

  auto *component = panel::find_component_snapshot(m_snapshot.components, component_name);
  if (component == nullptr) {
    return;
  }

  auto *field = panel::find_field(component->fields, field_name);
  if (field == nullptr) {
    return;
  }

  field->value = value;
  serialization::apply_component_snapshot(
      scene->world().entity(*m_snapshot.entity_id), *component
  );
  mark_render_dirty();
}

void InspectorPanelController::set_enum_field(
    std::string component_name,
    std::string field_name,
    std::string value
) {
  Scene *scene = active_scene();
  if (scene == nullptr || !m_snapshot.entity_id.has_value() ||
      !scene->world().contains(*m_snapshot.entity_id)) {
    return;
  }

  auto *component = panel::find_component_snapshot(m_snapshot.components, component_name);
  if (component == nullptr) {
    return;
  }

  auto *field = panel::find_field(component->fields, field_name);
  if (field == nullptr) {
    return;
  }

  field->value = std::move(value);
  serialization::apply_component_snapshot(
      scene->world().entity(*m_snapshot.entity_id), *component
  );
  refresh(true);
}

void InspectorPanelController::commit_numeric_field(
    std::string component_name,
    std::string field_name,
    std::string draft_key
) {
  Scene *scene = active_scene();
  if (scene == nullptr || !m_snapshot.entity_id.has_value() ||
      !scene->world().contains(*m_snapshot.entity_id)) {
    return;
  }

  const auto draft_it = m_scalar_drafts.find(draft_key);
  auto *component = panel::find_component_snapshot(m_snapshot.components, component_name);
  if (draft_it == m_scalar_drafts.end() || component == nullptr) {
    return;
  }

  auto *field = panel::find_field(component->fields, field_name);
  if (field == nullptr) {
    return;
  }

  if (std::holds_alternative<int>(field->value)) {
    auto parsed = panel::parse_int(draft_it->second);
    if (!parsed.has_value()) {
      return;
    }
    field->value = *parsed;
  } else if (std::holds_alternative<float>(field->value)) {
    auto parsed = panel::parse_float(draft_it->second);
    if (!parsed.has_value()) {
      return;
    }
    field->value = *parsed;
  } else {
    return;
  }

  serialization::apply_component_snapshot(
      scene->world().entity(*m_snapshot.entity_id), *component
  );
  refresh(true);
}

void InspectorPanelController::commit_numeric_group(
    std::string component_name,
    std::vector<std::string> field_names,
    std::string draft_key
) {
  Scene *scene = active_scene();
  if (scene == nullptr || !m_snapshot.entity_id.has_value() ||
      !scene->world().contains(*m_snapshot.entity_id)) {
    return;
  }

  const auto draft_it = m_group_drafts.find(draft_key);
  auto *component = panel::find_component_snapshot(m_snapshot.components, component_name);
  if (draft_it == m_group_drafts.end() || component == nullptr ||
      draft_it->second.size() != field_names.size()) {
    return;
  }

  std::vector<float> parsed_values;
  parsed_values.reserve(field_names.size());
  for (const auto &draft : draft_it->second) {
    auto parsed = panel::parse_float(draft);
    if (!parsed.has_value()) {
      return;
    }
    parsed_values.push_back(*parsed);
  }

  for (size_t index = 0u; index < field_names.size(); ++index) {
    auto *field = panel::find_field(component->fields, field_names[index]);
    if (field == nullptr) {
      return;
    }
    field->value = parsed_values[index];
  }

  serialization::apply_component_snapshot(
      scene->world().entity(*m_snapshot.entity_id), *component
  );
  refresh(true);
}

void InspectorPanelController::add_component(std::string component_name) {
  Scene *scene = active_scene();
  if (scene == nullptr || !m_snapshot.entity_id.has_value() ||
      !scene->world().contains(*m_snapshot.entity_id) ||
      component_name.empty()) {
    return;
  }

  auto entity = scene->world().entity(*m_snapshot.entity_id);
  const auto *descriptor = panel::find_component_descriptor(component_name);
  if (descriptor == nullptr || descriptor->can_add == nullptr ||
      !descriptor->can_add(entity)) {
    return;
  }

  serialization::apply_component_snapshot(
      entity, serialization::ComponentSnapshot{.name = component_name}
  );
  m_component_expansion[component_name] = true;
  refresh(true);
}

void InspectorPanelController::remove_component(std::string component_name) {
  Scene *scene = active_scene();
  if (scene == nullptr || !m_snapshot.entity_id.has_value() ||
      !scene->world().contains(*m_snapshot.entity_id)) {
    return;
  }

  auto entity = scene->world().entity(*m_snapshot.entity_id);
  const auto *descriptor = panel::find_component_descriptor(component_name);
  if (descriptor == nullptr || !descriptor->removable ||
      descriptor->remove_component == nullptr) {
    return;
  }

  descriptor->remove_component(entity);
  m_component_expansion.erase(component_name);
  refresh(true);
}

} // namespace astralix::editor
