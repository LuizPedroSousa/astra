#pragma once

#include "components/light.hpp"
#include "panels/panel-controller.hpp"
#include "resources/mesh.hpp"
#include "tools/scene-hierachy/helpers.hpp"
#include "virtual-list.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace astralix::editor {

class SceneHierarchyPanelController final : public PanelController {
public:
  static constexpr PanelMinimumSize kMinimumSize{
      .width = 360.0f,
      .height = 320.0f,
  };

  PanelMinimumSize minimum_size() const override { return kMinimumSize; }
  ui::dsl::NodeSpec build() override;
  void mount(const PanelMountContext &context) override;
  void unmount() override;
  void update(const PanelUpdateContext &context) override;
  void load_state(Ref<SerializationContext> state) override;
  void save_state(Ref<SerializationContext> state) const override;

private:
  using ScopeBucket = scene_hierarchy_panel::ScopeBucket;
  using TypeBucket = scene_hierarchy_panel::TypeBucket;

  struct EntityEntry {
    EntityID id{};
    std::string name;
    std::string kind_label;
    std::string scope_label;
    std::string search_blob;
    bool active = false;
    bool scene_backed = false;
    ScopeBucket scope_bucket = ScopeBucket::SceneBacked;
    TypeBucket type_bucket = TypeBucket::Other;
  };

  struct Snapshot {
    bool has_scene = false;
    std::string scene_name;
    std::vector<EntityEntry> entities;
  };

  struct VisibleRow {
    enum class Kind : uint8_t {
      ScopeHeader = 0,
      TypeHeader,
      Entity,
    };

    Kind kind = Kind::Entity;
    std::string group_key;
    std::string title;
    std::string id_label;
    std::string scope_label;
    std::string kind_label;
    std::string count_label;
    EntityID entity_id{};
    ScopeBucket scope_bucket = ScopeBucket::SceneBacked;
    TypeBucket type_bucket = TypeBucket::Other;
    bool open = false;
    bool active = false;
    bool selected = false;
    float height = 0.0f;
  };

  struct RowNodes {
    ui::UINodeId slot = ui::k_invalid_node_id;
    ui::UINodeId button = ui::k_invalid_node_id;
    ui::UINodeId selection_bar = ui::k_invalid_node_id;
    ui::UINodeId guide_scope = ui::k_invalid_node_id;
    ui::UINodeId guide_type = ui::k_invalid_node_id;
    ui::UINodeId guide_branch = ui::k_invalid_node_id;
    ui::UINodeId chevron = ui::k_invalid_node_id;
    ui::UINodeId icon_shell = ui::k_invalid_node_id;
    ui::UINodeId icon = ui::k_invalid_node_id;
    ui::UINodeId title = ui::k_invalid_node_id;
    ui::UINodeId count = ui::k_invalid_node_id;
    ui::UINodeId id_badge = ui::k_invalid_node_id;
    ui::UINodeId id_badge_text = ui::k_invalid_node_id;
    ui::UINodeId meta_row = ui::k_invalid_node_id;
    ui::UINodeId scope_badge = ui::k_invalid_node_id;
    ui::UINodeId scope_badge_text = ui::k_invalid_node_id;
    ui::UINodeId kind_badge = ui::k_invalid_node_id;
    ui::UINodeId kind_badge_text = ui::k_invalid_node_id;
    ui::UINodeId meta_spacer = ui::k_invalid_node_id;
    ui::UINodeId status_badge = ui::k_invalid_node_id;
    ui::UINodeId status_badge_text = ui::k_invalid_node_id;
  };

  Snapshot collect_snapshot() const;
  void refresh(bool force = false);
  void select_entity(EntityID entity_id);
  void handle_entity_click(EntityID entity_id);
  const EntityEntry *selected_entry() const;
  void toggle_group(std::string key);
  bool persisted_group_open(std::string_view key) const;
  std::unordered_set<std::string> transient_open_groups(
      const std::vector<EntityEntry> &filtered_entities,
      std::optional<EntityID> reveal_entity_id
  ) const;
  std::vector<VisibleRow> build_visible_rows(
      const std::vector<EntityEntry> &filtered_entities,
      std::optional<EntityID> reveal_entity_id
  ) const;
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
  ui::UINodeId m_selection_line_node = ui::k_invalid_node_id;
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
  std::vector<VisibleRow> m_visible_rows;
  std::unique_ptr<ui::VirtualListController> m_virtual_list;
  std::optional<EntityID> m_selected_entity_id;
  std::optional<EntityID> m_context_entity_id;
  std::string m_search_query;
  uint64_t m_last_selection_revision = 0u;
  std::unordered_map<std::string, bool> m_group_open;
  std::vector<std::string> m_add_component_options;
  std::unordered_map<std::string, std::string> m_add_component_lookup;
  std::optional<EntityID> m_last_clicked_entity_id;
  double m_last_click_time = 0.0;
  double m_elapsed_time = 0.0;
  double m_snapshot_poll_elapsed = 0.0;
  bool m_virtual_list_layout_dirty = true;
};

} // namespace astralix::editor
