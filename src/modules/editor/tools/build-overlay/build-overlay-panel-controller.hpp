#pragma once

#include "build-log-store.hpp"
#include "panels/panel-controller.hpp"

#include <chrono>
#include <string>
#include <vector>

namespace astralix::editor {

class BuildOverlayPanelController final : public PanelController {
public:
  static constexpr PanelMinimumSize kMinimumSize{
      .width = 360.0f,
      .height = 120.0f,
  };

  PanelMinimumSize minimum_size() const override { return kMinimumSize; }
  void render(ui::im::Frame &ui) override;
  void mount(const PanelMountContext &) override {}
  void update(const PanelUpdateContext &context) override;
  std::optional<uint64_t> render_version() const override;

private:
  struct DisplayLine {
    std::string text;
    float opacity = 1.0f;
    std::chrono::steady_clock::time_point fade_start;
    bool fading = false;
  };

  void sync_from_store();
  void populate_from_snapshot(const BuildLogStore::Snapshot &snapshot);
  void schedule_fade(std::chrono::steady_clock::time_point reference);

  std::vector<DisplayLine> m_display_lines;
  BuildStatus m_last_status = BuildStatus::Idle;
  uint64_t m_last_revision = 0;
  uint64_t m_render_revision = 0;
  bool m_visible = false;

  static constexpr float k_fade_duration = 2.5f;
  static constexpr float k_linger_seconds = 6.0f;
  static constexpr float k_stagger_seconds = 0.8f;
  static constexpr size_t k_max_visible_lines = 12;
};

} // namespace astralix::editor
