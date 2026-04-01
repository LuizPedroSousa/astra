#pragma once

#include "panels/panel-controller.hpp"
#include "components/light.hpp"
#include "resources/mesh.hpp"
#include "virtual-list.hpp"

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace astralix::editor {

class SceneHierarchyPanelController final : public PanelController {
public:
  ui::dsl::NodeSpec build() override;
  void mount(const PanelMountContext &context) override;
  void unmount() override;
  void update(const PanelUpdateContext &context) override;
  void load_state(Ref<SerializationContext> state) override;
  void save_state(Ref<SerializationContext> state) const override;

private:
  struct EntityEntry {
    EntityID id{};
    std::string name;
    std::string kind_label;
    std::string meta_label;
    bool active = false;
    bool scene_backed = false;
  };

  struct Snapshot {
    bool has_scene = false;
    std::string scene_name;
    std::vector<EntityEntry> entities;
  };

  struct RowNodes {
    ui::UINodeId slot = ui::k_invalid_node_id;
    ui::UINodeId button = ui::k_invalid_node_id;
    ui::UINodeId title = ui::k_invalid_node_id;
    ui::UINodeId status_badge = ui::k_invalid_node_id;
    ui::UINodeId status_badge_text = ui::k_invalid_node_id;
    ui::UINodeId kind_badge = ui::k_invalid_node_id;
    ui::UINodeId kind_badge_text = ui::k_invalid_node_id;
    ui::UINodeId meta = ui::k_invalid_node_id;
  };

  Snapshot collect_snapshot() const;
  void refresh(bool force = false);
  void select_entity(EntityID entity_id);
  const EntityEntry *selected_entry() const;
  void open_create_menu_anchored();
  void open_create_menu_at(glm::vec2 cursor);
  void open_row_menu(EntityID entity_id, glm::vec2 cursor);
  void close_menus();
  void rebuild_add_component_menu();
  void create_empty_entity();
  void create_mesh_primitive(std::string name, Mesh mesh);
  void create_light_entity(std::string name, rendering::LightType type);
  void delete_context_entity();
  void add_component_to_context_entity(std::string component_name);
  ui::UINodeId create_row_slot(size_t slot_index);
  void bind_row_slot(size_t slot_index, size_t item_index);
  void sync_virtual_list(bool force);

  Ref<ui::UIDocument> m_document = nullptr;
  ui::UINodeId m_scene_name_node = ui::k_invalid_node_id;
  ui::UINodeId m_entity_count_node = ui::k_invalid_node_id;
  ui::UINodeId m_selection_text_node = ui::k_invalid_node_id;
  ui::UINodeId m_create_button_node = ui::k_invalid_node_id;
  ui::UINodeId m_create_menu_node = ui::k_invalid_node_id;
  ui::UINodeId m_create_3d_trigger_node = ui::k_invalid_node_id;
  ui::UINodeId m_create_3d_menu_node = ui::k_invalid_node_id;
  ui::UINodeId m_create_light_trigger_node = ui::k_invalid_node_id;
  ui::UINodeId m_create_light_menu_node = ui::k_invalid_node_id;
  ui::UINodeId m_row_menu_node = ui::k_invalid_node_id;
  ui::UINodeId m_row_add_component_trigger_node = ui::k_invalid_node_id;
  ui::UINodeId m_row_add_component_menu_node = ui::k_invalid_node_id;
  ui::UINodeId m_row_add_component_container_node = ui::k_invalid_node_id;
  ui::UINodeId m_search_input_node = ui::k_invalid_node_id;
  ui::UINodeId m_scroll_node = ui::k_invalid_node_id;
  ui::UINodeId m_empty_state_node = ui::k_invalid_node_id;
  ui::UINodeId m_empty_title_node = ui::k_invalid_node_id;
  ui::UINodeId m_empty_body_node = ui::k_invalid_node_id;
  ResourceDescriptorID m_default_font_id;
  float m_default_font_size = 16.0f;

  std::vector<RowNodes> m_row_slots;
  std::vector<EntityEntry> m_all_entities;
  std::vector<EntityEntry> m_entities;
  std::unique_ptr<ui::VirtualListController> m_virtual_list;
  std::optional<EntityID> m_selected_entity_id;
  std::optional<EntityID> m_context_entity_id;
  std::string m_search_query;
  std::vector<std::string> m_add_component_options;
  std::unordered_map<std::string, std::string> m_add_component_lookup;
};

} // namespace astralix::editor
