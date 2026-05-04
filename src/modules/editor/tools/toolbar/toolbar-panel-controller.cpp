#include "tools/toolbar/toolbar-panel-controller.hpp"

#include "dsl.hpp"
#include "editor-selection-store.hpp"
#include "editor-theme.hpp"
#include "workspaces/workspace-ui-store.hpp"

namespace astralix::editor {

using namespace ui::dsl::styles;

void ToolbarPanelController::render(ui::im::Frame &ui) {
  const WorkspaceShellTheme theme{};
  const auto &buttons = workspace_ui_store()->toolbar_buttons();

  auto root = ui.scroll_view("root").style(
      fill_x()
          .padding(4.0f)
          .scroll_horizontal()
          .scrollbar_auto()
          .scrollbar_thickness(8.0f)
  );
  auto content = root.row("buttons").style(fill_x().gap(8.0f));

  for (const auto &button : buttons) {
    content
        .button(
            std::string("panel-") + button.panel_instance_id,
            button.title,
            [panel_instance_id = button.panel_instance_id, open = button.open]() {
              workspace_ui_store()->request_panel_visibility(
                  panel_instance_id, !open
              );
              if (!open) {
                editor_selection_store()->request_panel_focus(panel_instance_id);
              }
            }
        )
        .style(
            padding_xy(14.0f, 10.0f)
                .radius(12.0f)
                .background(button.open ? theme_alpha(theme.accent_soft, 0.5f) : theme.panel_background)
                .border(1.0f, button.open ? theme.accent : theme.panel_border)
                .text_color(button.open ? theme.text_primary : theme.text_muted)
                .hover(state().background(theme.panel_raised_background))
                .pressed(state().background(theme.accent_soft))
                .focused(state().border(2.0f, theme.accent))
        );
  }
}

std::optional<uint64_t> ToolbarPanelController::render_version() const {
  return workspace_ui_store()->revision();
}

} // namespace astralix::editor
