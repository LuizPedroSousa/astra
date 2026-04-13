#include "tools/runtime/runtime-panel-controller.hpp"

#include "dsl.hpp"
#include "editor-theme.hpp"
#include "managers/system-manager.hpp"
#include "systems/render-system/render-system.hpp"
#include "trace.hpp"

#include <algorithm>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace astralix::editor {
namespace {

using namespace ui::dsl::styles;

struct MetricCardSpec {
  std::string_view key;
  std::string label;
  std::string body;
  std::string value;
  glm::vec4 value_color = glm::vec4(1.0f);
};

std::string format_integer(size_t value) { return std::to_string(value); }

std::string format_large_integer(size_t value) {
  if (value < 1000u) {
    return std::to_string(value);
  }

  if (value < 1'000'000u) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(1)
           << static_cast<double>(value) / 1000.0 << "k";
    return stream.str();
  }

  std::ostringstream stream;
  stream << std::fixed << std::setprecision(1)
         << static_cast<double>(value) / 1'000'000.0 << "M";
  return stream.str();
}

std::string format_fps(float value, bool has_sample) {
  if (!has_sample) {
    return "--";
  }

  std::ostringstream stream;
  stream << std::fixed << std::setprecision(value >= 100.0f ? 0 : 1) << value;
  return stream.str();
}

std::string format_frame_time_ms(float value, bool has_sample) {
  if (!has_sample) {
    return "--";
  }

  std::ostringstream stream;
  stream << std::fixed << std::setprecision(1) << value << " ms";
  return stream.str();
}

std::string format_cpu_percent(float value) {
  std::ostringstream stream;
  stream << std::fixed << std::setprecision(1) << value << "%";
  return stream.str();
}

std::string format_memory_mb(float value) {
  std::ostringstream stream;
  stream << std::fixed << std::setprecision(1) << value << " MB";
  return stream.str();
}

std::string format_binary_size_mb(float value) {
  std::ostringstream stream;
  stream << std::fixed << std::setprecision(1) << value << " MB";
  return stream.str();
}

std::string format_bytes(float bytes) {
  if (bytes < 1024.0f) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(0) << bytes << " B";
    return stream.str();
  }

  if (bytes < 1024.0f * 1024.0f) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(1) << bytes / 1024.0f << " KB";
    return stream.str();
  }

  if (bytes < 1024.0f * 1024.0f * 1024.0f) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(1)
           << bytes / (1024.0f * 1024.0f) << " MB";
    return stream.str();
  }

  std::ostringstream stream;
  stream << std::fixed << std::setprecision(2)
         << bytes / (1024.0f * 1024.0f * 1024.0f) << " GB";
  return stream.str();
}

std::string format_mb(float value) {
  std::ostringstream stream;
  stream << std::fixed << std::setprecision(1) << value << " MB";
  return stream.str();
}

std::string format_mb_per_sec(float value) {
  std::ostringstream stream;
  stream << std::fixed << std::setprecision(2) << value << " MB/s";
  return stream.str();
}

std::string format_ms(float value) {
  std::ostringstream stream;
  stream << std::fixed << std::setprecision(2) << value << " ms";
  return stream.str();
}

glm::vec4 gauge_fill_color(const RuntimePanelTheme &theme, float ratio) {
  if (ratio <= 0.6f) {
    return theme.gauge_fill_normal;
  }
  if (ratio <= 0.85f) {
    return theme.gauge_fill_warning;
  }
  return theme.gauge_fill_critical;
}

template <size_t Capacity>
std::vector<ui::UILineChartSeries> make_line_chart_series(
    const ui::TimeSeries<Capacity> &history,
    const glm::vec4 &color
) {
  ui::UILineChartSeries series;
  series.color = color;
  series.thickness = 2.0f;
  series.values.reserve(history.size());
  for (size_t i = 0u; i < history.size(); ++i) {
    series.values.push_back(history[i]);
  }

  std::vector<ui::UILineChartSeries> result;
  result.push_back(std::move(series));
  return result;
}

