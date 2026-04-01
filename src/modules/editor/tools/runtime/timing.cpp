#include "tools/runtime/runtime-panel-controller.hpp"

namespace astralix::editor {

namespace {

constexpr double k_timing_sample_window_seconds = 0.25;

} // namespace

void RuntimePanelController::sample_timing(double dt) {
  if (dt <= 0.0) {
    return;
  }

  m_sample_elapsed += dt;
  m_sample_frames++;

  if (!m_has_timing_sample) {
    m_average_fps = static_cast<float>(1.0 / dt);
    m_average_frame_time_ms = static_cast<float>(dt * 1000.0);
    m_has_timing_sample = true;
  }

  if (m_sample_elapsed < k_timing_sample_window_seconds ||
      m_sample_frames == 0u) {
    return;
  }

  m_average_fps = static_cast<float>(
      static_cast<double>(m_sample_frames) / m_sample_elapsed
  );
  m_average_frame_time_ms = static_cast<float>(
      (m_sample_elapsed / static_cast<double>(m_sample_frames)) * 1000.0
  );
  m_has_timing_sample = true;
  m_sample_elapsed = 0.0;
  m_sample_frames = 0u;
}

} // namespace astralix::editor
