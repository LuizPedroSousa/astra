#include "tools/runtime/runtime-panel-controller.hpp"

#include "dsl.hpp"
#include "editor-theme.hpp"

namespace astralix::editor {

namespace {

void populate_bar_nodes(
    ui::UIDocument &document,
    ui::UINodeId container_node,
    std::array<ui::UINodeId, RuntimePanelController::k_history_capacity>
        &bar_nodes,
    const glm::vec4 &fill_color,
    float border_radius,
    size_t &hovered_index,
    size_t sentinel
) {
  using namespace ui::dsl;
  using namespace ui::dsl::styles;

  for (size_t i = 0u; i < bar_nodes.size(); ++i) {
    bar_nodes[i] = append(
        document,
        container_node,
        bar_chart_bar(fill_color, border_radius)
            .on_hover([&hovered_index, i]() { hovered_index = i; })
    );
  }
  hovered_index = sentinel;
}

} // namespace

void RuntimePanelController::mount(const PanelMountContext &context) {
  m_document = context.document;

  const RuntimePanelTheme theme;

  if (m_document != nullptr && m_fps_chart_container != ui::k_invalid_node_id) {
    populate_bar_nodes(
        *m_document,
        m_fps_chart_container,
        m_fps_bar_nodes,
        theme.chart_bar_fill,
        2.0f,
        m_hovered_fps_bar_index,
        k_history_capacity
    );
  }

  if (m_document != nullptr &&
      m_frame_time_chart_container != ui::k_invalid_node_id) {
    populate_bar_nodes(
        *m_document,
        m_frame_time_chart_container,
        m_frame_time_bar_nodes,
        theme.chart_bar_fill_alt,
        2.0f,
        m_hovered_frame_time_bar_index,
        k_history_capacity
    );
  }

  m_fps_history.clear();
  m_frame_time_history.clear();
  m_chart_sample_elapsed = 0.0;

  refresh(true);
}

void RuntimePanelController::unmount() {
  m_fps_history.clear();
  m_frame_time_history.clear();
  m_chart_sample_elapsed = 0.0;
  m_hovered_fps_bar_index = k_history_capacity;
  m_hovered_frame_time_bar_index = k_history_capacity;
  m_fps_bar_nodes.fill(ui::k_invalid_node_id);
  m_frame_time_bar_nodes.fill(ui::k_invalid_node_id);
  m_fps_line_chart_node = ui::k_invalid_node_id;
  m_frame_time_line_chart_node = ui::k_invalid_node_id;
  m_cpu_history.clear();
  m_memory_history.clear();
  m_latest_cpu_percent = 0.0f;
  m_latest_memory_rss_mb = 0.0f;
  m_latest_binary_size_mb = 0.0f;
  m_binary_size_value_node = ui::k_invalid_node_id;
  m_cpu_value_node = ui::k_invalid_node_id;
  m_memory_value_node = ui::k_invalid_node_id;
  m_cpu_line_chart_node = ui::k_invalid_node_id;
  m_memory_line_chart_node = ui::k_invalid_node_id;
  m_memory_gauge_fill = ui::k_invalid_node_id;
  m_threads_value_node = ui::k_invalid_node_id;
  m_fds_value_node = ui::k_invalid_node_id;
  m_minor_faults_value_node = ui::k_invalid_node_id;
  m_major_faults_value_node = ui::k_invalid_node_id;
  m_voluntary_ctx_value_node = ui::k_invalid_node_id;
  m_involuntary_ctx_value_node = ui::k_invalid_node_id;
  m_disk_read_value_node = ui::k_invalid_node_id;
  m_disk_write_value_node = ui::k_invalid_node_id;
  m_latest_thread_count = 0u;
  m_latest_open_fd_count = 0u;
  m_latest_minor_page_faults = 0u;
  m_latest_major_page_faults = 0u;
  m_latest_voluntary_ctx_switches = 0u;
  m_latest_involuntary_ctx_switches = 0u;
  m_latest_disk_read_bytes = 0.0f;
  m_latest_disk_write_bytes = 0.0f;
  m_heap_history.clear();
  m_latest_heap_used_mb = 0.0f;
  m_latest_mmap_used_mb = 0.0f;
  m_latest_alloc_rate_mb_per_sec = 0.0f;
  m_heap_used_value_node = ui::k_invalid_node_id;
  m_mmap_used_value_node = ui::k_invalid_node_id;
  m_alloc_rate_value_node = ui::k_invalid_node_id;
  m_heap_line_chart_node = ui::k_invalid_node_id;
  m_draw_calls_history.clear();
  m_gpu_time_history.clear();
  m_latest_frame_stats = {};
  m_draw_calls_value_node = ui::k_invalid_node_id;
  m_state_changes_value_node = ui::k_invalid_node_id;
  m_gpu_time_value_node = ui::k_invalid_node_id;
  m_gpu_mem_used_value_node = ui::k_invalid_node_id;
  m_gpu_mem_total_value_node = ui::k_invalid_node_id;
  m_draw_calls_line_chart_node = ui::k_invalid_node_id;
  m_gpu_time_line_chart_node = ui::k_invalid_node_id;
  m_gpu_memory_gauge_fill = ui::k_invalid_node_id;
  m_document = nullptr;
}

void RuntimePanelController::update(const PanelUpdateContext &context) {
  sample_timing(context.dt);
  m_chart_sample_elapsed += context.dt;
  if (m_chart_sample_elapsed >= k_chart_sample_interval) {
    m_chart_sample_elapsed -= k_chart_sample_interval;
    sample_chart_history();
    sample_process_metrics();
    sample_allocator_metrics();
    sample_gpu_metrics();
    refresh_bar_charts();
    refresh_line_charts();
    refresh_process_charts();
    refresh_extended_charts();
  }
  update_chart_tooltips();
  refresh();
}

} // namespace astralix::editor
