#include "foundations.hpp"
#include "systems/workspace-shell-system-internal.hpp"
#include "systems/ui-system/core.hpp"
#include "systems/ui-system/ui-system.hpp"

#include <array>
#include <limits>

namespace astralix::editor {
using namespace workspace_shell_detail;

namespace {

bool length_equal(const ui::UILength &lhs, const ui::UILength &rhs) {
  return lhs.unit == rhs.unit && lhs.value == rhs.value;
}

bool rect_equal(const ui::UIRect &lhs, const ui::UIRect &rhs) {
  return nearly_equal(lhs.x, rhs.x) && nearly_equal(lhs.y, rhs.y) &&
         nearly_equal(lhs.width, rhs.width) &&
         nearly_equal(lhs.height, rhs.height);
}

size_t dock_edge_index(WorkspaceDockEdge edge) {
  switch (edge) {
    case WorkspaceDockEdge::Top:
      return 1u;
    case WorkspaceDockEdge::Right:
      return 2u;
    case WorkspaceDockEdge::Bottom:
      return 3u;
    case WorkspaceDockEdge::Center:
      return 0u;
    case WorkspaceDockEdge::Left:
    default:
      return 0u;
  }
}

bool dock_edge_is_horizontal(WorkspaceDockEdge edge) {
  return edge == WorkspaceDockEdge::Top || edge == WorkspaceDockEdge::Bottom;
}

float dock_axis_extent(const ui::UIRect &rect, WorkspaceDockEdge edge) {
  if (edge == WorkspaceDockEdge::Center) {
    return 0.0f;
  }
  return dock_edge_is_horizontal(edge) ? rect.height : rect.width;
}

void reconcile_floating_panel_node_bounds(
    ui::UIDocument &document,
    ui::UINodeId node_id,
    const WorkspacePanelResolvedFrame &frame
) {
  const auto *node = document.node(node_id);
  if (node == nullptr) {
    return;
  }

  const WorkspacePanelResolvedFrame current_frame{
      .x = node->layout.bounds.x,
      .y = node->layout.bounds.y,
      .width = node->layout.bounds.width,
      .height = node->layout.bounds.height,
  };
  if (panel_frames_equal(current_frame, frame)) {
    return;
  }

  const ui::UIRect parent_bounds =
      ui_system_core::parent_content_bounds(document, node_id);
  document.mutate_style(
      node_id,
      [&](ui::UIStyle &style) {
        style.left = ui::UILength::pixels(frame.x - parent_bounds.x);
        style.top = ui::UILength::pixels(frame.y - parent_bounds.y);
        style.width = ui::UILength::pixels(frame.width);
        style.height = ui::UILength::pixels(frame.height);
        style.right = ui::UILength::auto_value();
        style.bottom = ui::UILength::auto_value();
      }
  );
}

void reconcile_split_runtime_node_style(
    ui::UIDocument &document,
    ui::UINodeId node_id,
    const LayoutNode &split_node,
    bool first_child,
    const PanelMinimumSize &minimum_size
) {
  auto *node = document.node(node_id);
  if (node == nullptr) {
    return;
  }

  const float split_ratio = std::clamp(split_node.split_ratio, 0.1f, 0.9f);
  const bool has_fixed_first_extent =
      split_node.split_behavior.first_extent.unit != ui::UILengthUnit::Auto;
  const ui::UILength min_width = ui::UILength::pixels(minimum_size.width);
  const ui::UILength min_height = ui::UILength::pixels(minimum_size.height);
  const ui::UILength full = ui::UILength::percent(1.0f);
  const ui::UILength zero = ui::UILength::pixels(0.0f);
  const ui::UILength auto_value = ui::UILength::auto_value();
  const ui::UILength first_basis =
      has_fixed_first_extent ? split_node.split_behavior.first_extent
                             : ui::UILength::percent(split_ratio);
  const bool lock_pixels =
      has_fixed_first_extent &&
      split_node.split_behavior.first_extent.unit == ui::UILengthUnit::Pixels &&
      !split_node.split_behavior.resizable;
  const bool row_split = split_node.split_axis == ui::FlexDirection::Row;
  const ui::UILength max_width =
      lock_pixels && row_split
          ? ui::UILength::pixels(std::max(
                split_node.split_behavior.first_extent.value,
                minimum_size.width
            ))
          : auto_value;
  const ui::UILength max_height =
      lock_pixels && !row_split
          ? ui::UILength::pixels(std::max(
                split_node.split_behavior.first_extent.value,
                minimum_size.height
            ))
          : auto_value;
  const ui::UILength effective_min_width =
      lock_pixels && row_split ? max_width : min_width;
  const ui::UILength effective_min_height =
      lock_pixels && !row_split ? max_height : min_height;

  const auto style_matches = [&](const ui::UIStyle &style) {
    if (!length_equal(style.min_width, effective_min_width) ||
        !length_equal(style.min_height, effective_min_height) ||
        !length_equal(style.max_width, max_width) ||
        !length_equal(style.max_height, max_height)) {
      return false;
    }

    if (first_child) {
      return style.flex_grow == 0.0f &&
             style.flex_shrink == (lock_pixels ? 0.0f : 1.0f) &&
             length_equal(style.flex_basis, first_basis) &&
             length_equal(style.width, row_split ? auto_value : full) &&
             length_equal(style.height, row_split ? full : auto_value);
    }

    return style.flex_grow == 1.0f && style.flex_shrink == 1.0f &&
           length_equal(style.flex_basis, zero) &&
           length_equal(style.width, auto_value) &&
           length_equal(style.height, auto_value);
  };

  if (style_matches(node->style)) {
    return;
  }

  document.mutate_style(
      node_id,
      [&](ui::UIStyle &style) {
        style.min_width = effective_min_width;
        style.min_height = effective_min_height;
        style.max_width = max_width;
        style.max_height = max_height;

        if (first_child) {
          style.flex_grow = 0.0f;
          style.flex_shrink = lock_pixels ? 0.0f : 1.0f;
          style.flex_basis = first_basis;
          if (row_split) {
            style.width = auto_value;
            style.height = full;
          } else {
            style.width = full;
            style.height = auto_value;
          }
          return;
        }

        style.flex_grow = 1.0f;
        style.flex_shrink = 1.0f;
        style.flex_basis = zero;
        style.width = auto_value;
        style.height = auto_value;
      }
  );
}

} // namespace

void WorkspaceShellSystem::apply_pending_requests() {
  if (auto workspace_request =
          workspace_ui_store()->consume_workspace_activation_request();
      workspace_request.has_value()) {
    m_requested_workspace_id = std::move(*workspace_request);
  }

  for (auto &request : workspace_ui_store()->consume_panel_visibility_requests()) {
    m_pending_panel_visibility.push_back(std::move(request));
  }

  if (m_requested_workspace_id.has_value()) {
    activate_workspace(*m_requested_workspace_id);
    m_requested_workspace_id.reset();
  }

  if (m_pending_tab_activation.has_value()) {
    if (auto *tabs_node = find_tabs_node(m_pending_tab_activation->path);
        tabs_node != nullptr && tabs_node->kind == LayoutNodeKind::Tabs) {
      if (tabs_node->active_tab_id != m_pending_tab_activation->tab_id) {
        tabs_node->active_tab_id = m_pending_tab_activation->tab_id;
        m_needs_rebuild = true;
        m_needs_save = true;
        m_save_accumulator = 0.0;
      }
    }

    m_pending_tab_activation.reset();
  }

  if (!m_pending_panel_visibility.empty()) {
    for (const auto &[panel_instance_id, open] : m_pending_panel_visibility) {
      set_panel_open(panel_instance_id, open);
    }

    m_pending_panel_visibility.clear();
  }

  if (auto store_focus = editor_selection_store()->consume_panel_focus_request();
      store_focus.has_value()) {
    set_panel_open(*store_focus, true);
    if (!m_pending_panel_focus.has_value()) {
      m_pending_panel_focus = std::move(store_focus);
    }
  }

  if (m_pending_panel_focus.has_value()) {
    focus_panel(*m_pending_panel_focus);
    m_pending_panel_focus.reset();
  }
}

void WorkspaceShellSystem::sync_runtime_layout_state() {
  if (!m_active_snapshot.has_value() || m_document == nullptr) {
    return;
  }

  if (active_workspace_uses_floating_panels()) {
    bool changed = false;
    bool needs_layout_refresh = false;
    const auto *workspace = workspace_registry()->find(m_active_workspace_id);
    auto system_manager = SystemManager::get();
    auto *ui_system =
        system_manager != nullptr ? system_manager->get_system<UISystem>() : nullptr;
    std::optional<std::string> active_resize_panel_id;
    std::optional<WorkspaceDockEdge> active_resize_edge;
    float active_resize_extent = 0.0f;

    if (ui_system != nullptr && ui_system->active_panel_resize_drag().has_value()) {
      const auto &active_resize = *ui_system->active_panel_resize_drag();
      auto resize_node_it = std::find_if(
          m_floating_panel_nodes.begin(),
          m_floating_panel_nodes.end(),
          [&active_resize](const auto &entry) {
            return entry.second == active_resize.target.node_id;
          }
      );
      if (resize_node_it != m_floating_panel_nodes.end()) {
        auto panel_state = m_active_snapshot->panels.find(resize_node_it->first);
        if (panel_state != m_active_snapshot->panels.end() &&
            panel_state->second.open && panel_state->second.dock_slot.has_value() &&
            panel_state->second.dock_slot->edge != WorkspaceDockEdge::Center) {
          active_resize_panel_id = resize_node_it->first;
          active_resize_edge = panel_state->second.dock_slot->edge;
          if (const auto *node = m_document->node(resize_node_it->second);
              node != nullptr) {
            active_resize_extent =
                dock_axis_extent(node->layout.bounds, *active_resize_edge);
          }
        }
      }
    }

    std::array<float, 4> dock_extents{};
    std::array<float, 4> dock_minimum_extents{};
    for (const auto &[instance_id, node_id] : m_floating_panel_nodes) {
      auto panel_state = m_active_snapshot->panels.find(instance_id);
      if (panel_state == m_active_snapshot->panels.end() ||
          !panel_state->second.open || !panel_state->second.dock_slot.has_value()) {
        continue;
      }

      const auto *node = m_document->node(node_id);
      if (node == nullptr) {
        continue;
      }

      const WorkspaceDockEdge edge = panel_state->second.dock_slot->edge;
      if (edge == WorkspaceDockEdge::Center) {
        continue;
      }
      const PanelMinimumSize minimum = panel_minimum_size(instance_id);
      const float minimum_extent =
          dock_edge_is_horizontal(edge) ? minimum.height : minimum.width;
      const size_t edge_index = dock_edge_index(edge);
      dock_minimum_extents[edge_index] = std::max(
          dock_minimum_extents[edge_index], minimum_extent
      );

      if (active_resize_edge.has_value() && edge == *active_resize_edge) {
        if (active_resize_panel_id.has_value() &&
            instance_id == *active_resize_panel_id) {
          active_resize_extent = std::max(active_resize_extent, minimum_extent);
        }
        continue;
      }

      dock_extents[edge_index] = std::max(
          dock_extents[edge_index],
          std::max(dock_axis_extent(node->layout.bounds, edge), minimum_extent)
      );
    }

    if (active_resize_edge.has_value()) {
      const size_t edge_index = dock_edge_index(*active_resize_edge);
      dock_extents[edge_index] = std::max(
          active_resize_extent, dock_minimum_extents[edge_index]
      );
    }

    for (auto &[instance_id, panel_state] : m_active_snapshot->panels) {
      if (!panel_state.open || !panel_state.dock_slot.has_value()) {
        continue;
      }

      const WorkspaceDockEdge edge = panel_state.dock_slot->edge;
      if (edge == WorkspaceDockEdge::Center) {
        continue;
      }
      const float next_extent = dock_extents[dock_edge_index(edge)];
      if (next_extent > 0.0f &&
          !nearly_equal(panel_state.dock_slot->extent, next_extent)) {
        panel_state.dock_slot->extent = next_extent;
        changed = true;
      }
    }

    for (const auto &[instance_id, node_id] : m_floating_panel_nodes) {
      auto panel_state = m_active_snapshot->panels.find(instance_id);
      if (panel_state == m_active_snapshot->panels.end() ||
          !panel_state->second.open) {
        continue;
      }

      const auto *node = m_document->node(node_id);
      if (node == nullptr) {
        continue;
      }

      if (panel_state->second.dock_slot.has_value()) {
        const WorkspacePanelResolvedFrame resolved =
            resolve_workspace_panel_frame(instance_id);
        const WorkspacePanelResolvedFrame current_frame{
            .x = node->layout.bounds.x,
            .y = node->layout.bounds.y,
            .width = node->layout.bounds.width,
            .height = node->layout.bounds.height,
        };
        if (!panel_frames_equal(current_frame, resolved)) {
          reconcile_floating_panel_node_bounds(*m_document, node_id, resolved);
          needs_layout_refresh = true;
        }
        continue;
      }

      if (const auto *panel = find_workspace_panel_spec(workspace, instance_id);
          panel != nullptr &&
          panel->floating_placement != WorkspaceFloatingPlacement::Absolute) {
        const WorkspacePanelResolvedFrame resolved =
            resolve_floating_panel_frame(instance_id);
        const WorkspacePanelResolvedFrame current_frame{
            .x = node->layout.bounds.x,
            .y = node->layout.bounds.y,
            .width = node->layout.bounds.width,
            .height = node->layout.bounds.height,
        };
        if (!panel_frames_equal(current_frame, resolved)) {
          reconcile_floating_panel_node_bounds(*m_document, node_id, resolved);
          needs_layout_refresh = true;
        }
        continue;
      }

      const WorkspacePanelResolvedFrame next_frame{
          .x = node->layout.bounds.x,
          .y = node->layout.bounds.y,
          .width = node->layout.bounds.width,
          .height = node->layout.bounds.height,
      };
      if (!panel_state->second.floating_frame.has_value() ||
          !panel_frames_equal(*panel_state->second.floating_frame, next_frame)) {
        panel_state->second.floating_frame = next_frame;
        changed = true;
      }
    }

    if (changed) {
      m_needs_save = true;
      m_save_accumulator = 0.0;
    }
    if (needs_layout_refresh) {
      m_document->mark_layout_dirty();
    }
    return;
  }

  bool changed = false;
  for (const auto &[path, runtime] : m_split_runtime_nodes) {
    auto *layout_node = find_layout_node(path);
    if (layout_node == nullptr || layout_node->kind != LayoutNodeKind::Split) {
      continue;
    }

    const auto *first_node = m_document->node(runtime.first);
    const auto *second_node = m_document->node(runtime.second);
    if (first_node == nullptr || second_node == nullptr) {
      continue;
    }

    const auto *parent_node = m_document->node(first_node->parent);
    if (parent_node == nullptr || first_node->parent != second_node->parent) {
      continue;
    }

    if (layout_node->split_behavior.resizable) {
      const float first_size =
          layout_node->split_axis == ui::FlexDirection::Row
              ? first_node->layout.measured_size.x
              : first_node->layout.measured_size.y;
      const float container_size =
          layout_node->split_axis == ui::FlexDirection::Row
              ? parent_node->layout.content_bounds.width
              : parent_node->layout.content_bounds.height;
      if (container_size > 1.0f && first_size > 0.0f) {
        const float next_ratio =
            std::clamp(first_size / container_size, 0.1f, 0.9f);
        if (std::fabs(layout_node->split_ratio - next_ratio) > 0.001f) {
          layout_node->split_ratio = next_ratio;
          changed = true;
        }
      }
    }

    const PanelMinimumSize first_minimum =
        layout_node->first != nullptr ? layout_node_minimum_size(*layout_node->first)
                                      : PanelMinimumSize{};
    const PanelMinimumSize second_minimum =
        layout_node->second != nullptr ? layout_node_minimum_size(*layout_node->second)
                                       : PanelMinimumSize{};
    reconcile_split_runtime_node_style(
        *m_document, runtime.first, *layout_node, true, first_minimum
    );
    reconcile_split_runtime_node_style(
        *m_document, runtime.second, *layout_node, false, second_minimum
    );
  }

  if (changed) {
    m_needs_save = true;
    m_save_accumulator = 0.0;
  }
}

bool WorkspaceShellSystem::panel_instance_open(
    std::string_view panel_instance_id
) const {
  if (!m_active_snapshot.has_value()) {
    return false;
  }

  const auto it = m_active_snapshot->panels.find(std::string(panel_instance_id));
  return it != m_active_snapshot->panels.end() && it->second.open;
}

const PanelProviderDescriptor *
WorkspaceShellSystem::panel_provider(std::string_view panel_instance_id) const {
  if (m_active_snapshot.has_value()) {
    const auto state_it =
        m_active_snapshot->panels.find(std::string(panel_instance_id));
    if (state_it != m_active_snapshot->panels.end()) {
      return panel_registry()->find(state_it->second.provider_id);
    }
  }

  if (const auto *workspace = workspace_registry()->find(m_active_workspace_id);
      workspace != nullptr) {
    if (const auto *panel =
            find_workspace_panel_spec(workspace, panel_instance_id);
        panel != nullptr) {
      return panel_registry()->find(panel->provider_id);
    }
  }

  return nullptr;
}

bool WorkspaceShellSystem::panel_instance_toggleable(
    std::string_view panel_instance_id
) const {
  const auto *provider = panel_provider(panel_instance_id);
  return provider == nullptr || provider->toggleable;
}

bool WorkspaceShellSystem::panel_instance_has_shell_frame(
    std::string_view panel_instance_id
) const {
  const auto *provider = panel_provider(panel_instance_id);
  return provider == nullptr || provider->show_shell_frame;
}

bool WorkspaceShellSystem::layout_node_visible(const LayoutNode &node) const {
  switch (node.kind) {
    case LayoutNodeKind::Split:
      return (node.first != nullptr && layout_node_visible(*node.first)) ||
             (node.second != nullptr && layout_node_visible(*node.second));
    case LayoutNodeKind::Tabs:
      for (const auto &tab_id : node.tabs) {
        const auto it = m_active_snapshot->panels.find(tab_id);
        if (it != m_active_snapshot->panels.end() && it->second.open) {
          return true;
        }
      }
      return false;
    case LayoutNodeKind::Leaf:
    default: {
      const auto it = m_active_snapshot->panels.find(node.panel_instance_id);
      return it != m_active_snapshot->panels.end() && it->second.open;
    }
  }
}

bool WorkspaceShellSystem::panel_instance_rendered(
    std::string_view panel_instance_id
) const {
  if (!m_active_snapshot.has_value()) {
    return false;
  }

  if (active_workspace_uses_floating_panels()) {
    return panel_instance_open(panel_instance_id);
  }

  return panel_instance_rendered_in_layout(
      m_active_snapshot->root, panel_instance_id
  );
}

PanelMinimumSize WorkspaceShellSystem::panel_minimum_size(
    std::string_view panel_instance_id
) const {
  auto minimum_size_for_provider =
      [](std::string_view provider_id) -> std::optional<PanelMinimumSize> {
    const auto *provider = panel_registry()->find(provider_id);
    if (provider == nullptr) {
      return std::nullopt;
    }

    return provider->minimum_size;
  };

  if (m_active_snapshot.has_value()) {
    const auto state_it =
        m_active_snapshot->panels.find(std::string(panel_instance_id));
    if (state_it != m_active_snapshot->panels.end()) {
      if (const auto minimum =
              minimum_size_for_provider(state_it->second.provider_id);
          minimum.has_value()) {
        return *minimum;
      }
    }
  }

  if (const auto *workspace = workspace_registry()->find(m_active_workspace_id);
      workspace != nullptr) {
    if (const auto *panel =
            find_workspace_panel_spec(workspace, panel_instance_id);
        panel != nullptr) {
      if (const auto minimum =
              minimum_size_for_provider(panel->provider_id);
          minimum.has_value()) {
        return *minimum;
      }
    }
  }

  return PanelMinimumSize{};
}

PanelMinimumSize WorkspaceShellSystem::layout_node_minimum_size(
    const LayoutNode &node
) const {
  switch (node.kind) {
    case LayoutNodeKind::Split:
      return combine_split_minimum_sizes(
          node.split_axis,
          node.first != nullptr ? layout_node_minimum_size(*node.first)
                                : PanelMinimumSize{},
          node.second != nullptr ? layout_node_minimum_size(*node.second)
                                 : PanelMinimumSize{}
      );

    case LayoutNodeKind::Tabs: {
      PanelMinimumSize minimum_size{};
      for (const auto &tab_id : node.tabs) {
        const PanelMinimumSize tab_minimum = panel_minimum_size(tab_id);
        minimum_size.width = std::max(minimum_size.width, tab_minimum.width);
        minimum_size.height = std::max(minimum_size.height, tab_minimum.height);
      }
      return minimum_size;
    }

    case LayoutNodeKind::Leaf:
    default:
      return panel_minimum_size(node.panel_instance_id);
  }
}

bool WorkspaceShellSystem::panel_instance_rendered_in_layout(
    const LayoutNode &node,
    std::string_view panel_instance_id
) const {
  if (!m_active_snapshot.has_value()) {
    return false;
  }

  switch (node.kind) {
    case LayoutNodeKind::Split:
      return (node.first != nullptr &&
              panel_instance_rendered_in_layout(
                  *node.first, panel_instance_id
              )) ||
             (node.second != nullptr &&
              panel_instance_rendered_in_layout(
                  *node.second, panel_instance_id
              ));

    case LayoutNodeKind::Tabs: {
      const std::string active_tab_id = resolved_active_tab(node);
      if (active_tab_id != panel_instance_id) {
        return false;
      }

      const auto it = m_active_snapshot->panels.find(active_tab_id);
      return it != m_active_snapshot->panels.end() && it->second.open;
    }

    case LayoutNodeKind::Leaf:
    default: {
      if (node.panel_instance_id != panel_instance_id) {
        return false;
      }

      const auto it = m_active_snapshot->panels.find(node.panel_instance_id);
      return it != m_active_snapshot->panels.end() && it->second.open;
    }
  }
}

std::string WorkspaceShellSystem::resolved_active_tab(const LayoutNode &node) const {
  if (node.kind != LayoutNodeKind::Tabs) {
    return {};
  }

  auto is_open = [this](std::string_view instance_id) {
    const auto it = m_active_snapshot->panels.find(std::string(instance_id));
    return it != m_active_snapshot->panels.end() && it->second.open;
  };

  if (!node.active_tab_id.empty() && is_open(node.active_tab_id)) {
    return node.active_tab_id;
  }

  for (const auto &tab_id : node.tabs) {
    if (is_open(tab_id)) {
      return tab_id;
    }
  }

  return node.tabs.empty() ? std::string{} : node.tabs.front();
}

LayoutNode *WorkspaceShellSystem::find_layout_node(std::string_view path) {
  if (!m_active_snapshot.has_value()) {
    return nullptr;
  }

  return find_layout_node_recursive(&m_active_snapshot->root, path);
}

LayoutNode *WorkspaceShellSystem::find_tabs_node(std::string_view path) {
  auto *node = find_layout_node(path);
  return node != nullptr && node->kind == LayoutNodeKind::Tabs ? node : nullptr;
}

void WorkspaceShellSystem::focus_panel(std::string_view panel_instance_id) {
  const auto it = std::find(
      m_panel_order.begin(), m_panel_order.end(), panel_instance_id
  );
  if (it == m_panel_order.end() || std::next(it) == m_panel_order.end()) {
    return;
  }

  const std::string instance_id = *it;
  m_panel_order.erase(it);
  m_panel_order.push_back(instance_id);

  if (active_workspace_uses_floating_panels() && m_document != nullptr) {
    auto node_it = m_floating_panel_nodes.find(instance_id);
    if (node_it != m_floating_panel_nodes.end()) {
      m_document->append_child(m_document->root(), node_it->second);
    }
    publish_workspace_ui_state();
    m_needs_save = true;
    m_save_accumulator = 0.0;
    return;
  }

  publish_workspace_ui_state();
  m_needs_rebuild = true;
  m_needs_save = true;
  m_save_accumulator = 0.0;
}

std::optional<WorkspaceDockSlot>
WorkspaceShellSystem::panel_dock_slot(std::string_view panel_instance_id) const {
  if (m_active_snapshot.has_value()) {
    const auto it = m_active_snapshot->panels.find(std::string(panel_instance_id));
    if (it != m_active_snapshot->panels.end() && it->second.dock_slot.has_value()) {
      return it->second.dock_slot;
    }
  }

  return std::nullopt;
}

std::vector<std::string>
WorkspaceShellSystem::docked_panel_ids(WorkspaceDockEdge edge) const {
  std::vector<std::string> docked;
  if (!m_active_snapshot.has_value()) {
    return docked;
  }

  docked.reserve(m_active_snapshot->panels.size());
  for (const auto &[instance_id, panel] : m_active_snapshot->panels) {
    if (!panel.open || !panel.dock_slot.has_value() ||
        panel.dock_slot->edge != edge) {
      continue;
    }

    docked.push_back(instance_id);
  }

  auto panel_order_index = [this](std::string_view panel_instance_id) {
    const auto it = std::find(
        m_panel_order.begin(), m_panel_order.end(), panel_instance_id
    );
    return it != m_panel_order.end()
               ? static_cast<int>(std::distance(m_panel_order.begin(), it))
               : std::numeric_limits<int>::max();
  };

  std::sort(
      docked.begin(),
      docked.end(),
      [this, panel_order_index](const std::string &lhs, const std::string &rhs) {
        const auto lhs_slot = panel_dock_slot(lhs);
        const auto rhs_slot = panel_dock_slot(rhs);
        const int lhs_order = lhs_slot.has_value() ? lhs_slot->order : 0;
        const int rhs_order = rhs_slot.has_value() ? rhs_slot->order : 0;
        if (lhs_order != rhs_order) {
          return lhs_order < rhs_order;
        }

        return panel_order_index(lhs) < panel_order_index(rhs);
      }
  );
  return docked;
}

std::vector<WorkspaceShellSystem::DockDropZone>
WorkspaceShellSystem::dock_drop_zones(std::string_view panel_instance_id) const {
  std::vector<DockDropZone> zones;
  const DockLayoutMetrics metrics = compute_dock_layout_metrics(panel_instance_id);
  const PanelMinimumSize minimum = panel_minimum_size(panel_instance_id);
  const ui::UIRect center_bounds =
      metrics.floating_bounds.width > 0.0f || metrics.floating_bounds.height > 0.0f
          ? metrics.floating_bounds
          : metrics.workspace_bounds;
  if (center_bounds.width <= 0.0f || center_bounds.height <= 0.0f) {
    return zones;
  }

  auto preferred_extent = [&](WorkspaceDockEdge edge) {
    if (edge == WorkspaceDockEdge::Center) {
      return 0.0f;
    }

    const float existing_extent = edge == WorkspaceDockEdge::Left
                                      ? metrics.left_extent
                                  : edge == WorkspaceDockEdge::Top
                                      ? metrics.top_extent
                                  : edge == WorkspaceDockEdge::Right
                                      ? metrics.right_extent
                                      : metrics.bottom_extent;
    if (existing_extent > 0.0f) {
      return existing_extent;
    }

    return dock_edge_is_horizontal(edge)
               ? std::max(200.0f, minimum.height)
               : std::max(280.0f, minimum.width);
  };

  const float center_side = std::clamp(
      std::min(center_bounds.width, center_bounds.height) * 0.12f,
      56.0f,
      72.0f
  );
  const float edge_major = center_side * 0.95f;
  const float edge_minor = center_side * 0.44f;
  const float gap = center_side * 0.20f;
  const float center_x =
      center_bounds.x + (center_bounds.width - center_side) * 0.5f;
  const float center_y =
      center_bounds.y + (center_bounds.height - center_side) * 0.5f;

  zones.push_back(
      DockDropZone{
          .edge = WorkspaceDockEdge::Left,
          .target_rect =
              ui::UIRect{
                  .x = center_x - gap - edge_minor,
                  .y = center_y + (center_side - edge_major) * 0.5f,
                  .width = edge_minor,
                  .height = edge_major,
              },
          .preview_rect =
              ui::UIRect{
                  .x = metrics.workspace_bounds.x,
                  .y = metrics.workspace_bounds.y + metrics.top_extent,
                  .width = preferred_extent(WorkspaceDockEdge::Left),
                  .height = std::max(
                      0.0f,
                      metrics.workspace_bounds.height - metrics.top_extent -
                          metrics.bottom_extent
                  ),
              },
      }
  );
  zones.push_back(
      DockDropZone{
          .edge = WorkspaceDockEdge::Top,
          .target_rect =
              ui::UIRect{
                  .x = center_x + (center_side - edge_major) * 0.5f,
                  .y = center_y - gap - edge_minor,
                  .width = edge_major,
                  .height = edge_minor,
              },
          .preview_rect =
              ui::UIRect{
                  .x = metrics.workspace_bounds.x,
                  .y = metrics.workspace_bounds.y,
                  .width = metrics.workspace_bounds.width,
                  .height = preferred_extent(WorkspaceDockEdge::Top),
              },
      }
  );
  zones.push_back(
      DockDropZone{
          .edge = WorkspaceDockEdge::Right,
          .target_rect =
              ui::UIRect{
                  .x = center_x + center_side + gap,
                  .y = center_y + (center_side - edge_major) * 0.5f,
                  .width = edge_minor,
                  .height = edge_major,
              },
          .preview_rect =
              ui::UIRect{
                  .x = metrics.workspace_bounds.right() -
                       preferred_extent(WorkspaceDockEdge::Right),
                  .y = metrics.workspace_bounds.y + metrics.top_extent,
                  .width = preferred_extent(WorkspaceDockEdge::Right),
                  .height = std::max(
                      0.0f,
                      metrics.workspace_bounds.height - metrics.top_extent -
                          metrics.bottom_extent
                  ),
              },
      }
  );
  zones.push_back(
      DockDropZone{
          .edge = WorkspaceDockEdge::Bottom,
          .target_rect =
              ui::UIRect{
                  .x = center_x + (center_side - edge_major) * 0.5f,
                  .y = center_y + center_side + gap,
                  .width = edge_major,
                  .height = edge_minor,
              },
          .preview_rect =
              ui::UIRect{
                  .x = metrics.workspace_bounds.x,
                  .y = metrics.workspace_bounds.bottom() -
                       preferred_extent(WorkspaceDockEdge::Bottom),
                  .width = metrics.workspace_bounds.width,
                  .height = preferred_extent(WorkspaceDockEdge::Bottom),
              },
      }
  );
  zones.push_back(
      DockDropZone{
          .edge = WorkspaceDockEdge::Center,
          .target_rect =
              ui::UIRect{
                  .x = center_x,
                  .y = center_y,
                  .width = center_side,
                  .height = center_side,
              },
          .preview_rect = center_bounds,
      }
  );

  return zones;
}

WorkspaceShellSystem::DockLayoutMetrics
WorkspaceShellSystem::compute_dock_layout_metrics(
    std::string_view exclude_panel_instance_id
) const {
  DockLayoutMetrics metrics;

  if (m_document != nullptr) {
    if (const auto *root = m_document->node(m_document->root()); root != nullptr) {
      metrics.workspace_bounds = root->layout.content_bounds.width > 0.0f &&
                                         root->layout.content_bounds.height > 0.0f
                                     ? root->layout.content_bounds
                                     : root->layout.bounds;
    }
  }

  if (metrics.workspace_bounds.width <= 0.0f ||
      metrics.workspace_bounds.height <= 0.0f) {
    const auto window = window_manager()->active_window();
    metrics.workspace_bounds = ui::UIRect{
        .x = 0.0f,
        .y = 0.0f,
        .width = window != nullptr ? static_cast<float>(window->width()) : 0.0f,
        .height =
            window != nullptr ? static_cast<float>(window->height()) : 0.0f,
    };
  }

  if (!m_active_snapshot.has_value()) {
    metrics.floating_bounds = metrics.workspace_bounds;
    return metrics;
  }

  for (const auto &[instance_id, panel] : m_active_snapshot->panels) {
    if (!panel.open || !panel.dock_slot.has_value() ||
        (!exclude_panel_instance_id.empty() &&
         instance_id == exclude_panel_instance_id)) {
      continue;
    }

    const WorkspaceDockEdge edge = panel.dock_slot->edge;
    if (edge == WorkspaceDockEdge::Center) {
      continue;
    }
    const PanelMinimumSize minimum = panel_minimum_size(instance_id);
    const float minimum_extent =
        dock_edge_is_horizontal(edge) ? minimum.height : minimum.width;
    const float extent = std::max(panel.dock_slot->extent, minimum_extent);

    switch (edge) {
      case WorkspaceDockEdge::Left:
        metrics.left_extent = std::max(metrics.left_extent, extent);
        break;
      case WorkspaceDockEdge::Top:
        metrics.top_extent = std::max(metrics.top_extent, extent);
        break;
      case WorkspaceDockEdge::Right:
        metrics.right_extent = std::max(metrics.right_extent, extent);
        break;
      case WorkspaceDockEdge::Bottom:
        metrics.bottom_extent = std::max(metrics.bottom_extent, extent);
        break;
      case WorkspaceDockEdge::Center:
        break;
    }
  }

  metrics.top_extent =
      std::clamp(metrics.top_extent, 0.0f, metrics.workspace_bounds.height);
  metrics.bottom_extent = std::clamp(
      metrics.bottom_extent,
      0.0f,
      std::max(0.0f, metrics.workspace_bounds.height - metrics.top_extent)
  );
  metrics.left_extent =
      std::clamp(metrics.left_extent, 0.0f, metrics.workspace_bounds.width);
  metrics.right_extent = std::clamp(
      metrics.right_extent,
      0.0f,
      std::max(0.0f, metrics.workspace_bounds.width - metrics.left_extent)
  );

  metrics.floating_bounds = ui::UIRect{
      .x = metrics.workspace_bounds.x + metrics.left_extent,
      .y = metrics.workspace_bounds.y + metrics.top_extent,
      .width = std::max(
          0.0f,
          metrics.workspace_bounds.width - metrics.left_extent -
              metrics.right_extent
      ),
      .height = std::max(
          0.0f,
          metrics.workspace_bounds.height - metrics.top_extent -
              metrics.bottom_extent
      ),
  };

  return metrics;
}

WorkspacePanelResolvedFrame
WorkspaceShellSystem::resolve_workspace_panel_frame(
    std::string_view panel_instance_id
) const {
  const auto dock_slot = panel_dock_slot(panel_instance_id);
  if (!dock_slot.has_value()) {
    return resolve_floating_panel_frame(panel_instance_id);
  }

  const DockLayoutMetrics metrics = compute_dock_layout_metrics();
  const std::vector<std::string> edge_panels = docked_panel_ids(dock_slot->edge);
  const auto it =
      std::find(edge_panels.begin(), edge_panels.end(), panel_instance_id);
  if (it == edge_panels.end()) {
    return resolve_floating_panel_frame(panel_instance_id);
  }

  const size_t index = static_cast<size_t>(std::distance(edge_panels.begin(), it));
  const size_t count = std::max<size_t>(1u, edge_panels.size());
  if (dock_slot->edge == WorkspaceDockEdge::Center) {
    const ui::UIRect center_bounds =
        metrics.floating_bounds.width > 0.0f || metrics.floating_bounds.height > 0.0f
            ? metrics.floating_bounds
            : metrics.workspace_bounds;
    return WorkspacePanelResolvedFrame{
        .x = center_bounds.x,
        .y = center_bounds.y,
        .width = center_bounds.width,
        .height = center_bounds.height,
    };
  }

  if (dock_slot->edge == WorkspaceDockEdge::Left ||
      dock_slot->edge == WorkspaceDockEdge::Right) {
    const float middle_y = metrics.workspace_bounds.y + metrics.top_extent;
    const float middle_height =
        std::max(0.0f, metrics.workspace_bounds.height - metrics.top_extent -
                           metrics.bottom_extent);
    const float slot_height =
        count > 0u ? middle_height / static_cast<float>(count) : middle_height;
    const float y = middle_y + slot_height * static_cast<float>(index);
    const float next_y =
        index + 1u >= count ? middle_y + middle_height : y + slot_height;
    const float width = dock_slot->edge == WorkspaceDockEdge::Left
                            ? metrics.left_extent
                            : metrics.right_extent;
    return WorkspacePanelResolvedFrame{
        .x = dock_slot->edge == WorkspaceDockEdge::Left
                 ? metrics.workspace_bounds.x
                 : metrics.workspace_bounds.right() - width,
        .y = y,
        .width = width,
        .height = std::max(0.0f, next_y - y),
    };
  }

  const float slot_width = count > 0u
                               ? metrics.workspace_bounds.width /
                                     static_cast<float>(count)
                               : metrics.workspace_bounds.width;
  const float x =
      metrics.workspace_bounds.x + slot_width * static_cast<float>(index);
  const float next_x = index + 1u >= count
                           ? metrics.workspace_bounds.right()
                           : x + slot_width;
  const float height = dock_slot->edge == WorkspaceDockEdge::Top
                           ? metrics.top_extent
                           : metrics.bottom_extent;
  return WorkspacePanelResolvedFrame{
      .x = x,
      .y = dock_slot->edge == WorkspaceDockEdge::Top
               ? metrics.workspace_bounds.y
               : metrics.workspace_bounds.bottom() - height,
      .width = std::max(0.0f, next_x - x),
      .height = height,
  };
}

WorkspacePanelResolvedFrame WorkspaceShellSystem::resolve_floating_panel_frame(
    std::string_view panel_instance_id
) const {
  const PanelMinimumSize minimum_size = panel_minimum_size(panel_instance_id);
  const auto *workspace = workspace_registry()->find(m_active_workspace_id);
  const DockLayoutMetrics metrics = compute_dock_layout_metrics();
  const ui::UIRect floating_bounds =
      metrics.floating_bounds.width > 0.0f || metrics.floating_bounds.height > 0.0f
          ? metrics.floating_bounds
          : metrics.workspace_bounds;
  const float window_width = floating_bounds.width;
  const float window_height = floating_bounds.height;
  auto resolve_axis = [this](const ui::UILength &value, float basis) {
    return ui_system_core::resolve_length(
        value, basis, m_default_font_size, 0.0f
    );
  };

  if (const auto *panel = find_workspace_panel_spec(workspace, panel_instance_id);
      panel != nullptr &&
      panel->floating_placement != WorkspaceFloatingPlacement::Absolute) {
    WorkspacePanelFrame frame_spec =
        panel->floating_frame.value_or(WorkspacePanelFrame{
            .x = ui::UILength::pixels(24.0f),
            .y = ui::UILength::pixels(24.0f),
            .width = ui::UILength::pixels(std::max(184.0f, minimum_size.width)),
            .height = ui::UILength::pixels(std::max(120.0f, minimum_size.height)),
        });
    const float local_x = resolve_axis(frame_spec.x, window_width);
    const float local_y = resolve_axis(frame_spec.y, window_height);
    WorkspacePanelResolvedFrame frame{
        .x = floating_bounds.x + local_x,
        .y = floating_bounds.y + local_y,
        .width = resolve_axis(frame_spec.width, window_width),
        .height = resolve_axis(frame_spec.height, window_height),
    };

    const bool relax_minimum_size =
        !panel->floating_draggable && !panel->floating_resizable &&
        !panel_instance_has_shell_frame(panel_instance_id) &&
        frame_spec.width.unit != ui::UILengthUnit::MaxContent &&
        frame_spec.height.unit != ui::UILengthUnit::MaxContent;
    if (!relax_minimum_size) {
      frame.width = std::max(frame.width, minimum_size.width);
      frame.height = std::max(frame.height, minimum_size.height);
    }

    const float inset_x = std::max(0.0f, local_x);
    const float inset_y = std::max(0.0f, local_y);

    switch (panel->floating_placement) {
      case WorkspaceFloatingPlacement::TopLeft:
        frame.x = floating_bounds.x + inset_x;
        frame.y = floating_bounds.y + inset_y;
        break;
      case WorkspaceFloatingPlacement::TopCenter:
        frame.x = floating_bounds.x +
                  std::max(inset_x, (window_width - frame.width) * 0.5f);
        frame.y = floating_bounds.y + inset_y;
        break;
      case WorkspaceFloatingPlacement::TopRight:
        frame.x = std::max(
            floating_bounds.x,
            floating_bounds.right() - frame.width - inset_x
        );
        frame.y = floating_bounds.y + inset_y;
        break;
      case WorkspaceFloatingPlacement::LeftCenter:
        frame.x = floating_bounds.x + inset_x;
        frame.y = floating_bounds.y +
                  std::max(inset_y, (window_height - frame.height) * 0.5f);
        break;
      case WorkspaceFloatingPlacement::Center:
        frame.x = floating_bounds.x +
                  std::max(0.0f, (window_width - frame.width) * 0.5f);
        frame.y = floating_bounds.y +
                  std::max(0.0f, (window_height - frame.height) * 0.5f);
        break;
      case WorkspaceFloatingPlacement::RightCenter:
        frame.x = std::max(
            floating_bounds.x,
            floating_bounds.right() - frame.width - inset_x
        );
        frame.y = floating_bounds.y +
                  std::max(inset_y, (window_height - frame.height) * 0.5f);
        break;
      case WorkspaceFloatingPlacement::BottomLeft:
        frame.x = floating_bounds.x + inset_x;
        frame.y = std::max(
            floating_bounds.y,
            floating_bounds.bottom() - frame.height - inset_y
        );
        break;
      case WorkspaceFloatingPlacement::BottomCenter:
        frame.x = floating_bounds.x +
                  std::max(inset_x, (window_width - frame.width) * 0.5f);
        frame.y = std::max(
            floating_bounds.y,
            floating_bounds.bottom() - frame.height - inset_y
        );
        break;
      case WorkspaceFloatingPlacement::BottomRight:
        frame.x = std::max(
            floating_bounds.x,
            floating_bounds.right() - frame.width - inset_x
        );
        frame.y = std::max(
            floating_bounds.y,
            floating_bounds.bottom() - frame.height - inset_y
        );
        break;
      case WorkspaceFloatingPlacement::Absolute:
      default:
        break;
    }

    const ui::UIRect clamped = ui::clamp_rect_to_bounds(
        ui::UIRect{
            .x = frame.x,
            .y = frame.y,
            .width = frame.width,
            .height = frame.height,
        },
        floating_bounds
    );
    return WorkspacePanelResolvedFrame{
        .x = clamped.x,
        .y = clamped.y,
        .width = clamped.width,
        .height = clamped.height,
    };
  }

  if (m_active_snapshot.has_value()) {
    const auto it =
        m_active_snapshot->panels.find(std::string(panel_instance_id));
    if (it != m_active_snapshot->panels.end() &&
        it->second.floating_frame.has_value() &&
        it->second.floating_frame->valid()) {
      const ui::UIRect clamped = ui::clamp_rect_to_bounds(
          ui::UIRect{
              .x = it->second.floating_frame->x,
              .y = it->second.floating_frame->y,
              .width = std::max(
                  it->second.floating_frame->width, minimum_size.width
              ),
              .height = std::max(
                  it->second.floating_frame->height, minimum_size.height
              ),
          },
          floating_bounds
      );
      return WorkspacePanelResolvedFrame{
          .x = clamped.x,
          .y = clamped.y,
          .width = clamped.width,
          .height = clamped.height,
      };
    }
  }

  if (const auto *panel = find_workspace_panel_spec(workspace, panel_instance_id);
      panel != nullptr && panel->floating_frame.has_value() &&
      panel->floating_frame->valid()) {
    const ui::UIRect clamped = ui::clamp_rect_to_bounds(
        ui::UIRect{
            .x = resolve_axis(panel->floating_frame->x, window_width),
            .y = resolve_axis(panel->floating_frame->y, window_height),
            .width = std::max(
                resolve_axis(panel->floating_frame->width, window_width),
                minimum_size.width
            ),
            .height = std::max(
                resolve_axis(panel->floating_frame->height, window_height),
                minimum_size.height
            ),
        },
        floating_bounds
    );
    return WorkspacePanelResolvedFrame{
        .x = clamped.x,
        .y = clamped.y,
        .width = clamped.width,
        .height = clamped.height,
    };
  }

  const auto order_it = std::find(
      m_panel_order.begin(), m_panel_order.end(), panel_instance_id
  );
  const size_t panel_index = order_it != m_panel_order.end()
                                 ? static_cast<size_t>(
                                       std::distance(m_panel_order.begin(), order_it)
                                   )
                                 : 0u;
  const ui::UIRect clamped = ui::clamp_rect_to_bounds(
      ui::UIRect{
          .x = floating_bounds.x + 48.0f +
               static_cast<float>(panel_index) * 36.0f,
          .y = floating_bounds.y + 56.0f +
               static_cast<float>(panel_index) * 28.0f,
          .width = std::max(720.0f, minimum_size.width),
          .height = std::max(320.0f, minimum_size.height),
      },
      floating_bounds
  );
  return WorkspacePanelResolvedFrame{
      .x = clamped.x,
      .y = clamped.y,
      .width = clamped.width,
      .height = clamped.height,
  };
}

std::optional<WorkspaceShellSystem::DockDropZone>
WorkspaceShellSystem::detect_dock_drop_zone(
    std::string_view panel_instance_id,
    glm::vec2 cursor
) const {
  std::optional<DockDropZone> best_zone;
  float best_distance = std::numeric_limits<float>::max();
  for (const auto &zone : dock_drop_zones(panel_instance_id)) {
    if (!zone.target_rect.contains(cursor)) {
      continue;
    }

    const glm::vec2 center =
        glm::vec2(zone.target_rect.x + zone.target_rect.width * 0.5f,
                  zone.target_rect.y + zone.target_rect.height * 0.5f);
    const float distance = glm::length(center - cursor);
    if (!best_zone.has_value() || distance < best_distance) {
      best_zone = zone;
      best_distance = distance;
    }
  }

  return best_zone;
}

void WorkspaceShellSystem::dock_panel_to_edge(
    std::string_view panel_instance_id,
    WorkspaceDockEdge edge
) {
  if (!m_active_snapshot.has_value()) {
    return;
  }

  auto state_it = m_active_snapshot->panels.find(std::string(panel_instance_id));
  if (state_it == m_active_snapshot->panels.end()) {
    return;
  }

  if (edge == WorkspaceDockEdge::Center) {
    for (auto &[instance_id, panel] : m_active_snapshot->panels) {
      if (instance_id == panel_instance_id || !panel.dock_slot.has_value() ||
          panel.dock_slot->edge != WorkspaceDockEdge::Center) {
        continue;
      }

      panel.dock_slot.reset();
      if (auto mounted = m_panels.find(instance_id); mounted != m_panels.end()) {
        mounted->second.spec.dock_slot.reset();
      }
    }

    state_it->second.dock_slot = WorkspaceDockSlot{
        .edge = WorkspaceDockEdge::Center,
        .extent = 0.0f,
        .order = 0,
    };
    if (auto mounted = m_panels.find(std::string(panel_instance_id));
        mounted != m_panels.end()) {
      mounted->second.spec.dock_slot = state_it->second.dock_slot;
    }

    m_needs_rebuild = true;
    m_needs_save = true;
    m_save_accumulator = 0.0;
    return;
  }

  const DockLayoutMetrics metrics = compute_dock_layout_metrics(panel_instance_id);
  float extent = edge == WorkspaceDockEdge::Left
                     ? metrics.left_extent
                 : edge == WorkspaceDockEdge::Top
                     ? metrics.top_extent
                 : edge == WorkspaceDockEdge::Right
                     ? metrics.right_extent
                     : metrics.bottom_extent;
  if (extent <= 0.0f) {
    const PanelMinimumSize minimum = panel_minimum_size(panel_instance_id);
    extent = dock_edge_is_horizontal(edge)
                 ? std::max(200.0f, minimum.height)
                 : std::max(280.0f, minimum.width);
  }

  auto node_it = m_floating_panel_nodes.find(std::string(panel_instance_id));
  if (node_it != m_floating_panel_nodes.end() && m_document != nullptr) {
    if (const auto *node = m_document->node(node_it->second); node != nullptr) {
      extent = std::max(extent, dock_axis_extent(node->layout.bounds, edge));
    }
  }

  int next_order = 0;
  for (const auto &[instance_id, panel] : m_active_snapshot->panels) {
    if (instance_id == panel_instance_id || !panel.dock_slot.has_value() ||
        panel.dock_slot->edge != edge) {
      continue;
    }

    next_order = std::max(next_order, panel.dock_slot->order + 1);
  }

  state_it->second.dock_slot = WorkspaceDockSlot{
      .edge = edge,
      .extent = extent,
      .order = next_order,
  };
  if (auto mounted = m_panels.find(std::string(panel_instance_id));
      mounted != m_panels.end()) {
    mounted->second.spec.dock_slot = state_it->second.dock_slot;
  }

  m_needs_rebuild = true;
  m_needs_save = true;
  m_save_accumulator = 0.0;
}

void WorkspaceShellSystem::undock_panel(std::string_view panel_instance_id) {
  if (!m_active_snapshot.has_value()) {
    return;
  }

  auto state_it = m_active_snapshot->panels.find(std::string(panel_instance_id));
  if (state_it == m_active_snapshot->panels.end() ||
      !state_it->second.dock_slot.has_value()) {
    return;
  }

  auto node_it = m_floating_panel_nodes.find(std::string(panel_instance_id));
  if (node_it != m_floating_panel_nodes.end() && m_document != nullptr) {
    if (const auto *node = m_document->node(node_it->second); node != nullptr) {
      state_it->second.floating_frame = WorkspacePanelResolvedFrame{
          .x = node->layout.bounds.x,
          .y = node->layout.bounds.y,
          .width = node->layout.bounds.width,
          .height = node->layout.bounds.height,
      };
    }
  } else {
    state_it->second.floating_frame = resolve_workspace_panel_frame(panel_instance_id);
  }

  state_it->second.dock_slot.reset();
  if (auto mounted = m_panels.find(std::string(panel_instance_id));
      mounted != m_panels.end()) {
    mounted->second.spec.dock_slot.reset();
  }

  m_needs_save = true;
  m_save_accumulator = 0.0;
}

void WorkspaceShellSystem::sync_dock_drop_preview() {
  if (m_document == nullptr) {
    m_dock_drop_preview_node = ui::k_invalid_node_id;
    return;
  }

  if (m_dock_drop_preview_node != ui::k_invalid_node_id &&
      m_document->node(m_dock_drop_preview_node) != nullptr) {
    m_document->destroy_subtree(m_dock_drop_preview_node);
  }
  m_dock_drop_preview_node = ui::k_invalid_node_id;

  if (!active_workspace_uses_floating_panels() || !m_dock_drag_state.has_value()) {
    return;
  }

  m_dock_drop_preview_node = ui::dsl::append(
      *m_document,
      m_document->root(),
      build_dock_drop_preview(
          m_dock_drag_state->panel_instance_id,
          m_dock_drag_state->active_drop_zone
      )
  );
}

void WorkspaceShellSystem::sync_dock_drag_state() {
  if (!active_workspace_uses_floating_panels() || !m_active_snapshot.has_value() ||
      m_document == nullptr) {
    m_dock_drag_state.reset();
    sync_dock_drop_preview();
    return;
  }

  auto system_manager = SystemManager::get();
  auto *ui_system =
      system_manager != nullptr ? system_manager->get_system<UISystem>() : nullptr;
  const auto *active_drag =
      ui_system != nullptr && ui_system->active_panel_move_drag().has_value()
          ? &*ui_system->active_panel_move_drag()
          : nullptr;

  if (active_drag == nullptr || !ui_system->has_last_pointer_position()) {
    if (m_dock_drag_state.has_value()) {
      const std::optional<DockDropZone> final_zone =
          ui_system != nullptr && ui_system->has_last_pointer_position()
              ? detect_dock_drop_zone(
                    m_dock_drag_state->panel_instance_id,
                    ui_system->last_pointer_position()
                )
              : m_dock_drag_state->active_drop_zone;
      if (final_zone.has_value()) {
        dock_panel_to_edge(
            m_dock_drag_state->panel_instance_id,
            final_zone->edge
        );
        m_dock_drag_state->layout_changed = true;
      }
      m_needs_rebuild = m_needs_rebuild || m_dock_drag_state->layout_changed;
      m_dock_drag_state.reset();
    }
    sync_dock_drop_preview();
    return;
  }

  const glm::vec2 pointer = ui_system->last_pointer_position();
  if (glm::length(pointer - active_drag->start_pointer) < 2.0f) {
    return;
  }

  auto node_it = std::find_if(
      m_floating_panel_nodes.begin(),
      m_floating_panel_nodes.end(),
      [&active_drag](const auto &entry) {
        return entry.second == active_drag->panel_target.node_id;
      }
  );
  if (node_it == m_floating_panel_nodes.end()) {
    return;
  }

  const std::string &instance_id = node_it->first;
  if (!m_dock_drag_state.has_value() ||
      m_dock_drag_state->panel_instance_id != instance_id) {
    const bool started_docked = panel_dock_slot(instance_id).has_value();
    if (panel_dock_slot(instance_id).has_value()) {
      const ui::UIRect docked_bounds = active_drag->start_bounds;
      std::optional<WorkspacePanelResolvedFrame> prior_floating_frame;
      if (auto state_it = m_active_snapshot->panels.find(instance_id);
          state_it != m_active_snapshot->panels.end() &&
          state_it->second.floating_frame.has_value() &&
          state_it->second.floating_frame->valid()) {
        prior_floating_frame = state_it->second.floating_frame;
      }
      undock_panel(instance_id);

      auto panel_state_it = m_active_snapshot->panels.find(instance_id);
      if (panel_state_it != m_active_snapshot->panels.end()) {
        panel_state_it->second.floating_frame = prior_floating_frame;
        WorkspacePanelResolvedFrame floating_frame =
            resolve_floating_panel_frame(instance_id);
        const DockLayoutMetrics metrics = compute_dock_layout_metrics(instance_id);
        const ui::UIRect floating_bounds =
            metrics.floating_bounds.width > 0.0f ||
                    metrics.floating_bounds.height > 0.0f
                ? metrics.floating_bounds
                : metrics.workspace_bounds;
        const float grab_x = std::clamp(
            active_drag->start_pointer.x - docked_bounds.x,
            24.0f,
            std::max(24.0f, floating_frame.width - 24.0f)
        );
        const float grab_y = std::clamp(
            active_drag->start_pointer.y - docked_bounds.y,
            18.0f,
            std::max(18.0f, floating_frame.height - 18.0f)
        );
        const ui::UIRect clamped = ui::clamp_rect_to_bounds(
            ui::UIRect{
                .x = pointer.x - grab_x,
                .y = pointer.y - grab_y,
                .width = floating_frame.width,
                .height = floating_frame.height,
            },
            floating_bounds
        );
        floating_frame = WorkspacePanelResolvedFrame{
            .x = clamped.x,
            .y = clamped.y,
            .width = clamped.width,
            .height = clamped.height,
        };
        panel_state_it->second.floating_frame = floating_frame;

        if (m_document != nullptr) {
          reconcile_floating_panel_node_bounds(
              *m_document, node_it->second, floating_frame
          );
          m_document->mark_layout_dirty();
        }

        if (ui_system != nullptr) {
          auto &drag_state = ui_system->active_panel_move_drag_mut();
          if (drag_state.has_value() &&
              drag_state->panel_target.node_id == active_drag->panel_target.node_id) {
            drag_state->start_pointer = pointer;
            drag_state->start_bounds = ui::UIRect{
                .x = floating_frame.x,
                .y = floating_frame.y,
                .width = floating_frame.width,
                .height = floating_frame.height,
            };
          }
        }
      }
    }

    m_dock_drag_state = DockDragState{
        .panel_instance_id = instance_id,
        .layout_changed = started_docked,
        .started_docked = started_docked,
    };
    sync_dock_drop_preview();
  }

  const std::optional<DockDropZone> next_zone =
      detect_dock_drop_zone(instance_id, pointer);
  const bool zone_changed =
      (m_dock_drag_state->active_drop_zone.has_value() != next_zone.has_value()) ||
      (m_dock_drag_state->active_drop_zone.has_value() && next_zone.has_value() &&
       (m_dock_drag_state->active_drop_zone->edge != next_zone->edge ||
        !rect_equal(
            m_dock_drag_state->active_drop_zone->target_rect,
            next_zone->target_rect
        )));
  if (zone_changed) {
    m_dock_drag_state->active_drop_zone = next_zone;
    sync_dock_drop_preview();
    return;
  }

  if (m_dock_drop_preview_node == ui::k_invalid_node_id) {
    sync_dock_drop_preview();
  }
}

void WorkspaceShellSystem::set_panel_open(
    std::string_view panel_instance_id,
    bool open
) {
  if (!m_active_snapshot.has_value()) {
    return;
  }

  if (!open && !panel_instance_toggleable(panel_instance_id)) {
    return;
  }

  auto it = m_active_snapshot->panels.find(std::string(panel_instance_id));
  if (it == m_active_snapshot->panels.end() || it->second.open == open) {
    return;
  }

  it->second.open = open;
  if (open && active_workspace_uses_floating_panels() &&
      !it->second.floating_frame.has_value() &&
      !it->second.dock_slot.has_value()) {
    it->second.floating_frame = resolve_floating_panel_frame(panel_instance_id);
  }

  const std::string instance_id(panel_instance_id);

  if (active_workspace_uses_floating_panels() && m_document != nullptr) {
    if (!open) {
      auto mounted = m_panels.find(instance_id);
      if (mounted != m_panels.end() && mounted->second.controller != nullptr) {
        auto ctx = m_store->create_context();
        mounted->second.controller->save_state(ctx);
        it->second.state_blob = m_store->encode_panel_state(ctx);
        mounted->second.controller->unmount();
        reset_mounted_panel_runtime(mounted->second);
        m_panels.erase(mounted);
      }

      auto node_it = m_floating_panel_nodes.find(instance_id);
      if (node_it != m_floating_panel_nodes.end()) {
        m_document->destroy_subtree(node_it->second);
        m_floating_panel_nodes.erase(node_it);
      }
    } else {
      if (!m_panels.contains(instance_id)) {
        const auto *provider = panel_registry()->find(it->second.provider_id);
        if (provider != nullptr && provider->factory) {
          auto controller = provider->factory();
          if (controller != nullptr) {
            if (auto panel_state =
                    m_store->decode_panel_state(it->second.state_blob);
                panel_state.has_value()) {
              controller->load_state(*panel_state);
            }
            m_panels.emplace(
                instance_id,
                MountedPanel{
                    .spec =
                        PanelInstanceSpec{
                    .instance_id = instance_id,
                    .provider_id = it->second.provider_id,
                    .title = it->second.title,
                    .open = true,
                    .dock_slot = it->second.dock_slot,
                },
            .controller = std::move(controller),
        }
            );
          }
        }
      }

      ui::dsl::append(
          *m_document, m_document->root(), build_floating_panel(instance_id)
      );

      auto mounted = m_panels.find(instance_id);
      if (mounted != m_panels.end() && mounted->second.controller != nullptr) {
        mount_rendered_panel(instance_id, mounted->second);
      }
    }

    publish_workspace_ui_state();
    m_needs_save = true;
    m_save_accumulator = 0.0;
    sync_root_visibility();
    return;
  }

  if (!open) {
    auto mounted = m_panels.find(it->first);
    if (mounted != m_panels.end() && mounted->second.controller != nullptr) {
      auto ctx = m_store->create_context();
      mounted->second.controller->save_state(ctx);
      it->second.state_blob = m_store->encode_panel_state(ctx);
    }
  }

  mount_panels_from_snapshot();
  publish_workspace_ui_state();
  m_needs_rebuild = true;
  m_needs_save = true;
  m_save_accumulator = 0.0;
  sync_root_visibility();
}

} // namespace astralix::editor
