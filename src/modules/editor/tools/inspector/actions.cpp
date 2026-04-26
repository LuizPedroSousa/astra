#include "fields.hpp"

#include "entities/serializers/scene-snapshot.hpp"
#include "managers/resource-manager.hpp"
#include "managers/scene-manager.hpp"
#include "resources/descriptors/material-descriptor.hpp"

#include <utility>

namespace astralix::editor {
namespace panel = inspector_panel;

namespace {

Scene *active_scene() {
  auto scene_manager = SceneManager::get();
  return scene_manager != nullptr ? scene_manager->get_active_scene() : nullptr;
}

Ref<MaterialDescriptor> inspected_material_descriptor(
    std::vector<serialization::ComponentSnapshot> &components,
    std::string_view component_name
) {
  if (!panel::is_material_properties_component(component_name)) {
    return nullptr;
  }

  auto *component = panel::find_component_snapshot(components, component_name);
  if (component == nullptr) {
    return nullptr;
  }

  const auto material_id =
      serialization::fields::read_string(component->fields, "material_id");
  if (!material_id.has_value() || material_id->empty()) {
    return nullptr;
  }

  auto manager = resource_manager();
  return manager != nullptr
             ? manager->get_by_descriptor_id<MaterialDescriptor>(*material_id)
             : nullptr;
}

bool apply_material_numeric_field(
    MaterialDescriptor &material,
    std::string_view field_name,
    float value
) {
  if (field_name == "base_color_factor.x") {
    material.base_color_factor.x = value;
    return true;
  }
  if (field_name == "base_color_factor.y") {
    material.base_color_factor.y = value;
    return true;
  }
  if (field_name == "base_color_factor.z") {
    material.base_color_factor.z = value;
    return true;
  }
  if (field_name == "base_color_factor.w") {
    material.base_color_factor.w = value;
    return true;
  }
  if (field_name == "emissive_factor.x") {
    material.emissive_factor.x = value;
    return true;
  }
  if (field_name == "emissive_factor.y") {
    material.emissive_factor.y = value;
    return true;
  }
  if (field_name == "emissive_factor.z") {
    material.emissive_factor.z = value;
    return true;
  }
  if (field_name == "metallic_factor") {
    material.metallic_factor = value;
    return true;
  }
  if (field_name == "roughness_factor") {
    material.roughness_factor = value;
    return true;
  }
  if (field_name == "occlusion_strength") {
    material.occlusion_strength = value;
    return true;
  }
  if (field_name == "normal_scale") {
    material.normal_scale = value;
    return true;
  }
  if (field_name == "bloom_intensity") {
    material.bloom_intensity = value;
    return true;
  }

  return false;
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

  if (panel::is_material_properties_component(component_name)) {
    auto material =
        inspected_material_descriptor(m_snapshot.components, component_name);
    auto parsed = panel::parse_float(draft_it->second);
    if (material == nullptr || !parsed.has_value() ||
        !apply_material_numeric_field(*material, field_name, *parsed)) {
      return;
    }

    field->value = *parsed;
    refresh(true);
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

  if (panel::is_material_properties_component(component_name)) {
    auto material =
        inspected_material_descriptor(m_snapshot.components, component_name);
    if (material == nullptr) {
      return;
    }

    for (size_t index = 0u; index < field_names.size(); ++index) {
      if (!apply_material_numeric_field(
              *material, field_names[index], parsed_values[index]
          )) {
        return;
      }
    }

    refresh(true);
    return;
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
