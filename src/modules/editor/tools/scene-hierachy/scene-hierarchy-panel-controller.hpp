#pragma once

#include "components/light.hpp"
#include "panels/panel-controller.hpp"
#include "resources/mesh.hpp"
#include "tools/scene-hierachy/helpers.hpp"

#include <cstdint>
#include <glm/glm.hpp>
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
  void render(ui::im::Frame &ui) override;
  void mount(const PanelMountContext &context) override;
  void unmount() override;
  void update(const PanelUpdateContext &context) override;
  std::optional<uint64_t> render_version() const override;
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

public:
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

private:
  Snapshot collect_snapshot() const;
  void refresh(bool force = false);
  void render_visible_rows(ui::im::Children &parent);
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
  void mark_render_dirty() { ++m_render_revision; }

  ui::im::Runtime *m_runtime = nullptr;
  ResourceDescriptorID m_default_font_id;
  float m_default_font_size = 16.0f;

  ui::im::WidgetId m_create_button_widget = ui::im::k_invalid_widget_id;
  ui::im::WidgetId m_create_3d_trigger_widget = ui::im::k_invalid_widget_id;
  ui::im::WidgetId m_create_light_trigger_widget = ui::im::k_invalid_widget_id;
  ui::im::WidgetId m_row_add_component_trigger_widget =
      ui::im::k_invalid_widget_id;
  ui::im::WidgetId m_rows_widget = ui::im::k_invalid_widget_id;

  bool m_has_scene = false;
  std::string m_scene_name;
  std::string m_empty_title;
  std::string m_empty_body;
  std::vector<EntityEntry> m_all_entities;
  std::vector<EntityEntry> m_entities;
  std::vector<VisibleRow> m_visible_rows;
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
  uint64_t m_render_revision = 1u;

  bool m_create_menu_open = false;
  std::optional<glm::vec2> m_create_menu_anchor_point;
  bool m_create_3d_menu_open = false;
  bool m_create_light_menu_open = false;

  bool m_row_menu_open = false;
  std::optional<glm::vec2> m_row_menu_anchor_point;
  bool m_row_add_component_menu_open = false;
};

} // namespace astralix::editor
