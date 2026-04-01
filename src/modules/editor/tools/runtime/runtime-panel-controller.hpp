#pragma once

#include "allocator-metrics.hpp"
#include "panels/panel-controller.hpp"
#include "process-metrics.hpp"
#include "systems/render-system/frame-stats.hpp"
#include "widgets/time-series.hpp"

#include <array>
#include <cstddef>
#include <string>

namespace astralix::editor {

class RuntimePanelController : public PanelController {
public:
  static constexpr PanelMinimumSize kMinimumSize{
      .width = 420.0f,
      .height = 280.0f,
  };

  static constexpr size_t k_history_capacity = 50u;

  PanelMinimumSize minimum_size() const override { return kMinimumSize; }
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
    size_t vertex_count = 0u;
    size_t triangle_count = 0u;
    size_t shadow_caster_count = 0u;
    size_t texture_count = 0u;
    size_t shader_count = 0u;
    size_t material_count = 0u;
    size_t model_count = 0u;
  };

  void refresh(bool force = false);
  void sample_timing(double dt);
  RuntimeSnapshot collect_snapshot() const;
  void sync_status_pill(bool has_scene, bool force);
  void sample_chart_history();
  void refresh_bar_charts();
  void refresh_gauges(const RuntimeSnapshot &snapshot);
  void refresh_line_charts();
  void sample_process_metrics();
  void refresh_process_charts();
  void sample_extended_process_metrics();
  void sample_allocator_metrics();
  void sample_gpu_metrics();
  void refresh_extended_charts();
  void update_chart_tooltips();

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
  ui::UINodeId m_vertices_value_node = ui::k_invalid_node_id;
  ui::UINodeId m_triangles_value_node = ui::k_invalid_node_id;
  ui::UINodeId m_shadow_casters_value_node = ui::k_invalid_node_id;
  ui::UINodeId m_textures_value_node = ui::k_invalid_node_id;
  ui::UINodeId m_shaders_value_node = ui::k_invalid_node_id;
  ui::UINodeId m_materials_value_node = ui::k_invalid_node_id;
  ui::UINodeId m_models_value_node = ui::k_invalid_node_id;
  double m_sample_elapsed = 0.0;
  size_t m_sample_frames = 0u;
  float m_average_fps = 0.0f;
  float m_average_frame_time_ms = 0.0f;
  bool m_has_timing_sample = false;
  bool m_last_scene_presence = false;

  static constexpr double k_chart_sample_interval = 0.2;

  ui::TimeSeries<k_history_capacity> m_fps_history;
  ui::TimeSeries<k_history_capacity> m_frame_time_history;
  double m_chart_sample_elapsed = 0.0;

  ui::UINodeId m_fps_chart_container = ui::k_invalid_node_id;
  std::array<ui::UINodeId, k_history_capacity> m_fps_bar_nodes{};
  ui::UINodeId m_fps_tooltip_text_node = ui::k_invalid_node_id;

  ui::UINodeId m_frame_time_chart_container = ui::k_invalid_node_id;
  std::array<ui::UINodeId, k_history_capacity> m_frame_time_bar_nodes{};
  ui::UINodeId m_frame_time_tooltip_text_node = ui::k_invalid_node_id;

  ui::UINodeId m_entities_gauge_fill = ui::k_invalid_node_id;
  ui::UINodeId m_renderables_gauge_fill = ui::k_invalid_node_id;
  ui::UINodeId m_triangles_gauge_fill = ui::k_invalid_node_id;

  ui::UINodeId m_fps_line_chart_node = ui::k_invalid_node_id;
  ui::UINodeId m_frame_time_line_chart_node = ui::k_invalid_node_id;

  size_t m_hovered_fps_bar_index = k_history_capacity;
  size_t m_hovered_frame_time_bar_index = k_history_capacity;

  ProcessMetricsSampler m_process_sampler;
  float m_latest_cpu_percent = 0.0f;
  float m_latest_memory_rss_mb = 0.0f;
  float m_latest_binary_size_mb = 0.0f;

  ui::TimeSeries<k_history_capacity> m_cpu_history;
  ui::TimeSeries<k_history_capacity> m_memory_history;

  ui::UINodeId m_cpu_value_node = ui::k_invalid_node_id;
  ui::UINodeId m_memory_value_node = ui::k_invalid_node_id;
  ui::UINodeId m_binary_size_value_node = ui::k_invalid_node_id;
  ui::UINodeId m_cpu_line_chart_node = ui::k_invalid_node_id;
  ui::UINodeId m_memory_line_chart_node = ui::k_invalid_node_id;
  ui::UINodeId m_memory_gauge_fill = ui::k_invalid_node_id;

  size_t m_latest_thread_count = 0u;
  size_t m_latest_open_fd_count = 0u;
  size_t m_latest_minor_page_faults = 0u;
  size_t m_latest_major_page_faults = 0u;
  size_t m_latest_voluntary_ctx_switches = 0u;
  size_t m_latest_involuntary_ctx_switches = 0u;
  float m_latest_disk_read_bytes = 0.0f;
  float m_latest_disk_write_bytes = 0.0f;

  ui::UINodeId m_threads_value_node = ui::k_invalid_node_id;
  ui::UINodeId m_fds_value_node = ui::k_invalid_node_id;
  ui::UINodeId m_minor_faults_value_node = ui::k_invalid_node_id;
  ui::UINodeId m_major_faults_value_node = ui::k_invalid_node_id;
  ui::UINodeId m_voluntary_ctx_value_node = ui::k_invalid_node_id;
  ui::UINodeId m_involuntary_ctx_value_node = ui::k_invalid_node_id;
  ui::UINodeId m_disk_read_value_node = ui::k_invalid_node_id;
  ui::UINodeId m_disk_write_value_node = ui::k_invalid_node_id;

  AllocatorMetricsSampler m_allocator_sampler;
  float m_latest_heap_used_mb = 0.0f;
  float m_latest_mmap_used_mb = 0.0f;
  float m_latest_alloc_rate_mb_per_sec = 0.0f;

  ui::TimeSeries<k_history_capacity> m_heap_history;

  ui::UINodeId m_heap_used_value_node = ui::k_invalid_node_id;
  ui::UINodeId m_mmap_used_value_node = ui::k_invalid_node_id;
  ui::UINodeId m_alloc_rate_value_node = ui::k_invalid_node_id;
  ui::UINodeId m_heap_line_chart_node = ui::k_invalid_node_id;

  FrameStats m_latest_frame_stats;
  ui::TimeSeries<k_history_capacity> m_draw_calls_history;
  ui::TimeSeries<k_history_capacity> m_gpu_time_history;

  ui::UINodeId m_draw_calls_value_node = ui::k_invalid_node_id;
  ui::UINodeId m_state_changes_value_node = ui::k_invalid_node_id;
  ui::UINodeId m_gpu_time_value_node = ui::k_invalid_node_id;
  ui::UINodeId m_gpu_mem_used_value_node = ui::k_invalid_node_id;
  ui::UINodeId m_gpu_mem_total_value_node = ui::k_invalid_node_id;
  ui::UINodeId m_draw_calls_line_chart_node = ui::k_invalid_node_id;
  ui::UINodeId m_gpu_time_line_chart_node = ui::k_invalid_node_id;
  ui::UINodeId m_gpu_memory_gauge_fill = ui::k_invalid_node_id;
};

} // namespace astralix::editor
