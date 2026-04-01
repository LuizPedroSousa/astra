#include "tools/runtime/runtime-panel-controller.hpp"

#include "dsl/widgets/data-viz/linear-gauge.hpp"
#include "editor-theme.hpp"
#include "managers/system-manager.hpp"
#include "systems/render-system/render-system.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <utility>
#include <vector>

namespace astralix::editor {

void RuntimePanelController::sample_chart_history() {
  if (!m_has_timing_sample) {
    return;
  }

  m_fps_history.push(m_average_fps);
  m_frame_time_history.push(m_average_frame_time_ms);
}

void RuntimePanelController::refresh_bar_charts() {
  if (m_document == nullptr) {
    return;
  }

  const auto update_bars =
      [this](
          const ui::TimeSeries<k_history_capacity> &series,
          const std::array<ui::UINodeId, k_history_capacity> &bar_nodes
      ) {
        const float range_max = std::max(series.max_value(), 1.0f);
        const size_t sample_count = series.size();
        const size_t empty_bars = k_history_capacity - sample_count;

        for (size_t i = 0u; i < k_history_capacity; ++i) {
          if (bar_nodes[i] == ui::k_invalid_node_id) {
            continue;
          }

          float normalized = 0.0f;
          if (i >= empty_bars) {
            const size_t sample_index = i - empty_bars;
            normalized =
                std::clamp(series[sample_index] / range_max, 0.0f, 1.0f);
          }

          constexpr float k_chart_height = 80.0f;
          const float target_pixels =
              std::max(normalized * k_chart_height, 1.0f);
          m_document->mutate_style(
              bar_nodes[i],
              [target_pixels](ui::UIStyle &style) {
                style.height = ui::UILength::pixels(target_pixels);
              }
          );
        }
      };

  update_bars(m_fps_history, m_fps_bar_nodes);
  update_bars(m_frame_time_history, m_frame_time_bar_nodes);
}

void RuntimePanelController::refresh_gauges(const RuntimeSnapshot &snapshot) {
  if (m_document == nullptr) {
    return;
  }

  const RuntimePanelTheme theme;
  const std::vector<ui::dsl::GaugeThreshold> thresholds = {
      {.limit = 0.6f, .color = theme.gauge_fill_normal},
      {.limit = 0.85f, .color = theme.gauge_fill_warning},
      {.limit = 1.0f, .color = theme.gauge_fill_critical},
  };

  constexpr size_t k_entity_budget = 10'000u;
  constexpr size_t k_renderable_budget = 5'000u;
  constexpr size_t k_triangle_budget = 2'000'000u;

  auto update_gauge =
      [this, &thresholds](ui::UINodeId fill_node, float ratio) {
        if (fill_node == ui::k_invalid_node_id) {
          return;
        }

        const float clamped_ratio = std::clamp(ratio, 0.0f, 1.0f);
        const glm::vec4 fill_color =
            ui::dsl::gauge_color_for_ratio(clamped_ratio, thresholds);

        m_document->mutate_style(
            fill_node,
            [clamped_ratio, fill_color](ui::UIStyle &style) {
              style.width = ui::UILength::percent(clamped_ratio);
              style.background_color = fill_color;
            }
        );
      };

  update_gauge(
      m_entities_gauge_fill,
      static_cast<float>(snapshot.entity_count) /
          static_cast<float>(k_entity_budget)
  );
  update_gauge(
      m_renderables_gauge_fill,
      static_cast<float>(snapshot.renderable_count) /
          static_cast<float>(k_renderable_budget)
  );
  update_gauge(
      m_triangles_gauge_fill,
      static_cast<float>(snapshot.triangle_count) /
          static_cast<float>(k_triangle_budget)
  );
}

void RuntimePanelController::refresh_line_charts() {
  if (m_document == nullptr) {
    return;
  }

  const RuntimePanelTheme theme;

  auto build_series = [](const ui::TimeSeries<k_history_capacity> &history,
                         const glm::vec4 &color,
                         float thickness) -> ui::UILineChartSeries {
    ui::UILineChartSeries series;
    series.color = color;
    series.thickness = thickness;
    series.values.reserve(history.size());
    for (size_t i = 0u; i < history.size(); ++i) {
      series.values.push_back(history[i]);
    }
    return series;
  };

  if (m_fps_line_chart_node != ui::k_invalid_node_id) {
    std::vector<ui::UILineChartSeries> fps_series;
    fps_series.push_back(
        build_series(m_fps_history, theme.line_chart_fps_line, 2.0f)
    );
    m_document->set_line_chart_series(m_fps_line_chart_node, std::move(fps_series));
  }

  if (m_frame_time_line_chart_node != ui::k_invalid_node_id) {
    std::vector<ui::UILineChartSeries> frame_time_series;
    frame_time_series.push_back(
        build_series(m_frame_time_history, theme.line_chart_frame_time_line, 2.0f)
    );
    m_document->set_line_chart_series(
        m_frame_time_line_chart_node, std::move(frame_time_series)
    );
  }
}

void RuntimePanelController::sample_process_metrics() {
  const ProcessMetrics metrics = m_process_sampler.sample();
  m_latest_cpu_percent = metrics.cpu_usage_percent;
  m_latest_memory_rss_mb = metrics.memory_rss_mb;
  m_latest_binary_size_mb = metrics.binary_size_mb;
  m_latest_thread_count = metrics.thread_count;
  m_latest_open_fd_count = metrics.open_fd_count;
  m_latest_minor_page_faults = metrics.minor_page_faults;
  m_latest_major_page_faults = metrics.major_page_faults;
  m_latest_voluntary_ctx_switches = metrics.voluntary_context_switches;
  m_latest_involuntary_ctx_switches = metrics.involuntary_context_switches;
  m_latest_disk_read_bytes = metrics.disk_read_bytes;
  m_latest_disk_write_bytes = metrics.disk_write_bytes;
  m_cpu_history.push(m_latest_cpu_percent);
  m_memory_history.push(m_latest_memory_rss_mb);
}

void RuntimePanelController::refresh_process_charts() {
  if (m_document == nullptr) {
    return;
  }

  const RuntimePanelTheme theme;

  if (m_cpu_line_chart_node != ui::k_invalid_node_id) {
    ui::UILineChartSeries cpu_series;
    cpu_series.color = theme.line_chart_cpu_line;
    cpu_series.thickness = 2.0f;
    cpu_series.values.reserve(m_cpu_history.size());
    for (size_t i = 0u; i < m_cpu_history.size(); ++i) {
      cpu_series.values.push_back(m_cpu_history[i]);
    }
    std::vector<ui::UILineChartSeries> series;
    series.push_back(std::move(cpu_series));
    m_document->set_line_chart_series(
        m_cpu_line_chart_node, std::move(series)
    );
  }

  if (m_memory_line_chart_node != ui::k_invalid_node_id) {
    ui::UILineChartSeries memory_series;
    memory_series.color = theme.line_chart_memory_line;
    memory_series.thickness = 2.0f;
    memory_series.values.reserve(m_memory_history.size());
    for (size_t i = 0u; i < m_memory_history.size(); ++i) {
      memory_series.values.push_back(m_memory_history[i]);
    }
    std::vector<ui::UILineChartSeries> series;
    series.push_back(std::move(memory_series));
    m_document->set_line_chart_series(
        m_memory_line_chart_node, std::move(series)
    );
  }

  if (m_memory_gauge_fill != ui::k_invalid_node_id) {
    constexpr float k_memory_budget_mb = 2048.0f;
    const float ratio =
        std::clamp(m_latest_memory_rss_mb / k_memory_budget_mb, 0.0f, 1.0f);

    const std::vector<ui::dsl::GaugeThreshold> thresholds = {
        {.limit = 0.6f, .color = theme.gauge_fill_normal},
        {.limit = 0.85f, .color = theme.gauge_fill_warning},
        {.limit = 1.0f, .color = theme.gauge_fill_critical},
    };
    const glm::vec4 fill_color =
        ui::dsl::gauge_color_for_ratio(ratio, thresholds);

    m_document->mutate_style(
        m_memory_gauge_fill,
        [ratio, fill_color](ui::UIStyle &style) {
          style.width = ui::UILength::percent(ratio);
          style.background_color = fill_color;
        }
    );
  }
}

void RuntimePanelController::update_chart_tooltips() {
  if (m_document == nullptr) {
    return;
  }

  const auto format_value = [](float value, const char *unit) -> std::string {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(1) << value << " " << unit;
    return stream.str();
  };

  const auto resolve_hover_label =
      [this, &format_value](
          size_t hovered_index,
          ui::UINodeId label_node,
          const std::array<ui::UINodeId, k_history_capacity> &bar_nodes,
          const ui::TimeSeries<k_history_capacity> &series,
          const char *unit
      ) {
        if (label_node == ui::k_invalid_node_id) {
          return;
        }

        const ui::UINodeId hot = m_document->hot_node();
        bool bar_is_hovered = false;
        for (size_t i = 0u; i < k_history_capacity; ++i) {
          if (bar_nodes[i] != ui::k_invalid_node_id && bar_nodes[i] == hot) {
            bar_is_hovered = true;
            break;
          }
        }

        if (!bar_is_hovered || hovered_index >= k_history_capacity) {
          m_document->set_text(label_node, {});
          return;
        }

        const size_t sample_count = series.size();
        const size_t empty_bars = k_history_capacity - sample_count;
        if (hovered_index < empty_bars) {
          m_document->set_text(label_node, {});
          return;
        }

        const size_t sample_index = hovered_index - empty_bars;
        m_document->set_text(
            label_node, format_value(series[sample_index], unit)
        );
      };

  resolve_hover_label(
      m_hovered_fps_bar_index,
      m_fps_tooltip_text_node,
      m_fps_bar_nodes,
      m_fps_history,
      "fps"
  );

  resolve_hover_label(
      m_hovered_frame_time_bar_index,
      m_frame_time_tooltip_text_node,
      m_frame_time_bar_nodes,
      m_frame_time_history,
      "ms"
  );
}

void RuntimePanelController::sample_extended_process_metrics() {
}

void RuntimePanelController::sample_allocator_metrics() {
  const AllocatorMetrics metrics =
      m_allocator_sampler.sample(k_chart_sample_interval);
  m_latest_heap_used_mb = metrics.heap_used_mb;
  m_latest_mmap_used_mb = metrics.mmap_used_mb;
  m_latest_alloc_rate_mb_per_sec = metrics.allocation_rate_mb_per_sec;
  m_heap_history.push(m_latest_heap_used_mb);
}

void RuntimePanelController::sample_gpu_metrics() {
  auto system_manager = SystemManager::get();
  if (system_manager == nullptr) {
    return;
  }

  auto *render_system = system_manager->get_system<RenderSystem>();
  if (render_system == nullptr) {
    return;
  }

  m_latest_frame_stats = render_system->current_frame_stats();
  m_draw_calls_history.push(
      static_cast<float>(m_latest_frame_stats.draw_call_count)
  );
  m_gpu_time_history.push(m_latest_frame_stats.gpu_frame_time_ms);
}

void RuntimePanelController::refresh_extended_charts() {
  if (m_document == nullptr) {
    return;
  }

  const RuntimePanelTheme theme;

  auto build_series = [](const ui::TimeSeries<k_history_capacity> &history,
                         const glm::vec4 &color,
                         float thickness) -> ui::UILineChartSeries {
    ui::UILineChartSeries series;
    series.color = color;
    series.thickness = thickness;
    series.values.reserve(history.size());
    for (size_t i = 0u; i < history.size(); ++i) {
      series.values.push_back(history[i]);
    }
    return series;
  };

  if (m_heap_line_chart_node != ui::k_invalid_node_id) {
    std::vector<ui::UILineChartSeries> heap_series;
    heap_series.push_back(
        build_series(m_heap_history, theme.line_chart_heap_line, 2.0f)
    );
    m_document->set_line_chart_series(
        m_heap_line_chart_node, std::move(heap_series)
    );
  }

  if (m_draw_calls_line_chart_node != ui::k_invalid_node_id) {
    std::vector<ui::UILineChartSeries> draw_series;
    draw_series.push_back(
        build_series(m_draw_calls_history, theme.line_chart_draw_calls_line, 2.0f)
    );
    m_document->set_line_chart_series(
        m_draw_calls_line_chart_node, std::move(draw_series)
    );
  }

  if (m_gpu_time_line_chart_node != ui::k_invalid_node_id) {
    std::vector<ui::UILineChartSeries> gpu_time_series;
    gpu_time_series.push_back(
        build_series(m_gpu_time_history, theme.line_chart_gpu_time_line, 2.0f)
    );
    m_document->set_line_chart_series(
        m_gpu_time_line_chart_node, std::move(gpu_time_series)
    );
  }

  if (m_gpu_memory_gauge_fill != ui::k_invalid_node_id &&
      m_latest_frame_stats.gpu_memory_total_mb > 0.0f) {
    const float ratio = std::clamp(
        m_latest_frame_stats.gpu_memory_used_mb /
            m_latest_frame_stats.gpu_memory_total_mb,
        0.0f,
        1.0f
    );

    const std::vector<ui::dsl::GaugeThreshold> thresholds = {
        {.limit = 0.6f, .color = theme.gauge_fill_normal},
        {.limit = 0.85f, .color = theme.gauge_fill_warning},
        {.limit = 1.0f, .color = theme.gauge_fill_critical},
    };
    const glm::vec4 fill_color =
        ui::dsl::gauge_color_for_ratio(ratio, thresholds);

    m_document->mutate_style(
        m_gpu_memory_gauge_fill,
        [ratio, fill_color](ui::UIStyle &style) {
          style.width = ui::UILength::percent(ratio);
          style.background_color = fill_color;
        }
    );
  }
}

} // namespace astralix::editor
