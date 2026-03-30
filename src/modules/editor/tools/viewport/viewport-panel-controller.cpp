#include "tools/viewport/viewport-panel-controller.hpp"

#include "editor-theme.hpp"

namespace astralix::editor {

using namespace ui::dsl;
using namespace ui::dsl::styles;

ui::dsl::NodeSpec ViewportPanelController::build() {
  const ViewportPanelTheme theme;

  return render_image_view(
             RenderImageResource::SceneColor,
             RenderImageAspect::Color0,
             "editor_viewport_surface"
  )
      .style(fill().background(theme.surface));
}

void ViewportPanelController::mount(const PanelMountContext &) {}

} // namespace astralix::editor
