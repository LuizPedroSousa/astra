#pragma once

#include "immediate.hpp"
#include "panels/panel-controller.hpp"

namespace astralix::editor {

class NavigationGizmoPanelController final : public PanelController {
public:
  static constexpr PanelMinimumSize kMinimumSize{
      .width = 164.0f,
      .height = 186.0f,
  };

  PanelMinimumSize minimum_size() const override { return kMinimumSize; }
  void render(ui::im::Frame &ui) override;
  void mount(const PanelMountContext &context) override;
  void unmount() override;
  void update(const PanelUpdateContext &context) override;
  std::optional<uint64_t> render_version() const override;

private:
  void sync_draw_rect();

  ui::im::Runtime *m_runtime = nullptr;
  ui::im::WidgetId m_body_widget = ui::im::k_invalid_widget_id;
};

} // namespace astralix::editor
