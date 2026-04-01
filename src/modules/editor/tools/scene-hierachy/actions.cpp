#include "tools/scene-hierachy/scene-hierarchy-panel-controller.hpp"

#include "components/light.hpp"
#include "components/material.hpp"
#include "components/mesh.hpp"
#include "components/tags.hpp"
#include "components/transform.hpp"
#include "editor-selection-store.hpp"
#include "entities/serializers/scene-component-serialization.hpp"
#include "managers/scene-manager.hpp"
#include "tools/inspector/build.hpp"

#include <utility>

namespace astralix::editor {
namespace {

Scene *active_scene() {
  auto scene_manager = SceneManager::get();
  return scene_manager != nullptr ? scene_manager->get_active_scene() : nullptr;
}

} // namespace

void SceneHierarchyPanelController::create_empty_entity() {
  Scene *active = active_scene();
  if (active == nullptr) {
    return;
  }

  auto entity = active->spawn_entity("GameObject");
  entity.emplace<scene::SceneEntity>();
  entity.emplace<scene::Transform>();

  close_menus();
  select_entity(entity.id());
}

void SceneHierarchyPanelController::create_mesh_primitive(
    std::string name,
    Mesh mesh
) {
  Scene *active = active_scene();
  if (active == nullptr) {
    return;
  }

  auto entity = active->spawn_entity(std::move(name));
  entity.emplace<scene::SceneEntity>();
  entity.emplace<scene::Transform>();
  entity.emplace<rendering::MeshSet>(
      rendering::MeshSet{.meshes = {std::move(mesh)}}
  );
  entity.emplace<rendering::ShaderBinding>(
      rendering::ShaderBinding{.shader = "shaders::lighting"}
  );
  entity.emplace<rendering::Renderable>();

  close_menus();
  select_entity(entity.id());
}

void SceneHierarchyPanelController::create_light_entity(
    std::string name,
    rendering::LightType type
) {
  Scene *active = active_scene();
  if (active == nullptr) {
    return;
  }

  auto entity = active->spawn_entity(std::move(name));
  entity.emplace<scene::SceneEntity>();
  entity.emplace<scene::Transform>();
  entity.emplace<rendering::Light>(rendering::Light{.type = type});

  if (type == rendering::LightType::Point) {
    entity.emplace<rendering::PointLightAttenuation>();
  } else if (type == rendering::LightType::Spot) {
    entity.emplace<rendering::SpotLightCone>();
  }

  close_menus();
  select_entity(entity.id());
}

void SceneHierarchyPanelController::delete_context_entity() {
  Scene *active = active_scene();
  if (active == nullptr || !m_context_entity_id.has_value() ||
      !active->world().contains(*m_context_entity_id)) {
    return;
  }

  const EntityID entity_id = *m_context_entity_id;
  active->world().destroy(entity_id);

  if (m_selected_entity_id.has_value() &&
      static_cast<uint64_t>(*m_selected_entity_id) ==
          static_cast<uint64_t>(entity_id)) {
    m_selected_entity_id.reset();
    editor_selection_store()->clear_selected_entity();
  }

  m_context_entity_id.reset();
  close_menus();
  refresh(true);
}

void SceneHierarchyPanelController::add_component_to_context_entity(
    std::string component_name
) {
  Scene *active = active_scene();
  if (active == nullptr || !m_context_entity_id.has_value() ||
      component_name.empty() ||
      !active->world().contains(*m_context_entity_id)) {
    return;
  }

  auto entity = active->world().entity(*m_context_entity_id);
  const auto *descriptor = inspector_panel::find_component_descriptor(component_name);
  if (descriptor == nullptr || descriptor->can_add == nullptr ||
      !descriptor->can_add(entity)) {
    return;
  }

  serialization::apply_component_snapshot(
      entity,
      serialization::ComponentSnapshot{.name = std::move(component_name)}
  );

  close_menus();
  refresh(true);
}

} // namespace astralix::editor
