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
  static constexpr PanelMinimumSize kMinimumSize{
      .width = 400.0f,
      .height = 320.0f,
  };

  struct InspectedEntitySnapshot {
    bool has_scene = false;
    std::string scene_name;
    std::optional<EntityID> entity_id;
    std::string entity_name;
    bool entity_active = false;
    std::vector<serialization::ComponentSnapshot> components;
  };

  PanelMinimumSize minimum_size() const override { return kMinimumSize; }
  void render(ui::im::Frame &ui) override;
  void mount(const PanelMountContext &context) override;
  void unmount() override;
  void update(const PanelUpdateContext &context) override;
  std::optional<uint64_t> render_version() const override;

private:
  void refresh(bool force = false);
  InspectedEntitySnapshot collect_snapshot() const;
  void sync_add_component_controls();
  void rebuild_component_cards();
  void render_component_cards(ui::im::Children &parent);

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
  void set_float_field(
      std::string component_name,
      std::string field_name,
      float value
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
  void mark_render_dirty() { ++m_render_revision; }

  InspectedEntitySnapshot m_snapshot;
  uint64_t m_last_selection_revision = 0u;
  uint64_t m_render_revision = 1u;
  std::vector<std::string> m_add_component_options;
  std::unordered_map<std::string, std::string> m_add_component_lookup;
  std::string m_pending_add_component_name;
  std::unordered_map<std::string, std::string> m_scalar_drafts;
  std::unordered_map<std::string, std::vector<std::string>> m_group_drafts;
  std::unordered_map<std::string, bool> m_component_expansion;
  ui::im::WidgetId m_entity_name_widget = ui::im::k_invalid_widget_id;
  bool m_pending_entity_name_focus = false;
};

} // namespace astralix::editor
