#pragma once

#include "panels/panel-controller.hpp"

namespace astralix::editor {

class ViewportPanelController final : public PanelController {
public:
  ui::dsl::NodeSpec build() override;
  void mount(const PanelMountContext &context) override;
};

} // namespace astralix::editor
