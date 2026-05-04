#pragma once

#include "immediate.hpp"
#include "panels/panel-controller.hpp"

namespace astralix::editor {

class ToolbarPanelController final : public PanelController {
public:
  static constexpr PanelMinimumSize kMinimumSize{
      .width = 176.0f,
      .height = 56.0f,
  };

  PanelMinimumSize minimum_size() const override { return kMinimumSize; }
  void render(ui::im::Frame &ui) override;
  void mount(const PanelMountContext &) override {}
  std::optional<uint64_t> render_version() const override;
};

} // namespace astralix::editor
