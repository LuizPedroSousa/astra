#pragma once

#include "panels/panel-controller.hpp"

namespace astralix::editor {

class InteractionHudPanelController final : public PanelController {
public:
  static constexpr PanelMinimumSize kMinimumSize{
      .width = 280.0f,
      .height = 156.0f,
  };

  PanelMinimumSize minimum_size() const override { return kMinimumSize; }
  void render(ui::im::Frame &ui) override;
  void mount(const PanelMountContext &) override {}
  void update(const PanelUpdateContext &context) override;
  std::optional<uint64_t> render_version() const override;
};

} // namespace astralix::editor
