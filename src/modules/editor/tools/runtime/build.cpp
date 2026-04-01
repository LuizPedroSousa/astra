#include "tools/runtime/build.hpp"

#include "tools/runtime/runtime-panel-controller.hpp"

#include <utility>

namespace astralix::editor {

using namespace ui::dsl;
using namespace ui::dsl::styles;

namespace {

StyleBuilder summary_card_style(const RuntimePanelTheme &theme) {
  return flex(1.0f)
      .padding(16.0f)
      .gap(10.0f)
      .radius(18.0f)
      .background(theme.panel_background)
      .border(1.0f, theme.panel_border)
      .items_start()
      .raw([](ui::UIStyle &style) {
        style.min_width = ui::UILength::pixels(180.0f);
      });
}

StyleBuilder metric_card_style(const RuntimePanelTheme &theme) {
  return flex(1.0f)
      .padding(14.0f)
      .gap(8.0f)
      .radius(16.0f)
      .background(theme.card_background)
      .border(1.0f, theme.card_border)
      .items_start()
      .raw([](ui::UIStyle &style) {
        style.min_width = ui::UILength::pixels(124.0f);
      });
}

NodeSpec section_heading(
    const char *title,
    const char *body,
    const RuntimePanelTheme &theme
) {
  return ui::dsl::column()
      .style(items_start().gap(2.0f))
      .children(
          text(title)
              .style(font_size(14.0f).text_color(theme.text_primary)),
          text(body)
              .style(font_size(12.5f).text_color(theme.text_muted))
      );
}

NodeSpec summary_card(
    const char *label,
    const char *body,
    ui::UINodeId &value_node,
    const glm::vec4 &value_color,
    const RuntimePanelTheme &theme
) {
  return ui::dsl::column()
      .style(summary_card_style(theme))
      .children(
          text(label)
              .style(font_size(12.5f).text_color(theme.text_muted)),
          text("--")
              .bind(value_node)
              .style(font_size(32.0f).text_color(value_color)),
          text(body)
              .style(font_size(12.0f).text_color(theme.text_muted))
      );
}

NodeSpec metric_card(
    const char *label,
    const char *body,
    ui::UINodeId &value_node,
    const glm::vec4 &value_color,
    const RuntimePanelTheme &theme
) {
  return ui::dsl::column()
      .style(metric_card_style(theme))
      .children(
          text(label)
              .style(font_size(12.0f).text_color(theme.text_muted)),
          text("--")
              .bind(value_node)
              .style(font_size(24.0f).text_color(value_color)),
          text(body)
              .style(font_size(11.5f).text_color(theme.text_muted))
      );
}

NodeSpec gauge_row(
    const char *label,
    ui::UINodeId &gauge_fill_node,
    const RuntimePanelTheme &theme
) {
  return ui::dsl::column()
      .style(fill_x().gap(4.0f))
      .children(
          text(label)
              .style(font_size(11.5f).text_color(theme.text_muted)),
          linear_gauge(
              ui::dsl::LinearGaugeStyle{
                  .height = 8.0f,
                  .border_radius = 4.0f,
                  .track_color = theme.gauge_track,
                  .thresholds =
                      {
                          {.limit = 0.6f, .color = theme.gauge_fill_normal},
                          {.limit = 0.85f, .color = theme.gauge_fill_warning},
                          {.limit = 1.0f, .color = theme.gauge_fill_critical},
                      },
              },
              gauge_fill_node
          )
      );
}

} // namespace

namespace runtime_panel {

ui::dsl::NodeSpec build_header(
    ui::UINodeId &scene_status_chip_node,
    ui::UINodeId &scene_status_text_node,
    ui::UINodeId &scene_name_node,
    const RuntimePanelTheme &theme
) {
  return ui::dsl::row()
      .style(fill_x().items_center().gap(12.0f))
      .children(
          ui::dsl::column()
              .style(items_start().gap(3.0f))
              .children(
                  text("Runtime")
                      .style(font_size(20.0f).text_color(theme.text_primary)),
                  text("Live counters for the active scene, renderer, physics, and UI state")
                      .style(font_size(13.0f).text_color(theme.text_muted))
              ),
          spacer(),
          ui::dsl::column()
              .style(items_end().gap(6.0f))
              .children(
                  ui::dsl::row()
                      .bind(scene_status_chip_node)
                      .style(
                          items_center()
                              .padding_xy(12.0f, 8.0f)
                              .radius(999.0f)
                              .background(theme.success_soft)
                              .border(1.0f, theme.success)
                      )
                      .children(
                          text("ACTIVE")
                              .bind(scene_status_text_node)
                              .style(
                                  font_size(12.5f).text_color(theme.success)
                              )
                      ),
                  text("No active scene")
                      .bind(scene_name_node)
                      .style(font_size(13.0f).text_color(theme.text_muted))
              )
      );
}

ui::dsl::NodeSpec build_summary_row(
    ui::UINodeId &fps_value_node,
    ui::UINodeId &frame_time_value_node,
    const RuntimePanelTheme &theme
) {
  return ui::dsl::row()
      .style(fill_x().gap(12.0f))
      .children(
          summary_card(
              "Frame Rate",
              "Average FPS over the last 0.25s.",
              fps_value_node,
              theme.accent,
              theme
          ),
          summary_card(
              "Frame Time",
              "Average milliseconds per frame.",
              frame_time_value_node,
              theme.text_primary,
              theme
          )
      );
}

ui::dsl::NodeSpec build_scene_section(
    ui::UINodeId &entities_value_node,
    ui::UINodeId &renderables_value_node,
    const RuntimePanelTheme &theme
) {
  auto cards = ui::dsl::row().style(fill_x().gap(12.0f));
  cards.child(
      metric_card(
          "Entities",
          "Scene entities tracked by the world.",
          entities_value_node,
          theme.text_primary,
          theme
      )
  );
  cards.child(
      metric_card(
          "Renderables",
          "Objects currently tagged for rendering.",
          renderables_value_node,
          theme.accent,
          theme
      )
  );

  return ui::dsl::column()
      .style(fill_x().gap(10.0f))
      .children(
          section_heading(
              "Scene",
              "High-level footprint of the active world.",
              theme
          ),
          std::move(cards)
      );
}

ui::dsl::NodeSpec build_physics_section(
    ui::UINodeId &rigid_bodies_value_node,
    ui::UINodeId &dynamic_bodies_value_node,
    ui::UINodeId &static_bodies_value_node,
    const RuntimePanelTheme &theme
) {
  auto cards = ui::dsl::row().style(fill_x().gap(12.0f));
  cards.child(
      metric_card(
          "Rigid Bodies",
          "All bodies registered with physics.",
          rigid_bodies_value_node,
          theme.text_primary,
          theme
      )
  );
  cards.child(
      metric_card(
          "Dynamic",
          "Bodies simulated every frame.",
          dynamic_bodies_value_node,
          theme.accent,
          theme
      )
  );
  cards.child(
      metric_card(
          "Static",
          "Bodies used as immovable collision.",
          static_bodies_value_node,
          theme.text_primary,
          theme
      )
  );

  return ui::dsl::column()
      .style(fill_x().gap(10.0f))
      .children(
          section_heading(
              "Physics",
              "Rigid-body totals by simulation mode.",
              theme
          ),
          std::move(cards)
      );
}

ui::dsl::NodeSpec build_systems_section(
    ui::UINodeId &lights_value_node,
    ui::UINodeId &cameras_value_node,
    ui::UINodeId &ui_roots_value_node,
    const RuntimePanelTheme &theme
) {
  auto cards = ui::dsl::row().style(fill_x().gap(12.0f));
  cards.child(
      metric_card(
          "Lights",
          "Directional, point, and spot lights.",
          lights_value_node,
          theme.accent,
          theme
      )
  );
  cards.child(
      metric_card(
          "Main Cameras",
          "Active view-driving camera tags.",
          cameras_value_node,
          theme.text_primary,
          theme
      )
  );
  cards.child(
      metric_card(
          "UI Roots",
          "Top-level UI documents in the world.",
          ui_roots_value_node,
          theme.text_primary,
          theme
      )
  );

  return ui::dsl::column()
      .style(fill_x().gap(10.0f))
      .children(
          section_heading(
              "Systems",
              "Core runtime subsystems visible in the scene.",
              theme
          ),
          std::move(cards)
      );
}

ui::dsl::NodeSpec build_geometry_section(
    ui::UINodeId &vertices_value_node,
    ui::UINodeId &triangles_value_node,
    ui::UINodeId &shadow_casters_value_node,
    const RuntimePanelTheme &theme
) {
  auto cards = ui::dsl::row().style(fill_x().gap(12.0f));
  cards.child(
      metric_card(
          "Vertices",
          "Total vertices across all meshes.",
          vertices_value_node,
          theme.text_primary,
          theme
      )
  );
  cards.child(
      metric_card(
          "Triangles",
          "Total triangles drawn per frame.",
          triangles_value_node,
          theme.accent,
          theme
      )
  );
  cards.child(
      metric_card(
          "Shadow Casters",
          "Entities casting shadows.",
          shadow_casters_value_node,
          theme.text_primary,
          theme
      )
  );

  return ui::dsl::column()
      .style(fill_x().gap(10.0f))
      .children(
          section_heading(
              "Geometry",
              "Mesh budget across all renderables.",
              theme
          ),
          std::move(cards)
      );
}

ui::dsl::NodeSpec build_resources_section(
    ui::UINodeId &textures_value_node,
    ui::UINodeId &shaders_value_node,
    ui::UINodeId &materials_value_node,
    ui::UINodeId &models_value_node,
    const RuntimePanelTheme &theme
) {
  auto top_row = ui::dsl::row().style(fill_x().gap(12.0f));
  top_row.child(
      metric_card(
          "Textures",
          "2D and 3D textures loaded.",
          textures_value_node,
          theme.text_primary,
          theme
      )
  );
  top_row.child(
      metric_card(
          "Shaders",
          "Compiled shader programs.",
          shaders_value_node,
          theme.accent,
          theme
      )
  );

  auto bottom_row = ui::dsl::row().style(fill_x().gap(12.0f));
  bottom_row.child(
      metric_card(
          "Materials",
          "Material instances in use.",
          materials_value_node,
          theme.text_primary,
          theme
      )
  );
  bottom_row.child(
      metric_card(
          "Models",
          "Loaded model assets.",
          models_value_node,
          theme.text_primary,
          theme
      )
  );

  return ui::dsl::column()
      .style(fill_x().gap(10.0f))
      .children(
          section_heading(
              "Resources",
              "GPU-resident asset pools.",
              theme
          ),
          std::move(top_row),
          std::move(bottom_row)
      );
}

ui::dsl::NodeSpec build_fps_chart(
    ui::UINodeId &chart_container_node,
    ui::UINodeId &hover_label_node,
    const RuntimePanelTheme &theme
) {
  return ui::dsl::column()
      .style(fill_x().gap(6.0f))
      .children(
          ui::dsl::row()
              .style(fill_x().items_center())
              .children(
                  text("FPS History")
                      .style(font_size(11.5f).text_color(theme.text_muted)),
                  spacer(),
                  text("")
                      .bind(hover_label_node)
                      .style(font_size(12.0f).text_color(theme.chart_bar_fill))
              ),
          bar_chart(ui::dsl::BarChartStyle{
              .height = 80.0f,
              .bar_gap = 1.0f,
              .bar_color = theme.chart_bar_fill,
              .bar_background = theme.chart_bar_background,
              .border_radius = 2.0f,
          })
          .bind(chart_container_node)
      );
}

ui::dsl::NodeSpec build_frame_time_chart(
    ui::UINodeId &chart_container_node,
    ui::UINodeId &hover_label_node,
    const RuntimePanelTheme &theme
) {
  return ui::dsl::column()
      .style(fill_x().gap(6.0f))
      .children(
          ui::dsl::row()
              .style(fill_x().items_center())
              .children(
                  text("Frame Time History")
                      .style(font_size(11.5f).text_color(theme.text_muted)),
                  spacer(),
                  text("")
                      .bind(hover_label_node)
                      .style(
                          font_size(12.0f).text_color(theme.chart_bar_fill_alt)
                      )
              ),
          bar_chart(ui::dsl::BarChartStyle{
              .height = 80.0f,
              .bar_gap = 1.0f,
              .bar_color = theme.chart_bar_fill_alt,
              .bar_background = theme.chart_bar_background,
              .border_radius = 2.0f,
          })
          .bind(chart_container_node)
      );
}

ui::dsl::NodeSpec build_entity_gauge(
    ui::UINodeId &gauge_fill_node,
    const RuntimePanelTheme &theme
) {
  return gauge_row("Entity budget", gauge_fill_node, theme);
}

ui::dsl::NodeSpec build_renderable_gauge(
    ui::UINodeId &gauge_fill_node,
    const RuntimePanelTheme &theme
) {
  return gauge_row("Renderable budget", gauge_fill_node, theme);
}

ui::dsl::NodeSpec build_triangle_gauge(
    ui::UINodeId &gauge_fill_node,
    const RuntimePanelTheme &theme
) {
  return gauge_row("Triangle budget", gauge_fill_node, theme);
}

ui::dsl::NodeSpec build_fps_line_chart(
    ui::UINodeId &line_chart_node,
    const RuntimePanelTheme &theme
) {
  return ui::dsl::column()
      .style(fill_x().gap(6.0f))
      .children(
          text("FPS Trend")
              .style(font_size(11.5f).text_color(theme.text_muted)),
          line_chart(ui::dsl::LineChartStyle{
              .height = 100.0f,
              .grid_line_count = 4u,
              .grid_color = theme.line_chart_grid,
              .background_color = theme.line_chart_background,
              .border_radius = 8.0f,
          })
          .bind(line_chart_node)
      );
}

ui::dsl::NodeSpec build_frame_time_line_chart(
    ui::UINodeId &line_chart_node,
    const RuntimePanelTheme &theme
) {
  return ui::dsl::column()
      .style(fill_x().gap(6.0f))
      .children(
          text("Frame Time Trend")
              .style(font_size(11.5f).text_color(theme.text_muted)),
          line_chart(ui::dsl::LineChartStyle{
              .height = 100.0f,
              .grid_line_count = 4u,
              .grid_color = theme.line_chart_grid,
              .background_color = theme.line_chart_background,
              .border_radius = 8.0f,
          })
          .bind(line_chart_node)
      );
}

ui::dsl::NodeSpec build_process_section(
    ui::UINodeId &cpu_value_node,
    ui::UINodeId &memory_value_node,
    ui::UINodeId &binary_size_value_node,
    const RuntimePanelTheme &theme
) {
  auto cards = ui::dsl::row().style(fill_x().gap(12.0f));
  cards.child(
      metric_card(
          "CPU Usage",
          "Process CPU utilization.",
          cpu_value_node,
          theme.line_chart_cpu_line,
          theme
      )
  );
  cards.child(
      metric_card(
          "Memory (RSS)",
          "Resident set size of the process.",
          memory_value_node,
          theme.line_chart_memory_line,
          theme
      )
  );
  cards.child(
      metric_card(
          "Binary Size",
          "On-disk size of the executable.",
          binary_size_value_node,
          theme.text_primary,
          theme
      )
  );

  return ui::dsl::column()
      .style(fill_x().gap(10.0f))
      .children(
          section_heading(
              "Process",
              "CPU and memory usage of the running process.",
              theme
          ),
          std::move(cards)
      );
}

ui::dsl::NodeSpec build_cpu_line_chart(
    ui::UINodeId &line_chart_node,
    const RuntimePanelTheme &theme
) {
  return ui::dsl::column()
      .style(fill_x().gap(6.0f))
      .children(
          text("CPU Trend")
              .style(font_size(11.5f).text_color(theme.text_muted)),
          line_chart(ui::dsl::LineChartStyle{
              .height = 100.0f,
              .grid_line_count = 4u,
              .grid_color = theme.line_chart_grid,
              .background_color = theme.line_chart_background,
              .border_radius = 8.0f,
          })
          .bind(line_chart_node)
      );
}

ui::dsl::NodeSpec build_memory_line_chart(
    ui::UINodeId &line_chart_node,
    const RuntimePanelTheme &theme
) {
  return ui::dsl::column()
      .style(fill_x().gap(6.0f))
      .children(
          text("Memory Trend")
              .style(font_size(11.5f).text_color(theme.text_muted)),
          line_chart(ui::dsl::LineChartStyle{
              .height = 100.0f,
              .grid_line_count = 4u,
              .grid_color = theme.line_chart_grid,
              .background_color = theme.line_chart_background,
              .border_radius = 8.0f,
          })
          .bind(line_chart_node)
      );
}

ui::dsl::NodeSpec build_memory_gauge(
    ui::UINodeId &gauge_fill_node,
    const RuntimePanelTheme &theme
) {
  return gauge_row("Memory budget (2 GB)", gauge_fill_node, theme);
}

ui::dsl::NodeSpec build_process_details_section(
    ui::UINodeId &threads_value_node,
    ui::UINodeId &fds_value_node,
    ui::UINodeId &minor_faults_value_node,
    ui::UINodeId &major_faults_value_node,
    ui::UINodeId &voluntary_ctx_value_node,
    ui::UINodeId &involuntary_ctx_value_node,
    const RuntimePanelTheme &theme
) {
  auto top_row = ui::dsl::row().style(fill_x().gap(12.0f));
  top_row.child(
      metric_card(
          "Threads", "Active OS threads.", threads_value_node,
          theme.text_primary, theme
      )
  );
  top_row.child(
      metric_card(
          "Open FDs", "File descriptors in use.", fds_value_node,
          theme.text_primary, theme
      )
  );

  auto mid_row = ui::dsl::row().style(fill_x().gap(12.0f));
  mid_row.child(
      metric_card(
          "Minor Faults", "Pages resolved from cache.", minor_faults_value_node,
          theme.text_primary, theme
      )
  );
  mid_row.child(
      metric_card(
          "Major Faults", "Pages loaded from disk.", major_faults_value_node,
          theme.accent, theme
      )
  );

  auto bottom_row = ui::dsl::row().style(fill_x().gap(12.0f));
  bottom_row.child(
      metric_card(
          "Vol. Ctx Sw", "Voluntary context switches.",
          voluntary_ctx_value_node, theme.text_primary, theme
      )
  );
  bottom_row.child(
      metric_card(
          "Invol. Ctx Sw", "Involuntary context switches.",
          involuntary_ctx_value_node, theme.text_primary, theme
      )
  );

  return ui::dsl::column()
      .style(fill_x().gap(10.0f))
      .children(
          section_heading(
              "Process Details",
              "Threads, file descriptors, page faults, and context switches.",
              theme
          ),
          std::move(top_row),
          std::move(mid_row),
          std::move(bottom_row)
      );
}

ui::dsl::NodeSpec build_io_section(
    ui::UINodeId &disk_read_value_node,
    ui::UINodeId &disk_write_value_node,
    const RuntimePanelTheme &theme
) {
  auto cards = ui::dsl::row().style(fill_x().gap(12.0f));
  cards.child(
      metric_card(
          "Disk Read", "Cumulative bytes read.", disk_read_value_node,
          theme.text_primary, theme
      )
  );
  cards.child(
      metric_card(
          "Disk Write", "Cumulative bytes written.", disk_write_value_node,
          theme.accent, theme
      )
  );

  return ui::dsl::column()
      .style(fill_x().gap(10.0f))
      .children(
          section_heading(
              "I/O", "Disk read and write activity.", theme
          ),
          std::move(cards)
      );
}

ui::dsl::NodeSpec build_allocator_section(
    ui::UINodeId &heap_used_value_node,
    ui::UINodeId &mmap_used_value_node,
    ui::UINodeId &alloc_rate_value_node,
    const RuntimePanelTheme &theme
) {
  auto cards = ui::dsl::row().style(fill_x().gap(12.0f));
  cards.child(
      metric_card(
          "Heap Used", "In-use heap allocations.", heap_used_value_node,
          theme.line_chart_heap_line, theme
      )
  );
  cards.child(
      metric_card(
          "mmap'd", "Memory-mapped allocations.", mmap_used_value_node,
          theme.text_primary, theme
      )
  );
  cards.child(
      metric_card(
          "Alloc Rate", "Heap growth per second.", alloc_rate_value_node,
          theme.text_primary, theme
      )
  );

  return ui::dsl::column()
      .style(fill_x().gap(10.0f))
      .children(
          section_heading(
              "Allocator",
              "Heap and mmap usage from the C allocator.",
              theme
          ),
          std::move(cards)
      );
}

ui::dsl::NodeSpec build_heap_line_chart(
    ui::UINodeId &line_chart_node,
    const RuntimePanelTheme &theme
) {
  return ui::dsl::column()
      .style(fill_x().gap(6.0f))
      .children(
          text("Heap Trend")
              .style(font_size(11.5f).text_color(theme.text_muted)),
          line_chart(ui::dsl::LineChartStyle{
              .height = 100.0f,
              .grid_line_count = 4u,
              .grid_color = theme.line_chart_grid,
              .background_color = theme.line_chart_background,
              .border_radius = 8.0f,
          })
          .bind(line_chart_node)
      );
}

ui::dsl::NodeSpec build_gpu_section(
    ui::UINodeId &draw_calls_value_node,
    ui::UINodeId &state_changes_value_node,
    ui::UINodeId &gpu_time_value_node,
    ui::UINodeId &gpu_mem_used_value_node,
    ui::UINodeId &gpu_mem_total_value_node,
    const RuntimePanelTheme &theme
) {
  auto top_row = ui::dsl::row().style(fill_x().gap(12.0f));
  top_row.child(
      metric_card(
          "Draw Calls", "GL draw commands per frame.",
          draw_calls_value_node, theme.line_chart_draw_calls_line, theme
      )
  );
  top_row.child(
      metric_card(
          "State Changes", "GL state mutations per frame.",
          state_changes_value_node, theme.text_primary, theme
      )
  );
  top_row.child(
      metric_card(
          "GPU Time", "GPU-side frame duration.",
          gpu_time_value_node, theme.line_chart_gpu_time_line, theme
      )
  );

  auto bottom_row = ui::dsl::row().style(fill_x().gap(12.0f));
  bottom_row.child(
      metric_card(
          "GPU Mem Used", "Video memory in use.",
          gpu_mem_used_value_node, theme.accent, theme
      )
  );
  bottom_row.child(
      metric_card(
          "GPU Mem Total", "Total video memory.",
          gpu_mem_total_value_node, theme.text_primary, theme
      )
  );

  return ui::dsl::column()
      .style(fill_x().gap(10.0f))
      .children(
          section_heading(
              "GPU",
              "Draw calls, state changes, and video memory.",
              theme
          ),
          std::move(top_row),
          std::move(bottom_row)
      );
}

ui::dsl::NodeSpec build_draw_calls_line_chart(
    ui::UINodeId &line_chart_node,
    const RuntimePanelTheme &theme
) {
  return ui::dsl::column()
      .style(fill_x().gap(6.0f))
      .children(
          text("Draw Calls Trend")
              .style(font_size(11.5f).text_color(theme.text_muted)),
          line_chart(ui::dsl::LineChartStyle{
              .height = 100.0f,
              .grid_line_count = 4u,
              .grid_color = theme.line_chart_grid,
              .background_color = theme.line_chart_background,
              .border_radius = 8.0f,
          })
          .bind(line_chart_node)
      );
}

ui::dsl::NodeSpec build_gpu_time_line_chart(
    ui::UINodeId &line_chart_node,
    const RuntimePanelTheme &theme
) {
  return ui::dsl::column()
      .style(fill_x().gap(6.0f))
      .children(
          text("GPU Time Trend")
              .style(font_size(11.5f).text_color(theme.text_muted)),
          line_chart(ui::dsl::LineChartStyle{
              .height = 100.0f,
              .grid_line_count = 4u,
              .grid_color = theme.line_chart_grid,
              .background_color = theme.line_chart_background,
              .border_radius = 8.0f,
          })
          .bind(line_chart_node)
      );
}

ui::dsl::NodeSpec build_gpu_memory_gauge(
    ui::UINodeId &gauge_fill_node,
    const RuntimePanelTheme &theme
) {
  return gauge_row("GPU memory budget", gauge_fill_node, theme);
}

ui::dsl::NodeSpec
build_empty_state(ui::UINodeId &empty_state_node, const RuntimePanelTheme &theme) {
  return ui::dsl::column()
      .bind(empty_state_node)
      .style(
          fill_x()
              .padding(18.0f)
              .gap(6.0f)
              .radius(18.0f)
              .background(theme.panel_background)
              .border(1.0f, theme.panel_border)
              .items_start()
      )
      .visible(false)
      .children(
          text("No active scene")
              .style(font_size(18.0f).text_color(theme.text_primary)),
          text("Runtime counters appear here when SceneManager exposes an active scene.")
              .style(font_size(13.0f).text_color(theme.text_muted))
      );
}

} // namespace runtime_panel

ui::dsl::NodeSpec RuntimePanelController::build() {
  const RuntimePanelTheme theme;

  return scroll_view()
      .style(fill_x().flex(1.0f).min_height(px(0.0f)).background(theme.shell_background))
      .child(
          ui::dsl::column()
              .style(fill_x().padding(18.0f).gap(14.0f))
              .children(
                  runtime_panel::build_header(
                      m_scene_status_chip_node,
                      m_scene_status_text_node,
                      m_scene_name_node,
                      theme
                  ),
                  runtime_panel::build_summary_row(
                      m_fps_value_node, m_frame_time_value_node, theme
                  ),
                  runtime_panel::build_fps_chart(
                      m_fps_chart_container,
                      m_fps_tooltip_text_node,
                      theme
                  ),
                  runtime_panel::build_frame_time_chart(
                      m_frame_time_chart_container,
                      m_frame_time_tooltip_text_node,
                      theme
                  ),
                  runtime_panel::build_fps_line_chart(
                      m_fps_line_chart_node, theme
                  ),
                  runtime_panel::build_frame_time_line_chart(
                      m_frame_time_line_chart_node, theme
                  ),
                  runtime_panel::build_process_section(
                      m_cpu_value_node,
                      m_memory_value_node,
                      m_binary_size_value_node,
                      theme
                  ),
                  runtime_panel::build_cpu_line_chart(
                      m_cpu_line_chart_node, theme
                  ),
                  runtime_panel::build_memory_line_chart(
                      m_memory_line_chart_node, theme
                  ),
                  runtime_panel::build_memory_gauge(
                      m_memory_gauge_fill, theme
                  ),
                  runtime_panel::build_process_details_section(
                      m_threads_value_node,
                      m_fds_value_node,
                      m_minor_faults_value_node,
                      m_major_faults_value_node,
                      m_voluntary_ctx_value_node,
                      m_involuntary_ctx_value_node,
                      theme
                  ),
                  runtime_panel::build_io_section(
                      m_disk_read_value_node,
                      m_disk_write_value_node,
                      theme
                  ),
                  runtime_panel::build_allocator_section(
                      m_heap_used_value_node,
                      m_mmap_used_value_node,
                      m_alloc_rate_value_node,
                      theme
                  ),
                  runtime_panel::build_heap_line_chart(
                      m_heap_line_chart_node, theme
                  ),
                  runtime_panel::build_gpu_section(
                      m_draw_calls_value_node,
                      m_state_changes_value_node,
                      m_gpu_time_value_node,
                      m_gpu_mem_used_value_node,
                      m_gpu_mem_total_value_node,
                      theme
                  ),
                  runtime_panel::build_draw_calls_line_chart(
                      m_draw_calls_line_chart_node, theme
                  ),
                  runtime_panel::build_gpu_time_line_chart(
                      m_gpu_time_line_chart_node, theme
                  ),
                  runtime_panel::build_gpu_memory_gauge(
                      m_gpu_memory_gauge_fill, theme
                  ),
                  ui::dsl::column()
                      .bind(m_metrics_root_node)
                      .style(fill_x().gap(12.0f))
                      .children(
                          runtime_panel::build_scene_section(
                              m_entities_value_node,
                              m_renderables_value_node,
                              theme
                          ),
                          runtime_panel::build_entity_gauge(
                              m_entities_gauge_fill, theme
                          ),
                          runtime_panel::build_renderable_gauge(
                              m_renderables_gauge_fill, theme
                          ),
                          runtime_panel::build_physics_section(
                              m_rigid_bodies_value_node,
                              m_dynamic_bodies_value_node,
                              m_static_bodies_value_node,
                              theme
                          ),
                          runtime_panel::build_systems_section(
                              m_lights_value_node,
                              m_cameras_value_node,
                              m_ui_roots_value_node,
                              theme
                          ),
                          runtime_panel::build_geometry_section(
                              m_vertices_value_node,
                              m_triangles_value_node,
                              m_shadow_casters_value_node,
                              theme
                          ),
                          runtime_panel::build_triangle_gauge(
                              m_triangles_gauge_fill, theme
                          ),
                          runtime_panel::build_resources_section(
                              m_textures_value_node,
                              m_shaders_value_node,
                              m_materials_value_node,
                              m_models_value_node,
                              theme
                          )
                      ),
                  runtime_panel::build_empty_state(m_empty_state_node, theme)
              )
      );
}

} // namespace astralix::editor
