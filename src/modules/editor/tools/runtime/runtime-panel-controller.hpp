#pragma once

#include "panels/panel-controller.hpp"

#include <cstddef>
#include <string>

namespace astralix::editor {

class RuntimePanelController : public PanelController {
public:
  ui::dsl::NodeSpec build() override;
  void mount(const PanelMountContext &context) override;
  void unmount() override;
  void update(const PanelUpdateContext &context) override;

private:
  struct RuntimeSnapshot {
    bool has_scene = false;
    std::string scene_name;
    size_t entity_count = 0u;
    size_t renderable_count = 0u;
    size_t rigid_body_count = 0u;
    size_t dynamic_body_count = 0u;
    size_t static_body_count = 0u;
    size_t light_count = 0u;
    size_t camera_count = 0u;
    size_t ui_root_count = 0u;
  };

  void refresh(bool force = false);
  void sample_timing(double dt);
  RuntimeSnapshot collect_snapshot() const;
  void sync_status_pill(bool has_scene, bool force);

  Ref<ui::UIDocument> m_document = nullptr;
  ui::UINodeId m_scene_status_chip_node = ui::k_invalid_node_id;
  ui::UINodeId m_scene_status_text_node = ui::k_invalid_node_id;
  ui::UINodeId m_scene_name_node = ui::k_invalid_node_id;
  ui::UINodeId m_metrics_root_node = ui::k_invalid_node_id;
  ui::UINodeId m_empty_state_node = ui::k_invalid_node_id;
  ui::UINodeId m_fps_value_node = ui::k_invalid_node_id;
  ui::UINodeId m_frame_time_value_node = ui::k_invalid_node_id;
  ui::UINodeId m_entities_value_node = ui::k_invalid_node_id;
  ui::UINodeId m_renderables_value_node = ui::k_invalid_node_id;
  ui::UINodeId m_rigid_bodies_value_node = ui::k_invalid_node_id;
  ui::UINodeId m_dynamic_bodies_value_node = ui::k_invalid_node_id;
  ui::UINodeId m_static_bodies_value_node = ui::k_invalid_node_id;
  ui::UINodeId m_lights_value_node = ui::k_invalid_node_id;
  ui::UINodeId m_cameras_value_node = ui::k_invalid_node_id;
  ui::UINodeId m_ui_roots_value_node = ui::k_invalid_node_id;
  double m_sample_elapsed = 0.0;
  size_t m_sample_frames = 0u;
  float m_average_fps = 0.0f;
  float m_average_frame_time_ms = 0.0f;
  bool m_has_timing_sample = false;
  bool m_last_scene_presence = false;
};

} // namespace astralix::editor
