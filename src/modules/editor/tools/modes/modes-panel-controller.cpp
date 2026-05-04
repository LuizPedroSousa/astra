#include "tools/modes/modes-panel-controller.hpp"

#include "dsl.hpp"
#include "editor-theme.hpp"
#include "workspaces/workspace-registry.hpp"
#include "workspaces/workspace-ui-store.hpp"

namespace astralix::editor {

using namespace ui::dsl::styles;

void ModesPanelController::render(ui::im::Frame &ui) {
  const WorkspaceShellTheme theme{};
  const std::string active_workspace_id =
      workspace_ui_store()->active_workspace_id();

  auto root = ui.row("root").style(
      fill_x()
          .gap(8.0f)
          .padding(6.0f)
          .justify_end()
  );

  for (const auto *workspace : workspace_registry()->workspaces()) {
    if (workspace == nullptr) {
      continue;
    }

    const bool active = workspace->id == active_workspace_id;
    root.button(
            std::string("workspace-") + workspace->id,
            workspace->title,
            [workspace_id = workspace->id]() {
              workspace_ui_store()->request_workspace_activation(workspace_id);
            }
    )
        .style(
            padding_xy(14.0f, 10.0f)
                .radius(12.0f)
                .background(active ? theme_alpha(theme.accent_soft, 0.5f) : theme.panel_background)

                .border(1.0f, active ? theme.accent : theme.panel_border)
                .text_color(active ? theme.text_primary : theme.text_muted)
                .hover(state().background(theme.panel_raised_background))
                .pressed(state().background(theme.accent_soft))
                .focused(state().border(2.0f, theme.accent))
        );
  }
}

std::optional<uint64_t> ModesPanelController::render_version() const {
  return workspace_ui_store()->revision();
}

} // namespace astralix::editor
