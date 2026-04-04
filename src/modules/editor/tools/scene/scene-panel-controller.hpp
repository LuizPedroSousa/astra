#pragma once

#include "managers/scene-manager.hpp"
#include "panels/panel-controller.hpp"

#include <optional>
#include <string>

namespace astralix::editor {

class ScenePanelController final : public PanelController {
public:
  static constexpr PanelMinimumSize kMinimumSize{
      .width = 460.0f,
      .height = 360.0f,
  };

  PanelMinimumSize minimum_size() const override { return kMinimumSize; }
  ui::dsl::NodeSpec build() override;
  void mount(const PanelMountContext &context) override;
  void unmount() override;
  void update(const PanelUpdateContext &) override;

private:
  void refresh(bool force = false);
  void rebuild_scene_menu_content();
  void open_scene_menu(bool reset_query = true);
  bool scene_menu_open() const;
  void open_runtime_prompt(SceneSessionKind target_kind);
  void close_runtime_prompt();
  bool runtime_prompt_open() const;
  void switch_active_scene_session(SceneSessionKind kind);
  void play_active_scene();
  void pause_active_scene();
  void stop_active_scene();
  void save_active_scene();
  void promote_active_scene();
  void promote_source_to_preview_and_activate();
  void activate_scene(const std::string &scene_id);

  Ref<ui::UIDocument> m_document = nullptr;
  ResourceDescriptorID m_default_font_id = "fonts::roboto";
  float m_default_font_size = 16.0f;

  ui::UINodeId m_scene_menu_button_node = ui::k_invalid_node_id;
  ui::UINodeId m_scene_menu_trigger_icon_node = ui::k_invalid_node_id;
  ui::UINodeId m_scene_menu_trigger_label_node = ui::k_invalid_node_id;
  ui::UINodeId m_scene_mode_toggle_node = ui::k_invalid_node_id;
  ui::UINodeId m_scene_save_button_node = ui::k_invalid_node_id;
  ui::UINodeId m_scene_promote_button_node = ui::k_invalid_node_id;
  ui::UINodeId m_scene_menu_node = ui::k_invalid_node_id;
  ui::UINodeId m_scene_menu_search_input_node = ui::k_invalid_node_id;
  ui::UINodeId m_scene_menu_result_count_node = ui::k_invalid_node_id;
  ui::UINodeId m_scene_menu_list_node = ui::k_invalid_node_id;
  ui::UINodeId m_scene_menu_content_node = ui::k_invalid_node_id;
  ui::UINodeId m_scene_menu_empty_state_node = ui::k_invalid_node_id;
  ui::UINodeId m_scene_menu_empty_title_node = ui::k_invalid_node_id;
  ui::UINodeId m_scene_menu_empty_body_node = ui::k_invalid_node_id;
  ui::UINodeId m_scene_status_card_node = ui::k_invalid_node_id;
  ui::UINodeId m_scene_status_scene_value_node = ui::k_invalid_node_id;
  ui::UINodeId m_scene_status_source_chip_node = ui::k_invalid_node_id;
  ui::UINodeId m_scene_status_source_value_node = ui::k_invalid_node_id;
  ui::UINodeId m_scene_status_preview_chip_node = ui::k_invalid_node_id;
  ui::UINodeId m_scene_status_preview_value_node = ui::k_invalid_node_id;
  ui::UINodeId m_scene_status_runtime_chip_node = ui::k_invalid_node_id;
  ui::UINodeId m_scene_status_runtime_value_node = ui::k_invalid_node_id;
  ui::UINodeId m_scene_status_execution_chip_node = ui::k_invalid_node_id;
  ui::UINodeId m_scene_status_execution_value_node = ui::k_invalid_node_id;
  ui::UINodeId m_scene_playback_controls_node = ui::k_invalid_node_id;
  ui::UINodeId m_scene_play_button_node = ui::k_invalid_node_id;
  ui::UINodeId m_scene_pause_button_node = ui::k_invalid_node_id;
  ui::UINodeId m_scene_stop_button_node = ui::k_invalid_node_id;
  ui::UINodeId m_scene_status_runtime_hint_node = ui::k_invalid_node_id;
  ui::UINodeId m_runtime_prompt_node = ui::k_invalid_node_id;
  ui::UINodeId m_runtime_prompt_title_node = ui::k_invalid_node_id;
  ui::UINodeId m_runtime_prompt_body_node = ui::k_invalid_node_id;

  std::string m_scene_menu_query;
  std::optional<SceneSessionKind> m_runtime_prompt_target_kind;
};

} // namespace astralix::editor
