#pragma once

#include "systems/workspace-shell-system.hpp"

#include "components/tags.hpp"
#include "components/ui.hpp"
#include "dsl.hpp"
#include "editor-selection-store.hpp"
#include "editor-theme.hpp"
#include "layouts/layout-registry.hpp"
#include "log.hpp"
#include "managers/project-manager.hpp"
#include "managers/scene-manager.hpp"
#include "managers/system-manager.hpp"
#include "managers/window-manager.hpp"
#include "panels/panel-registry.hpp"
#include "systems/render-system/render-system.hpp"
#include "systems/render-system/scene-selection.hpp"
#include "tools/viewport/gizmo-math.hpp"
#include "trace.hpp"
#include "workspaces/workspace-registry.hpp"

#include "glm/gtx/quaternion.hpp"

#include <algorithm>
#include <cmath>
#include <string_view>
#include <unordered_set>

namespace astralix::editor::workspace_shell_detail {

inline constexpr std::string_view k_workspace_shell_root_name =
    "editor_workspace_shell";
inline constexpr std::string_view k_root_path = "root";

const WorkspaceShellTheme &workspace_shell_theme();

void configure_workspace_ui_root(
    rendering::UIRoot &root,
    const Ref<ui::UIDocument> &document,
    const ResourceDescriptorID &default_font_id,
    float default_font_size
);

std::optional<EntityID> find_workspace_shell_root_entity(ecs::World &world);

ui::dsl::StyleBuilder panel_close_button_style();
ui::dsl::NodeSpec build_panel_close_button(std::function<void()> on_click);

bool path_is_root(std::string_view path);

bool panel_frames_equal(
    const WorkspacePanelResolvedFrame &lhs,
    const WorkspacePanelResolvedFrame &rhs
);

bool nearly_equal(float lhs, float rhs, float epsilon = 0.001f);

std::optional<glm::ivec2> cursor_to_framebuffer_pixel(
    const ui::UIRect &rect,
    glm::vec2 cursor,
    const glm::ivec2 &framebuffer_extent
);

using SelectedCamera = rendering::CameraSelection;

std::optional<SelectedCamera> select_camera(Scene &scene);

std::optional<glm::vec3> cursor_world_hit(
    const gizmo::CameraFrame &camera_frame,
    const ui::UIRect &viewport_rect,
    glm::vec2 cursor,
    const glm::vec3 &plane_point,
    const glm::vec3 &plane_normal
);

float axis_scale_component(const glm::vec3 &scale, const glm::vec3 &axis);

void set_axis_scale_component(
    glm::vec3 &scale,
    const glm::vec3 &axis,
    float value
);

PanelMinimumSize combine_split_minimum_sizes(
    ui::FlexDirection axis,
    const PanelMinimumSize &first,
    const PanelMinimumSize &second
);

bool layout_contains_panel_slot(
    const LayoutNode &node,
    std::string_view panel_instance_id
);

bool layout_is_leaf(
    const LayoutNode *node,
    std::string_view panel_instance_id
);

const PanelInstanceSpec *find_workspace_panel_spec(
    const WorkspaceDefinition *workspace,
    std::string_view panel_instance_id
);

std::vector<std::string> ordered_shell_panel_ids(
    const WorkspaceDefinition *workspace,
    const std::optional<WorkspaceSnapshot> &snapshot,
    const std::vector<std::string> &panel_order
);

LayoutNode *find_layout_node_recursive(LayoutNode *node, std::string_view path);

ui::dsl::NodeSpec build_empty_workspace_state(
    std::string title,
    std::string body
);

} // namespace astralix::editor::workspace_shell_detail