void render_metric_card(
    ui::im::Children &parent,
    std::string_view local_name,
    const MetricCardSpec &spec,
    const RuntimePanelTheme &theme,
    float min_width = 124.0f,
    float value_font_size = 24.0f
) {
  auto card = parent.column(local_name).style(flex(1.0f).min_width(px(min_width)).padding(14.0f).gap(8.0f).radius(16.0f).background(theme.card_background).border(1.0f, theme.card_border).items_start());
  card.text("label", spec.label)
      .style(font_size(12.0f).text_color(theme.text_muted));
  card.text("value", spec.value)
      .style(font_size(value_font_size).text_color(spec.value_color));
  card.text("body", spec.body)
      .style(font_size(11.5f).text_color(theme.text_muted));
}

void render_metric_row(
    ui::im::Children &parent,
    std::string_view local_name,
    std::initializer_list<MetricCardSpec> cards,
    const RuntimePanelTheme &theme,
    float min_width = 124.0f,
    float value_font_size = 24.0f
) {
  auto row_node = parent.row(local_name).style(fill_x().gap(12.0f));
  for (const auto &card : cards) {
    render_metric_card(
        row_node, card.key, card, theme, min_width, value_font_size
    );
  }
}

void render_section_heading(
    ui::im::Children &parent,
    std::string_view local_name,
    std::string title,
    std::string body,
    const RuntimePanelTheme &theme
) {
  auto heading = parent.column(local_name).style(items_start().gap(2.0f));
  heading.text("title", std::move(title))
      .style(font_size(14.0f).text_color(theme.text_primary));
  heading.text("body", std::move(body))
      .style(font_size(12.5f).text_color(theme.text_muted));
}

template <size_t Capacity, typename Formatter>
void render_bar_chart(
    ui::im::Children &parent,
    std::string_view local_name,
    std::string title,
    const ui::TimeSeries<Capacity> &series,
    glm::vec4 bar_color,
    const RuntimePanelTheme &theme,
    Formatter &&formatter
) {
  auto block = parent.column(local_name).style(fill_x().gap(6.0f));
  auto heading = block.row("heading").style(fill_x().items_center());
  heading.text("title", std::move(title))
      .style(font_size(11.5f).text_color(theme.text_muted));
  heading.spacer("spacer");
  heading.text(
             "value", series.empty() ? std::string{} : formatter(series.latest())
  )
      .style(font_size(12.0f).text_color(bar_color));

  auto shell = block.view("shell").style(
      fill_x()
          .height(px(80.0f))
          .padding(4.0f)
          .items_end()
          .background(theme.chart_bar_background)
          .border(1.0f, theme.card_border)
          .radius(10.0f)
          .overflow_hidden()
  );
  auto bars = shell.row("bars").style(fill().items_end().gap(1.0f));

  const float range_max = std::max(series.max_value(), 1.0f);
  const size_t sample_count = series.size();
  const size_t empty_bars = Capacity - sample_count;
  for (size_t i = 0u; i < Capacity; ++i) {
    const bool has_sample = i >= empty_bars;
    const float normalized =
        has_sample ? std::clamp(series[i - empty_bars] / range_max, 0.0f, 1.0f)
                   : 0.0f;
    const float height_pixels =
        has_sample ? std::max(normalized * 72.0f, 1.0f) : 1.0f;

    auto bar = bars.item_scope("bar", i).view("track").style(
        flex(1.0f).min_width(px(0.0f)).items_end().justify_end()
    );
    bar.view("fill").style(
        fill_x()
            .height(px(height_pixels))
            .background(has_sample ? bar_color : theme.chart_bar_background)
            .radius(2.0f)
    );
  }
}

template <size_t Capacity>
void render_line_chart(
    ui::im::Children &parent,
    std::string_view local_name,
    std::string title,
    const ui::TimeSeries<Capacity> &history,
    glm::vec4 color,
    const RuntimePanelTheme &theme
) {
  auto block = parent.column(local_name).style(fill_x().gap(6.0f));
  block.text("title", std::move(title))
      .style(font_size(11.5f).text_color(theme.text_muted));
  block.line_chart("chart")
      .line_chart_grid(4u, theme.line_chart_grid)
      .line_chart_auto_range(true)
      .line_chart_series(make_line_chart_series(history, color))
      .style(
          fill_x()
              .height(px(100.0f))
              .background(theme.line_chart_background)
              .radius(8.0f)
      );
}

