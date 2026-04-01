#include "tools/scene-hierachy/scene-hierarchy-panel-controller.hpp"

#include "dsl.hpp"
#include "managers/scene-manager.hpp"
#include "tools/inspector/build.hpp"

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
  if (m_document == nullptr || m_create_menu_node == ui::k_invalid_node_id ||
      m_create_button_node == ui::k_invalid_node_id || active_scene() == nullptr) {
    return;
  }

  close_menus();
  m_document->open_popover_anchored_to(
      m_create_menu_node,
      m_create_button_node,
      ui::UIPopupPlacement::BottomStart,
      0u
  );
}

void SceneHierarchyPanelController::open_create_menu_at(glm::vec2 cursor) {
  if (m_document == nullptr || m_create_menu_node == ui::k_invalid_node_id ||
      active_scene() == nullptr) {
    return;
  }

  close_menus();
  m_document->open_popover_at(
      m_create_menu_node,
      cursor,
      ui::UIPopupPlacement::BottomStart,
      0u
  );
}

void SceneHierarchyPanelController::open_row_menu(
    EntityID entity_id,
    glm::vec2 cursor
) {
  Scene *scene = active_scene();
  if (m_document == nullptr || m_row_menu_node == ui::k_invalid_node_id ||
      scene == nullptr || !scene->world().contains(entity_id)) {
    return;
  }

  close_menus();
  m_context_entity_id = entity_id;
  rebuild_add_component_menu();
  m_document->open_popover_at(
      m_row_menu_node,
      cursor,
      ui::UIPopupPlacement::BottomStart,
      0u
  );
}

void SceneHierarchyPanelController::close_menus() {
  m_context_entity_id.reset();
  if (m_document != nullptr) {
    m_document->close_all_popovers();
  }
}

void SceneHierarchyPanelController::rebuild_add_component_menu() {
  if (m_document == nullptr ||
      m_row_add_component_container_node == ui::k_invalid_node_id) {
    return;
  }

  m_document->clear_children(m_row_add_component_container_node);
  m_add_component_options.clear();
  m_add_component_lookup.clear();

  Scene *active = active_scene();
  const bool has_context =
      m_context_entity_id.has_value() && active != nullptr &&
      active->world().contains(*m_context_entity_id);
  if (m_row_add_component_trigger_node != ui::k_invalid_node_id) {
    m_document->set_enabled(m_row_add_component_trigger_node, has_context);
  }
  if (!has_context) {
    return;
  }

  auto entity = active->world().entity(*m_context_entity_id);
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

  for (const std::string &label : m_add_component_options) {
    ui::dsl::append(
        *m_document,
        m_row_add_component_container_node,
        ui::dsl::menu_item(
            label,
            [this, label]() {
              auto it = m_add_component_lookup.find(label);
              if (it != m_add_component_lookup.end()) {
                add_component_to_context_entity(it->second);
              }
            }
        )
    );
  }

  if (m_row_add_component_trigger_node != ui::k_invalid_node_id) {
    m_document->set_enabled(
        m_row_add_component_trigger_node,
        !m_add_component_options.empty()
    );
  }
}

} // namespace astralix::editor
