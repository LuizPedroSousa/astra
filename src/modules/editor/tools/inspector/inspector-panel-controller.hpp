#pragma once

#include "panels/panel-controller.hpp"
#include "entities/serializers/scene-snapshot-types.hpp"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace astralix::editor {

class InspectorPanelController final : public PanelController {
public:
  struct InspectedEntitySnapshot {
    bool has_scene = false;
    std::string scene_name;
    std::optional<EntityID> entity_id;
    std::string entity_name;
    bool entity_active = false;
    std::vector<serialization::ComponentSnapshot> components;
  };

  ui::dsl::NodeSpec build() override;
  void mount(const PanelMountContext &context) override;
  void unmount() override;
  void update(const PanelUpdateContext &context) override;

private:
  void refresh(bool force = false);
  InspectedEntitySnapshot collect_snapshot() const;
  void sync_static_ui();
  void sync_add_component_controls();
  void rebuild_component_cards();

  void set_entity_name(std::string value);
  void set_entity_active(bool active);
  void set_string_field(
      std::string component_name,
      std::string field_name,
      std::string value
  );
  void set_bool_field(
      std::string component_name,
      std::string field_name,
      bool value
  );
  void set_enum_field(
      std::string component_name,
      std::string field_name,
      std::string value
  );
  void commit_numeric_field(
      std::string component_name,
      std::string field_name,
      std::string draft_key
  );
  void commit_numeric_group(
      std::string component_name,
      std::vector<std::string> field_names,
      std::string draft_key
  );
  void add_component(std::string component_name);
  void remove_component(std::string component_name);

  Ref<ui::UIDocument> m_document = nullptr;
  ResourceDescriptorID m_default_font_id;
  float m_default_font_size = 16.0f;

  ui::UINodeId m_scene_name_node = ui::k_invalid_node_id;
  ui::UINodeId m_selection_title_node = ui::k_invalid_node_id;
  ui::UINodeId m_component_count_node = ui::k_invalid_node_id;
  ui::UINodeId m_entity_id_node = ui::k_invalid_node_id;
  ui::UINodeId m_entity_name_input_node = ui::k_invalid_node_id;
  ui::UINodeId m_entity_active_node = ui::k_invalid_node_id;
  ui::UINodeId m_add_component_select_node = ui::k_invalid_node_id;
  ui::UINodeId m_add_component_button_node = ui::k_invalid_node_id;
  ui::UINodeId m_component_scroll_node = ui::k_invalid_node_id;
  ui::UINodeId m_component_stack_node = ui::k_invalid_node_id;
  ui::UINodeId m_empty_state_node = ui::k_invalid_node_id;
  ui::UINodeId m_empty_title_node = ui::k_invalid_node_id;
  ui::UINodeId m_empty_body_node = ui::k_invalid_node_id;

  InspectedEntitySnapshot m_snapshot;
  uint64_t m_last_selection_revision = 0u;
  std::vector<std::string> m_add_component_options;
  std::unordered_map<std::string, std::string> m_add_component_lookup;
  std::string m_pending_add_component_name;
  std::unordered_map<std::string, std::string> m_scalar_drafts;
  std::unordered_map<std::string, std::vector<std::string>> m_group_drafts;
  std::unordered_map<std::string, bool> m_component_expansion;
};

} // namespace astralix::editor