void render_gauge(
    ui::im::Children &parent,
    std::string_view local_name,
    std::string label,
    float ratio,
    const RuntimePanelTheme &theme
) {
  const float clamped_ratio = std::clamp(ratio, 0.0f, 1.0f);
  auto block = parent.column(local_name).style(fill_x().gap(4.0f));
  block.text("label", std::move(label))
      .style(font_size(11.5f).text_color(theme.text_muted));
  auto track = block.view("track").style(
      fill_x()
          .height(px(8.0f))
          .background(theme.gauge_track)
          .radius(4.0f)
          .overflow_hidden()
  );
  track.view("fill").style(
      width(percent(clamped_ratio))
          .height(percent(1.0f))
          .background(gauge_fill_color(theme, clamped_ratio))
          .radius(4.0f)
  );
}

} // namespace

void RuntimePanelController::sample_chart_history() {
  if (!m_has_timing_sample) {
    return;
  }

  m_fps_history.push(m_average_fps);
  m_frame_time_history.push(m_average_frame_time_ms);
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

void RuntimePanelController::mount(const PanelMountContext &) {
  m_last_rendered_header_version = std::numeric_limits<uint64_t>::max();
  m_sample_elapsed = 0.0;
  m_sample_frames = 0u;
  m_average_fps = 0.0f;
  m_average_frame_time_ms = 0.0f;
  m_has_timing_sample = false;

  m_fps_history.clear();
  m_frame_time_history.clear();
  m_chart_sample_elapsed = 0.0;
  m_cpu_history.clear();
  m_memory_history.clear();
  m_heap_history.clear();
  m_draw_calls_history.clear();
  m_gpu_time_history.clear();

  m_latest_cpu_percent = 0.0f;
  m_latest_memory_rss_mb = 0.0f;
  m_latest_binary_size_mb = 0.0f;
  m_latest_thread_count = 0u;
  m_latest_open_fd_count = 0u;
  m_latest_minor_page_faults = 0u;
  m_latest_major_page_faults = 0u;
  m_latest_voluntary_ctx_switches = 0u;
  m_latest_involuntary_ctx_switches = 0u;
  m_latest_disk_read_bytes = 0.0f;
  m_latest_disk_write_bytes = 0.0f;
  m_latest_heap_used_mb = 0.0f;
  m_latest_mmap_used_mb = 0.0f;
  m_latest_alloc_rate_mb_per_sec = 0.0f;
  m_latest_frame_stats = {};
  m_latest_snapshot = collect_snapshot();

  sample_process_metrics();
  sample_allocator_metrics();
  sample_gpu_metrics();
  mark_render_dirty();
}

void RuntimePanelController::render(ui::im::Frame &ui) {
  ASTRA_PROFILE_N("RuntimePanel::render");
  const RuntimePanelTheme theme;
  const RuntimeSnapshot &snapshot = m_latest_snapshot;

  auto root = ui.scroll_view("root").style(
      fill_x()
          .flex(1.0f)
          .min_height(px(0.0f))
          .background(theme.shell_background)
          .scroll_vertical()
          .scrollbar_auto()
          .scrollbar_thickness(8.0f)
          .scrollbar_track_color(glm::vec4(theme.card_background.r, theme.card_background.g, theme.card_background.b, 0.92f))
          .scrollbar_thumb_color(glm::vec4(theme.panel_border.r, theme.panel_border.g, theme.panel_border.b, 0.96f))
          .scrollbar_thumb_hovered_color(glm::vec4(theme.accent.r, theme.accent.g, theme.accent.b, 0.78f))
          .scrollbar_thumb_active_color(glm::vec4(theme.accent.r, theme.accent.g, theme.accent.b, 0.86f))
  );
  auto content = root.column("content").style(fill_x().padding(18.0f).gap(14.0f));
  const auto scroll_id = root.widget_id();

  if (auto header = content.row("header")
          .frozen(m_header_version == m_last_rendered_header_version)
          .style(fill_x().items_center().gap(12.0f))) {
    m_last_rendered_header_version = m_header_version;
    auto title = header.column("title").style(items_start().gap(3.0f));
    title.text("heading", "Runtime")
        .style(font_size(20.0f).text_color(theme.text_primary));
    title.text(
             "body",
             "Live counters for the active scene, renderer, physics, and process."
    )
        .style(font_size(13.0f).text_color(theme.text_muted));
    header.spacer("spacer");
    auto scene_status = header.column("scene-status").style(items_end().gap(6.0f));
    const glm::vec4 status_color = snapshot.has_scene ? theme.success
                                                      : theme.text_muted;
    auto pill = scene_status.view("pill").style(
        items_center()
            .padding_xy(12.0f, 8.0f)
            .radius(999.0f)
            .background(glm::vec4(status_color.r, status_color.g, status_color.b, 0.16f))
            .border(1.0f, glm::vec4(status_color.r, status_color.g, status_color.b, 0.44f))
    );
    pill.text("label", snapshot.has_scene ? "ACTIVE" : "IDLE")
        .style(font_size(12.5f).text_color(status_color));
    scene_status.text(
                    "scene-name",
                    snapshot.has_scene ? snapshot.scene_name : std::string("No active scene")
    )
        .style(font_size(13.0f).text_color(theme.text_muted));
  }

  if (auto timing = content.lazy_section("timing", scroll_id, 600.0f)) {
    ASTRA_PROFILE_N("RuntimePanel::timing_charts");
    timing.style(fill_x().gap(14.0f));
    render_metric_row(
        timing,
        "summary-row",
        {
            MetricCardSpec{
                .key = "fps",
                .label = "Frame Rate",
                .body = "Average FPS over the last 0.25s.",
                .value = format_fps(m_average_fps, m_has_timing_sample),
                .value_color = theme.accent,
            },
            MetricCardSpec{
                .key = "frame-time",
                .label = "Frame Time",
                .body = "Average milliseconds per frame.",
                .value = format_frame_time_ms(
                    m_average_frame_time_ms, m_has_timing_sample
                ),
                .value_color = theme.text_primary,
            },
        },
        theme,
        180.0f,
        32.0f
    );

    render_bar_chart(
        timing,
        "fps-history",
        "FPS History",
        m_fps_history,
        theme.chart_bar_fill,
        theme,
        [](float value) { return format_fps(value, true); }
    );

    render_bar_chart(
        timing,
        "frame-time-history",
        "Frame Time History",
        m_frame_time_history,
        theme.chart_bar_fill_alt,
        theme,
        [](float value) { return format_frame_time_ms(value, true); }
    );
    render_line_chart(
        timing,
        "fps-line-chart",
        "FPS Trend",
        m_fps_history,
        theme.line_chart_fps_line,
        theme
    );
    render_line_chart(
        timing,
        "frame-time-line-chart",
        "Frame Time Trend",
        m_frame_time_history,
        theme.line_chart_frame_time_line,
        theme
    );
  }

  if (auto process_section = content.lazy_section("process-section", scroll_id, 450.0f)) {
    ASTRA_PROFILE_N("RuntimePanel::process");
    process_section.style(fill_x().gap(10.0f));
    render_section_heading(
        process_section,
        "heading",
        "Process",
        "CPU and memory usage of the running process.",
        theme
    );
    render_metric_row(
        process_section,
        "cards",
        {
            MetricCardSpec{
                .key = "cpu",
                .label = "CPU Usage",
                .body = "Process CPU utilization.",
                .value = format_cpu_percent(m_latest_cpu_percent),
                .value_color = theme.line_chart_cpu_line,
            },
            MetricCardSpec{
                .key = "memory",
                .label = "Memory (RSS)",
                .body = "Resident set size of the process.",
                .value = format_memory_mb(m_latest_memory_rss_mb),
                .value_color = theme.line_chart_memory_line,
            },
            MetricCardSpec{
                .key = "binary-size",
                .label = "Binary Size",
                .body = "On-disk size of the executable.",
                .value = format_binary_size_mb(m_latest_binary_size_mb),
                .value_color = theme.text_primary,
            },
        },
        theme
    );
    render_line_chart(
        process_section,
        "cpu-trend",
        "CPU Trend",
        m_cpu_history,
        theme.line_chart_cpu_line,
        theme
    );
    render_line_chart(
        process_section,
        "memory-trend",
        "Memory Trend",
        m_memory_history,
        theme.line_chart_memory_line,
        theme
    );
    render_gauge(
        process_section,
        "memory-gauge",
        "Memory budget (2 GB)",
        m_latest_memory_rss_mb / 2048.0f,
        theme
    );
  }

  if (auto process_details = content.lazy_section("process-details", scroll_id, 350.0f)) {
    ASTRA_PROFILE_N("RuntimePanel::process_details");
    process_details.style(fill_x().gap(10.0f));
    render_section_heading(
        process_details,
        "heading",
        "Process Details",
        "Threads, file descriptors, page faults, and context switches.",
        theme
    );
    render_metric_row(
        process_details,
        "top-row",
        {
            MetricCardSpec{
                .key = "threads",
                .label = "Threads",
                .body = "Active OS threads.",
                .value = format_integer(m_latest_thread_count),
                .value_color = theme.text_primary,
            },
            MetricCardSpec{
                .key = "fds",
                .label = "Open FDs",
                .body = "File descriptors in use.",
                .value = format_integer(m_latest_open_fd_count),
                .value_color = theme.text_primary,
            },
        },
        theme
    );
    render_metric_row(
        process_details,
        "middle-row",
        {
            MetricCardSpec{
                .key = "minor-faults",
                .label = "Minor Faults",
                .body = "Pages resolved from cache.",
                .value = format_large_integer(m_latest_minor_page_faults),
                .value_color = theme.text_primary,
            },
            MetricCardSpec{
                .key = "major-faults",
                .label = "Major Faults",
                .body = "Pages loaded from disk.",
                .value = format_large_integer(m_latest_major_page_faults),
                .value_color = theme.accent,
            },
        },
        theme
    );
    render_metric_row(
        process_details,
        "bottom-row",
        {
            MetricCardSpec{
                .key = "voluntary",
                .label = "Vol. Ctx Sw",
                .body = "Voluntary context switches.",
                .value = format_large_integer(m_latest_voluntary_ctx_switches),
                .value_color = theme.text_primary,
            },
            MetricCardSpec{
                .key = "involuntary",
                .label = "Invol. Ctx Sw",
                .body = "Involuntary context switches.",
                .value = format_large_integer(m_latest_involuntary_ctx_switches),
                .value_color = theme.text_primary,
            },
        },
        theme
    );
  }

  if (auto io_section = content.lazy_section("io-section", scroll_id, 200.0f)) {
    ASTRA_PROFILE_N("RuntimePanel::io");
    io_section.style(fill_x().gap(10.0f));
    render_section_heading(io_section, "heading", "I/O", "Disk read and write activity.", theme);
    render_metric_row(
        io_section,
        "cards",
        {
            MetricCardSpec{
                .key = "disk-read",
                .label = "Disk Read",
                .body = "Cumulative bytes read.",
                .value = format_bytes(m_latest_disk_read_bytes),
                .value_color = theme.text_primary,
            },
            MetricCardSpec{
                .key = "disk-write",
                .label = "Disk Write",
                .body = "Cumulative bytes written.",
                .value = format_bytes(m_latest_disk_write_bytes),
                .value_color = theme.accent,
            },
        },
        theme
    );
  }

  if (auto allocator_section = content.lazy_section("allocator", scroll_id, 300.0f)) {
    ASTRA_PROFILE_N("RuntimePanel::allocator");
    allocator_section.style(fill_x().gap(10.0f));
    render_section_heading(
        allocator_section,
        "heading",
        "Allocator",
        "Heap and mmap usage from the C allocator.",
        theme
    );
    render_metric_row(
        allocator_section,
        "cards",
        {
            MetricCardSpec{
                .key = "heap-used",
                .label = "Heap Used",
                .body = "In-use heap allocations.",
                .value = format_mb(m_latest_heap_used_mb),
                .value_color = theme.line_chart_heap_line,
            },
            MetricCardSpec{
                .key = "mmap-used",
                .label = "mmap'd",
                .body = "Memory-mapped allocations.",
                .value = format_mb(m_latest_mmap_used_mb),
                .value_color = theme.text_primary,
            },
            MetricCardSpec{
                .key = "alloc-rate",
                .label = "Alloc Rate",
                .body = "Heap growth per second.",
                .value = format_mb_per_sec(m_latest_alloc_rate_mb_per_sec),
                .value_color = theme.text_primary,
            },
        },
        theme
    );
    render_line_chart(
        allocator_section,
        "heap-trend",
        "Heap Trend",
        m_heap_history,
        theme.line_chart_heap_line,
        theme
    );
  }

  if (auto gpu_section = content.lazy_section("gpu", scroll_id, 500.0f)) {
    ASTRA_PROFILE_N("RuntimePanel::gpu");
    gpu_section.style(fill_x().gap(10.0f));
    render_section_heading(
        gpu_section,
        "heading",
        "GPU",
        "Draw calls, state changes, and video memory.",
        theme
    );
    render_metric_row(
        gpu_section,
        "top-row",
        {
            MetricCardSpec{
                .key = "draw-calls",
                .label = "Draw Calls",
                .body = "GL draw commands per frame.",
                .value = format_integer(m_latest_frame_stats.draw_call_count),
                .value_color = theme.line_chart_draw_calls_line,
            },
            MetricCardSpec{
                .key = "state-changes",
                .label = "State Changes",
                .body = "GL state mutations per frame.",
                .value = format_integer(m_latest_frame_stats.state_change_count),
                .value_color = theme.text_primary,
            },
            MetricCardSpec{
                .key = "gpu-time",
                .label = "GPU Time",
                .body = "GPU-side frame duration.",
                .value = format_ms(m_latest_frame_stats.gpu_frame_time_ms),
                .value_color = theme.line_chart_gpu_time_line,
            },
        },
        theme
    );
    render_metric_row(
        gpu_section,
        "bottom-row",
        {
            MetricCardSpec{
                .key = "gpu-used",
                .label = "GPU Mem Used",
                .body = "Video memory in use.",
                .value = format_mb(m_latest_frame_stats.gpu_memory_used_mb),
                .value_color = theme.accent,
            },
            MetricCardSpec{
                .key = "gpu-total",
                .label = "GPU Mem Total",
                .body = "Total video memory.",
                .value = format_mb(m_latest_frame_stats.gpu_memory_total_mb),
                .value_color = theme.text_primary,
            },
        },
        theme
    );
    render_line_chart(
        gpu_section,
        "draw-calls-trend",
        "Draw Calls Trend",
        m_draw_calls_history,
        theme.line_chart_draw_calls_line,
        theme
    );
    render_line_chart(
        gpu_section,
        "gpu-time-trend",
        "GPU Time Trend",
        m_gpu_time_history,
        theme.line_chart_gpu_time_line,
        theme
    );
    if (m_latest_frame_stats.gpu_memory_total_mb > 0.0f) {
      render_gauge(
          gpu_section,
          "gpu-memory-gauge",
          "GPU memory budget",
          m_latest_frame_stats.gpu_memory_used_mb /
              m_latest_frame_stats.gpu_memory_total_mb,
          theme
      );
    }
  }

  if (auto scene_block = content.lazy_section("scene-block", scroll_id, 900.0f).visible(snapshot.has_scene)) {
    ASTRA_PROFILE_N("RuntimePanel::scene_metrics");
    scene_block.style(fill_x().gap(12.0f));

    auto scene_section = scene_block.column("scene-section").style(fill_x().gap(10.0f));
    render_section_heading(
        scene_section,
        "heading",
        "Scene",
        "High-level footprint of the active world.",
        theme
    );
    render_metric_row(
        scene_section,
        "cards",
        {
            MetricCardSpec{
                .key = "entities",
                .label = "Entities",
                .body = "Scene entities tracked by the world.",
                .value = format_integer(snapshot.entity_count),
                .value_color = theme.text_primary,
            },
            MetricCardSpec{
                .key = "renderables",
                .label = "Renderables",
                .body = "Objects currently tagged for rendering.",
                .value = format_integer(snapshot.renderable_count),
                .value_color = theme.accent,
            },
        },
        theme
    );
    render_gauge(
        scene_section,
        "entity-gauge",
        "Entity budget",
        static_cast<float>(snapshot.entity_count) / 10'000.0f,
        theme
    );
    render_gauge(
        scene_section,
        "renderable-gauge",
        "Renderable budget",
        static_cast<float>(snapshot.renderable_count) / 5'000.0f,
        theme
    );

    auto physics = scene_block.column("physics").style(fill_x().gap(10.0f));
    render_section_heading(
        physics,
        "heading",
        "Physics",
        "Rigid-body totals by simulation mode.",
        theme
    );
    render_metric_row(
        physics,
        "cards",
        {
            MetricCardSpec{
                .key = "rigid-bodies",
                .label = "Rigid Bodies",
                .body = "All bodies registered with physics.",
                .value = format_integer(snapshot.rigid_body_count),
                .value_color = theme.text_primary,
            },
            MetricCardSpec{
                .key = "dynamic-bodies",
                .label = "Dynamic",
                .body = "Bodies simulated every frame.",
                .value = format_integer(snapshot.dynamic_body_count),
                .value_color = theme.accent,
            },
            MetricCardSpec{
                .key = "static-bodies",
                .label = "Static",
                .body = "Bodies used as immovable collision.",
                .value = format_integer(snapshot.static_body_count),
                .value_color = theme.text_primary,
            },
        },
        theme
    );

    auto systems = scene_block.column("systems").style(fill_x().gap(10.0f));
    render_section_heading(
        systems,
        "heading",
        "Systems",
        "Core runtime subsystems visible in the scene.",
        theme
    );
    render_metric_row(
        systems,
        "cards",
        {
            MetricCardSpec{
                .key = "lights",
                .label = "Lights",
                .body = "Directional, point, and spot lights.",
                .value = format_integer(snapshot.light_count),
                .value_color = theme.accent,
            },
            MetricCardSpec{
                .key = "cameras",
                .label = "Main Cameras",
                .body = "Active view-driving camera tags.",
                .value = format_integer(snapshot.camera_count),
                .value_color = theme.text_primary,
            },
            MetricCardSpec{
                .key = "ui-roots",
                .label = "UI Roots",
                .body = "Top-level UI documents in the world.",
                .value = format_integer(snapshot.ui_root_count),
                .value_color = theme.text_primary,
            },
        },
        theme
    );

    auto geometry = scene_block.column("geometry").style(fill_x().gap(10.0f));
    render_section_heading(
        geometry,
        "heading",
        "Geometry",
        "Mesh budget across all renderables.",
        theme
    );
    render_metric_row(
        geometry,
        "cards",
        {
            MetricCardSpec{
                .key = "vertices",
                .label = "Vertices",
                .body = "Total vertices across all meshes.",
                .value = format_large_integer(snapshot.vertex_count),
                .value_color = theme.text_primary,
            },
            MetricCardSpec{
                .key = "triangles",
                .label = "Triangles",
                .body = "Total triangles drawn per frame.",
                .value = format_large_integer(snapshot.triangle_count),
                .value_color = theme.accent,
            },
            MetricCardSpec{
                .key = "shadow-casters",
                .label = "Shadow Casters",
                .body = "Entities casting shadows.",
                .value = format_integer(snapshot.shadow_caster_count),
                .value_color = theme.text_primary,
            },
        },
        theme
    );
    render_gauge(
        geometry,
        "triangle-gauge",
        "Triangle budget",
        static_cast<float>(snapshot.triangle_count) / 2'000'000.0f,
        theme
    );

    auto resources = scene_block.column("resources").style(fill_x().gap(10.0f));
    render_section_heading(
        resources,
        "heading",
        "Resources",
        "GPU-resident asset pools.",
        theme
    );
    render_metric_row(
        resources,
        "top-row",
        {
            MetricCardSpec{
                .key = "textures",
                .label = "Textures",
                .body = "2D and 3D textures loaded.",
                .value = format_integer(snapshot.texture_count),
                .value_color = theme.text_primary,
            },
            MetricCardSpec{
                .key = "shaders",
                .label = "Shaders",
                .body = "Compiled shader programs.",
                .value = format_integer(snapshot.shader_count),
                .value_color = theme.accent,
            },
        },
        theme
    );
    render_metric_row(
        resources,
        "bottom-row",
        {
            MetricCardSpec{
                .key = "materials",
                .label = "Materials",
                .body = "Material instances in use.",
                .value = format_integer(snapshot.material_count),
                .value_color = theme.text_primary,
            },
            MetricCardSpec{
                .key = "models",
                .label = "Models",
                .body = "Loaded model assets.",
                .value = format_integer(snapshot.model_count),
                .value_color = theme.text_primary,
            },
        },
        theme
    );
  }

  if (auto empty = content.view("empty-state").visible(!snapshot.has_scene)) {
    empty.style(fill_x().padding(18.0f).gap(6.0f).radius(18.0f).background(theme.panel_background).border(1.0f, theme.panel_border).items_start());
    empty.text("title", "No active scene")
        .style(font_size(18.0f).text_color(theme.text_primary));
    empty.text(
             "body",
             "Scene-specific counters appear here when SceneManager exposes an active scene."
    )
        .style(font_size(13.0f).text_color(theme.text_muted));
  }
}

std::optional<uint64_t> RuntimePanelController::render_version() const {
  return m_render_revision;
}

void RuntimePanelController::unmount() {
  m_last_rendered_header_version = std::numeric_limits<uint64_t>::max();
  m_fps_history.clear();
  m_frame_time_history.clear();
  m_chart_sample_elapsed = 0.0;
  m_cpu_history.clear();
  m_memory_history.clear();
  m_heap_history.clear();
  m_draw_calls_history.clear();
  m_gpu_time_history.clear();
  m_sample_elapsed = 0.0;
  m_sample_frames = 0u;
  m_average_fps = 0.0f;
  m_average_frame_time_ms = 0.0f;
  m_has_timing_sample = false;
  m_latest_cpu_percent = 0.0f;
  m_latest_memory_rss_mb = 0.0f;
  m_latest_binary_size_mb = 0.0f;
  m_latest_thread_count = 0u;
  m_latest_open_fd_count = 0u;
  m_latest_minor_page_faults = 0u;
  m_latest_major_page_faults = 0u;
  m_latest_voluntary_ctx_switches = 0u;
  m_latest_involuntary_ctx_switches = 0u;
  m_latest_disk_read_bytes = 0.0f;
  m_latest_disk_write_bytes = 0.0f;
  m_latest_heap_used_mb = 0.0f;
  m_latest_mmap_used_mb = 0.0f;
  m_latest_alloc_rate_mb_per_sec = 0.0f;
  m_latest_frame_stats = {};
  m_latest_snapshot = {};
  mark_render_dirty();
}

void RuntimePanelController::update(const PanelUpdateContext &context) {
  sample_timing(context.dt);
  m_chart_sample_elapsed += context.dt;
  if (m_chart_sample_elapsed < k_chart_sample_interval) {
    return;
  }

  m_chart_sample_elapsed -= k_chart_sample_interval;
  sample_chart_history();
  sample_process_metrics();
  sample_allocator_metrics();
  sample_gpu_metrics();
  const RuntimeSnapshot previous_snapshot = m_latest_snapshot;
  m_latest_snapshot = collect_snapshot();
  if (m_latest_snapshot.has_scene != previous_snapshot.has_scene ||
      m_latest_snapshot.scene_name != previous_snapshot.scene_name) {
    ++m_header_version;
  }
  mark_render_dirty();
}

} // namespace astralix::editor
