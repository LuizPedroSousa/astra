#pragma once

#include "systems/ui-system/ui-system.hpp"
#include "window.hpp"

namespace astralix::ui_system_core {

CursorIcon cursor_icon_for_hit_part(const ui::UIDocument &document,
                                    ui::UINodeId node_id, ui::UIHitPart part);
void apply_resize_visual_state(
    const std::vector<RootEntry> &roots,
    const std::optional<PointerHit> &hover_hit,
    const std::optional<std::pair<Target, ui::UIHitPart>> &active_hit);
void update_panel_move_drag(const UISystem::PanelMoveDrag &drag,
                            glm::vec2 pointer);
std::optional<UISystem::SplitterResizeDrag>
begin_splitter_resize_drag(const Target &target, glm::vec2 pointer);
void update_splitter_resize_drag(const UISystem::SplitterResizeDrag &drag,
                                 glm::vec2 pointer);
void update_panel_resize_drag(const UISystem::PanelResizeDrag &drag,
                              glm::vec2 pointer);

} // namespace astralix::ui_system_core
