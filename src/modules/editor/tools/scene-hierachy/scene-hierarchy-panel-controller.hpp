#pragma once

#include "panels/panel-controller.hpp"
#include "virtual-list.hpp"

#include <memory>
#include <optional>
#include <string>
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
  ui::UINodeId create_row_slot(size_t slot_index);
  void bind_row_slot(size_t slot_index, size_t item_index);
  void sync_virtual_list(bool force);

  Ref<ui::UIDocument> m_document = nullptr;
  ui::UINodeId m_scene_name_node = ui::k_invalid_node_id;
  ui::UINodeId m_entity_count_node = ui::k_invalid_node_id;
  ui::UINodeId m_selection_text_node = ui::k_invalid_node_id;
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
  std::string m_search_query;
};

} // namespace astralix::editor
