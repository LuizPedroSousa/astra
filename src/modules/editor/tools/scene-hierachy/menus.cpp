#include "tools/scene-hierachy/scene-hierarchy-panel-controller.hpp"

#include "managers/scene-manager.hpp"
#include "tools/inspector/fields.hpp"

#include <algorithm>

namespace astralix::editor {
namespace panel = inspector_panel;
namespace {

Scene *active_scene() {
  auto scene_manager = SceneManager::get();
  return scene_manager != nullptr ? scene_manager->get_active_scene() : nullptr;
}

} // namespace

void SceneHierarchyPanelController::open_create_menu_anchored() {
  if (active_scene() == nullptr) {
    return;
  }

  const bool should_open = !m_create_menu_open || m_create_menu_anchor_point.has_value();
  close_menus();
  if (!should_open) {
    return;
  }

  m_create_menu_open = true;
  m_create_menu_anchor_point.reset();
  mark_render_dirty();
}

void SceneHierarchyPanelController::open_create_menu_at(glm::vec2 cursor) {
  if (active_scene() == nullptr) {
    return;
  }

  close_menus();
  m_create_menu_open = true;
  m_create_menu_anchor_point = cursor;
  mark_render_dirty();
}

void SceneHierarchyPanelController::open_row_menu(
    EntityID entity_id,
    glm::vec2 cursor
) {
  Scene *scene = active_scene();
  if (scene == nullptr || !scene->world().contains(entity_id)) {
    return;
  }

  close_menus();
  m_context_entity_id = entity_id;
  rebuild_add_component_menu();
  m_row_menu_open = true;
  m_row_menu_anchor_point = cursor;
  mark_render_dirty();
}

void SceneHierarchyPanelController::close_menus() {
  const bool changed =
      m_create_menu_open || m_create_menu_anchor_point.has_value() ||
      m_create_3d_menu_open || m_create_light_menu_open || m_row_menu_open ||
      m_row_menu_anchor_point.has_value() || m_row_add_component_menu_open ||
      m_context_entity_id.has_value();

  m_create_menu_open = false;
  m_create_menu_anchor_point.reset();
  m_create_3d_menu_open = false;
  m_create_light_menu_open = false;
  m_row_menu_open = false;
  m_row_menu_anchor_point.reset();
  m_row_add_component_menu_open = false;
  m_context_entity_id.reset();

  if (changed) {
    mark_render_dirty();
  }
}

void SceneHierarchyPanelController::rebuild_add_component_menu() {
  m_add_component_options.clear();
  m_add_component_lookup.clear();

  Scene *active = active_scene();
  const bool has_context =
      m_context_entity_id.has_value() && active != nullptr &&
      active->world().contains(*m_context_entity_id);
  if (!has_context) {
    return;
  }

  const auto entity = active->world().entity(*m_context_entity_id);
  const auto *descriptors = panel::component_descriptors();
  for (size_t index = 0u; index < panel::component_descriptor_count(); ++index) {
    const auto &descriptor = descriptors[index];
    if (!descriptor.visible || descriptor.can_add == nullptr ||
        !descriptor.can_add(entity)) {
      continue;
    }

    const std::string label = panel::humanize_token(descriptor.name);
    m_add_component_lookup.emplace(label, descriptor.name);
    m_add_component_options.push_back(label);
  }

  std::sort(m_add_component_options.begin(), m_add_component_options.end());
}

} // namespace astralix::editor
