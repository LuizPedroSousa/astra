#include "tools/runtime/runtime-panel-controller.hpp"

#include "editor-theme.hpp"

#include <iomanip>
#include <sstream>

namespace astralix::editor {

namespace {

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

} // namespace

void RuntimePanelController::refresh(bool force) {
  if (m_document == nullptr) {
    return;
  }

  const RuntimeSnapshot snapshot = collect_snapshot();

  m_document->set_text(
      m_scene_status_text_node, snapshot.has_scene ? "ACTIVE" : "IDLE"
  );
  m_document->set_text(
      m_scene_name_node,
      snapshot.has_scene ? snapshot.scene_name : std::string("No active scene")
  );
  m_document->set_text(
      m_fps_value_node, format_fps(m_average_fps, m_has_timing_sample)
  );
  m_document->set_text(
      m_frame_time_value_node,
      format_frame_time_ms(m_average_frame_time_ms, m_has_timing_sample)
  );
  m_document->set_text(
      m_cpu_value_node, format_cpu_percent(m_latest_cpu_percent)
  );
  m_document->set_text(
      m_memory_value_node, format_memory_mb(m_latest_memory_rss_mb)
  );
  m_document->set_text(
      m_binary_size_value_node, format_binary_size_mb(m_latest_binary_size_mb)
  );
  m_document->set_text(
      m_threads_value_node, format_integer(m_latest_thread_count)
  );
  m_document->set_text(
      m_fds_value_node, format_integer(m_latest_open_fd_count)
  );
  m_document->set_text(
      m_minor_faults_value_node, format_large_integer(m_latest_minor_page_faults)
  );
  m_document->set_text(
      m_major_faults_value_node, format_large_integer(m_latest_major_page_faults)
  );
  m_document->set_text(
      m_voluntary_ctx_value_node,
      format_large_integer(m_latest_voluntary_ctx_switches)
  );
  m_document->set_text(
      m_involuntary_ctx_value_node,
      format_large_integer(m_latest_involuntary_ctx_switches)
  );
  m_document->set_text(
      m_disk_read_value_node, format_bytes(m_latest_disk_read_bytes)
  );
  m_document->set_text(
      m_disk_write_value_node, format_bytes(m_latest_disk_write_bytes)
  );
  m_document->set_text(
      m_heap_used_value_node, format_mb(m_latest_heap_used_mb)
  );
  m_document->set_text(
      m_mmap_used_value_node, format_mb(m_latest_mmap_used_mb)
  );
  m_document->set_text(
      m_alloc_rate_value_node, format_mb_per_sec(m_latest_alloc_rate_mb_per_sec)
  );
  m_document->set_text(
      m_draw_calls_value_node,
      format_integer(m_latest_frame_stats.draw_call_count)
  );
  m_document->set_text(
      m_state_changes_value_node,
      format_integer(m_latest_frame_stats.state_change_count)
  );
  m_document->set_text(
      m_gpu_time_value_node, format_ms(m_latest_frame_stats.gpu_frame_time_ms)
  );
  m_document->set_text(
      m_gpu_mem_used_value_node,
      format_mb(m_latest_frame_stats.gpu_memory_used_mb)
  );
  m_document->set_text(
      m_gpu_mem_total_value_node,
      format_mb(m_latest_frame_stats.gpu_memory_total_mb)
  );
  m_document->set_visible(m_metrics_root_node, snapshot.has_scene);
  m_document->set_visible(m_empty_state_node, !snapshot.has_scene);

  if (snapshot.has_scene) {
    m_document->set_text(
        m_entities_value_node, format_integer(snapshot.entity_count)
    );
    m_document->set_text(
        m_renderables_value_node, format_integer(snapshot.renderable_count)
    );
    m_document->set_text(
        m_rigid_bodies_value_node, format_integer(snapshot.rigid_body_count)
    );
    m_document->set_text(
        m_dynamic_bodies_value_node, format_integer(snapshot.dynamic_body_count)
    );
    m_document->set_text(
        m_static_bodies_value_node, format_integer(snapshot.static_body_count)
    );
    m_document->set_text(
        m_lights_value_node, format_integer(snapshot.light_count)
    );
    m_document->set_text(
        m_cameras_value_node, format_integer(snapshot.camera_count)
    );
    m_document->set_text(
        m_ui_roots_value_node, format_integer(snapshot.ui_root_count)
    );
    m_document->set_text(
        m_vertices_value_node, format_large_integer(snapshot.vertex_count)
    );
    m_document->set_text(
        m_triangles_value_node, format_large_integer(snapshot.triangle_count)
    );
    m_document->set_text(
        m_shadow_casters_value_node,
        format_integer(snapshot.shadow_caster_count)
    );
    m_document->set_text(
        m_textures_value_node, format_integer(snapshot.texture_count)
    );
    m_document->set_text(
        m_shaders_value_node, format_integer(snapshot.shader_count)
    );
    m_document->set_text(
        m_materials_value_node, format_integer(snapshot.material_count)
    );
    m_document->set_text(
        m_models_value_node, format_integer(snapshot.model_count)
    );
  }

  sync_status_pill(snapshot.has_scene, force);
  if (snapshot.has_scene) {
    refresh_gauges(snapshot);
  }
}

void RuntimePanelController::sync_status_pill(bool has_scene, bool force) {
  if (m_document == nullptr || (!force && m_last_scene_presence == has_scene)) {
    return;
  }

  const RuntimePanelTheme theme;
  m_document->mutate_style(
      m_scene_status_chip_node, [has_scene, theme](ui::UIStyle &style) {
        style.background_color =
            has_scene ? theme.success_soft : theme.accent_soft;
        style.border_color = has_scene ? theme.success : theme.accent;
      }
  );
  m_document->mutate_style(
      m_scene_status_text_node, [has_scene, theme](ui::UIStyle &style) {
        style.text_color = has_scene ? theme.success : theme.accent;
      }
  );
  m_last_scene_presence = has_scene;
}

} // namespace astralix::editor
