#include "tools/build-overlay/build-overlay-panel-controller.hpp"

#include "build-log-store.hpp"
#include "dsl.hpp"
#include "editor-theme.hpp"

#include <algorithm>

namespace astralix::editor {

using namespace ui::dsl::styles;

void BuildOverlayPanelController::schedule_fade(
    std::chrono::steady_clock::time_point reference
) {
  for (size_t index = 0; index < m_display_lines.size(); ++index) {
    auto &line = m_display_lines[index];
    if (line.fading)
      continue;
    line.fading = true;
    line.fade_start =
        reference +
        std::chrono::milliseconds(static_cast<int>((k_linger_seconds + static_cast<float>(index) * k_stagger_seconds) * 1000.0f));
  }
}

void BuildOverlayPanelController::populate_from_snapshot(
    const BuildLogStore::Snapshot &snapshot
) {
  m_display_lines.clear();
  for (auto &log_line : snapshot.lines) {
    m_display_lines.push_back({
        .text = log_line.text,
        .opacity = 1.0f,
        .fade_start = {},
        .fading = false,
    });
  }
  if (m_display_lines.size() > k_max_visible_lines) {
    m_display_lines.erase(
        m_display_lines.begin(),
        m_display_lines.begin() +
            static_cast<ptrdiff_t>(m_display_lines.size() - k_max_visible_lines)
    );
  }
}

void BuildOverlayPanelController::sync_from_store() {
  auto &store = BuildLogStore::get();
  auto snapshot = store.snapshot();

  if (snapshot.revision == m_last_revision)
    return;

  bool first_sync = (m_last_revision == 0);
  m_last_revision = snapshot.revision;

  if (snapshot.status == BuildStatus::Building) {
    m_visible = true;
    populate_from_snapshot(snapshot);
    m_last_status = snapshot.status;
    m_render_revision++;
    return;
  }

  if (snapshot.status == BuildStatus::Succeeded ||
      snapshot.status == BuildStatus::Failed) {
    if (first_sync || snapshot.status != m_last_status) {
      if (m_display_lines.empty()) {
        populate_from_snapshot(snapshot);
      }
      if (!m_display_lines.empty()) {
        m_visible = true;
        schedule_fade(snapshot.finished_at);
        m_render_revision++;
      }
    }
  }

  m_last_status = snapshot.status;
}

void BuildOverlayPanelController::update(const PanelUpdateContext &) {
  sync_from_store();

  if (!m_visible || m_display_lines.empty())
    return;

  auto now = std::chrono::steady_clock::now();
  bool changed = false;

  for (auto &line : m_display_lines) {
    if (!line.fading)
      continue;

    if (now < line.fade_start)
      continue;

    float elapsed =
        std::chrono::duration<float>(now - line.fade_start).count();
    float target_opacity =
        std::clamp(1.0f - (elapsed / k_fade_duration), 0.0f, 1.0f);

    if (line.opacity != target_opacity) {
      line.opacity = target_opacity;
      changed = true;
    }
  }

  auto before_size = m_display_lines.size();
  std::erase_if(m_display_lines, [](const DisplayLine &line) {
    return line.fading && line.opacity <= 0.0f;
  });

  if (m_display_lines.size() != before_size)
    changed = true;

  if (m_display_lines.empty()) {
    m_visible = false;
    changed = true;
  }

  if (changed)
    m_render_revision++;
}

void BuildOverlayPanelController::render(ui::im::Frame &ui) {
  if (!m_visible || m_display_lines.empty())
    return;

  auto root = ui.column("build-overlay-root")
                  .style(
                      fill_x()
                          .background(glm::vec4(0.0f))
                          .fill_y()
                          .padding(8.0f)
                          .gap(2.0f)
                  );

  for (size_t index = 0; index < m_display_lines.size(); ++index) {
    const auto &line = m_display_lines[index];
    glm::vec4 color = k_editor_text_muted;
    color.a = line.opacity;

    root.text("line-" + std::to_string(index), line.text)
        .style(font_size(24.0f)
                   .text_color(color)
                   .font_id("fonts::noto_sans_mono"));
  }
}

std::optional<uint64_t> BuildOverlayPanelController::render_version() const {
  return m_render_revision;
}

} // namespace astralix::editor
