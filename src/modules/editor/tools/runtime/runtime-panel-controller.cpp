#include "tools/runtime/runtime-panel-controller.hpp"

namespace astralix::editor {

void RuntimePanelController::mount(const PanelMountContext &context) {
  m_document = context.document;
  refresh(true);
}

void RuntimePanelController::unmount() { m_document = nullptr; }

void RuntimePanelController::update(const PanelUpdateContext &context) {
  sample_timing(context.dt);
  refresh();
}

} // namespace astralix::editor
