#pragma once

#include "allocator-metrics.hpp"
#include "panels/panel-controller.hpp"
#include "process-metrics.hpp"
#include "systems/render-system/frame-stats.hpp"
#include "widgets/time-series.hpp"

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
  void render(ui::im::Frame &ui) override;
  void mount(const PanelMountContext &context) override;
  void unmount() override;
  void update(const PanelUpdateContext &context) override;
  std::optional<uint64_t> render_version() const override;

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

  void sample_timing(double dt);
  RuntimeSnapshot collect_snapshot() const;
  void sample_chart_history();
  void sample_process_metrics();
  void sample_allocator_metrics();
  void sample_gpu_metrics();
  void mark_render_dirty() { ++m_render_revision; }

  double m_sample_elapsed = 0.0;
  size_t m_sample_frames = 0u;
  float m_average_fps = 0.0f;
  float m_average_frame_time_ms = 0.0f;
  bool m_has_timing_sample = false;
  uint64_t m_render_revision = 1u;

  static constexpr double k_chart_sample_interval = 0.2;

  RuntimeSnapshot m_latest_snapshot;
  uint64_t m_header_version = 0u;
  uint64_t m_last_rendered_header_version = 0u;

  ui::TimeSeries<k_history_capacity> m_fps_history;
  ui::TimeSeries<k_history_capacity> m_frame_time_history;
  double m_chart_sample_elapsed = 0.0;

  ProcessMetricsSampler m_process_sampler;
  float m_latest_cpu_percent = 0.0f;
  float m_latest_memory_rss_mb = 0.0f;
  float m_latest_binary_size_mb = 0.0f;

  ui::TimeSeries<k_history_capacity> m_cpu_history;
  ui::TimeSeries<k_history_capacity> m_memory_history;

  size_t m_latest_thread_count = 0u;
  size_t m_latest_open_fd_count = 0u;
  size_t m_latest_minor_page_faults = 0u;
  size_t m_latest_major_page_faults = 0u;
  size_t m_latest_voluntary_ctx_switches = 0u;
  size_t m_latest_involuntary_ctx_switches = 0u;
  float m_latest_disk_read_bytes = 0.0f;
  float m_latest_disk_write_bytes = 0.0f;

  AllocatorMetricsSampler m_allocator_sampler;
  float m_latest_heap_used_mb = 0.0f;
  float m_latest_mmap_used_mb = 0.0f;
  float m_latest_alloc_rate_mb_per_sec = 0.0f;

  ui::TimeSeries<k_history_capacity> m_heap_history;

  FrameStats m_latest_frame_stats;
  ui::TimeSeries<k_history_capacity> m_draw_calls_history;
  ui::TimeSeries<k_history_capacity> m_gpu_time_history;
};

} // namespace astralix::editor
