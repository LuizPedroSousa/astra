#include "tools/runtime/runtime-panel-controller.hpp"

#include "editor-theme.hpp"

#include <iomanip>
#include <sstream>

namespace astralix::editor {

namespace {

std::string format_integer(size_t value) { return std::to_string(value); }

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
  }

  sync_status_pill(snapshot.has_scene, force);
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
