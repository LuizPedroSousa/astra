#pragma once

#include "dsl.hpp"
#include "editor-theme.hpp"

namespace astralix::editor::runtime_panel {

ui::dsl::NodeSpec build_header(
    ui::UINodeId &scene_status_chip_node,
    ui::UINodeId &scene_status_text_node,
    ui::UINodeId &scene_name_node,
    const RuntimePanelTheme &theme
);

ui::dsl::NodeSpec build_summary_row(
    ui::UINodeId &fps_value_node,
    ui::UINodeId &frame_time_value_node,
    const RuntimePanelTheme &theme
);

ui::dsl::NodeSpec build_scene_section(
    ui::UINodeId &entities_value_node,
    ui::UINodeId &renderables_value_node,
    const RuntimePanelTheme &theme
);

ui::dsl::NodeSpec build_physics_section(
    ui::UINodeId &rigid_bodies_value_node,
    ui::UINodeId &dynamic_bodies_value_node,
    ui::UINodeId &static_bodies_value_node,
    const RuntimePanelTheme &theme
);

ui::dsl::NodeSpec build_systems_section(
    ui::UINodeId &lights_value_node,
    ui::UINodeId &cameras_value_node,
    ui::UINodeId &ui_roots_value_node,
    const RuntimePanelTheme &theme
);

ui::dsl::NodeSpec build_geometry_section(
    ui::UINodeId &vertices_value_node,
    ui::UINodeId &triangles_value_node,
    ui::UINodeId &shadow_casters_value_node,
    const RuntimePanelTheme &theme
);

ui::dsl::NodeSpec build_resources_section(
    ui::UINodeId &textures_value_node,
    ui::UINodeId &shaders_value_node,
    ui::UINodeId &materials_value_node,
    ui::UINodeId &models_value_node,
    const RuntimePanelTheme &theme
);

ui::dsl::NodeSpec build_fps_chart(
    ui::UINodeId &chart_container_node,
    ui::UINodeId &hover_label_node,
    const RuntimePanelTheme &theme
);

ui::dsl::NodeSpec build_frame_time_chart(
    ui::UINodeId &chart_container_node,
    ui::UINodeId &hover_label_node,
    const RuntimePanelTheme &theme
);

ui::dsl::NodeSpec build_entity_gauge(
    ui::UINodeId &gauge_fill_node,
    const RuntimePanelTheme &theme
);

ui::dsl::NodeSpec build_renderable_gauge(
    ui::UINodeId &gauge_fill_node,
    const RuntimePanelTheme &theme
);

ui::dsl::NodeSpec build_triangle_gauge(
    ui::UINodeId &gauge_fill_node,
    const RuntimePanelTheme &theme
);

ui::dsl::NodeSpec build_fps_line_chart(
    ui::UINodeId &line_chart_node,
    const RuntimePanelTheme &theme
);

ui::dsl::NodeSpec build_frame_time_line_chart(
    ui::UINodeId &line_chart_node,
    const RuntimePanelTheme &theme
);

ui::dsl::NodeSpec build_process_section(
    ui::UINodeId &cpu_value_node,
    ui::UINodeId &memory_value_node,
    ui::UINodeId &binary_size_value_node,
    const RuntimePanelTheme &theme
);

ui::dsl::NodeSpec build_cpu_line_chart(
    ui::UINodeId &line_chart_node,
    const RuntimePanelTheme &theme
);

ui::dsl::NodeSpec build_memory_line_chart(
    ui::UINodeId &line_chart_node,
    const RuntimePanelTheme &theme
);

ui::dsl::NodeSpec build_memory_gauge(
    ui::UINodeId &gauge_fill_node,
    const RuntimePanelTheme &theme
);

ui::dsl::NodeSpec build_process_details_section(
    ui::UINodeId &threads_value_node,
    ui::UINodeId &fds_value_node,
    ui::UINodeId &minor_faults_value_node,
    ui::UINodeId &major_faults_value_node,
    ui::UINodeId &voluntary_ctx_value_node,
    ui::UINodeId &involuntary_ctx_value_node,
    const RuntimePanelTheme &theme
);

ui::dsl::NodeSpec build_io_section(
    ui::UINodeId &disk_read_value_node,
    ui::UINodeId &disk_write_value_node,
    const RuntimePanelTheme &theme
);

ui::dsl::NodeSpec build_allocator_section(
    ui::UINodeId &heap_used_value_node,
    ui::UINodeId &mmap_used_value_node,
    ui::UINodeId &alloc_rate_value_node,
    const RuntimePanelTheme &theme
);

ui::dsl::NodeSpec build_heap_line_chart(
    ui::UINodeId &line_chart_node,
    const RuntimePanelTheme &theme
);

ui::dsl::NodeSpec build_gpu_section(
    ui::UINodeId &draw_calls_value_node,
    ui::UINodeId &state_changes_value_node,
    ui::UINodeId &gpu_time_value_node,
    ui::UINodeId &gpu_mem_used_value_node,
    ui::UINodeId &gpu_mem_total_value_node,
    const RuntimePanelTheme &theme
);

ui::dsl::NodeSpec build_draw_calls_line_chart(
    ui::UINodeId &line_chart_node,
    const RuntimePanelTheme &theme
);

ui::dsl::NodeSpec build_gpu_time_line_chart(
    ui::UINodeId &line_chart_node,
    const RuntimePanelTheme &theme
);

ui::dsl::NodeSpec build_gpu_memory_gauge(
    ui::UINodeId &gauge_fill_node,
    const RuntimePanelTheme &theme
);

ui::dsl::NodeSpec
build_empty_state(ui::UINodeId &empty_state_node, const RuntimePanelTheme &theme);

} // namespace astralix::editor::runtime_panel
