#include "systems/workspace-shell-system-internal.hpp"

namespace astralix::editor {
using namespace workspace_shell_detail;

ui::dsl::NodeSpec WorkspaceShellSystem::build_shell() {
  const auto &theme = workspace_shell_theme();

  if (active_workspace_uses_floating_panels()) {
    return build_floating_shell();
  }

  return ui::dsl::view()
      .style(ui::dsl::styles::fill().padding(2.0f).background(theme.backdrop))
      .child(
          layout_node_visible(m_active_snapshot->root)
              ? build_layout_node(m_active_snapshot->root, std::string(k_root_path))
              : build_empty_workspace_state(
                    "No panels open",
                    "Use the toolbar pane to reopen a tool in this workspace."
                )
      );
}

ui::dsl::NodeSpec WorkspaceShellSystem::build_floating_shell() {
  auto root = ui::dsl::view().style(ui::dsl::styles::fill());
  if (m_active_snapshot.has_value()) {
    for (const auto &instance_id : m_panel_order) {
      if (panel_instance_open(instance_id) &&
          panel_dock_slot(instance_id).has_value()) {
        root.child(build_floating_panel(instance_id));
      }
    }

    for (const auto &instance_id : m_panel_order) {
      if (panel_instance_open(instance_id) &&
          !panel_dock_slot(instance_id).has_value()) {
        root.child(build_floating_panel(instance_id));
      }
    }
  }

  return root;
}

ui::dsl::NodeSpec WorkspaceShellSystem::build_dock_drop_preview(
    std::string_view panel_instance_id,
    const std::optional<DockDropZone> &active_zone
) const {
  using namespace ui::dsl;
  using namespace ui::dsl::styles;
  const auto &theme = workspace_shell_theme();
  const std::vector<DockDropZone> zones = dock_drop_zones(panel_instance_id);

  auto overlay = view().enabled(false).style(fill());
  if (active_zone.has_value()) {
    overlay.child(
        view()
            .enabled(false)
            .style(
                absolute()
                    .left(px(active_zone->preview_rect.x))
                    .top(px(active_zone->preview_rect.y))
                    .width(px(active_zone->preview_rect.width))
                    .height(px(active_zone->preview_rect.height))
                    .background(
                        glm::vec4(
                            theme.accent.r, theme.accent.g, theme.accent.b, 0.10f
                        )
                    )
                    .radius(18.0f)
                    .border(
                        2.0f,
                        glm::vec4(
                            theme.accent.r, theme.accent.g, theme.accent.b, 0.72f
                        )
                    )
            )
    );
  }

  for (const auto &zone : zones) {
    const bool active =
        active_zone.has_value() && active_zone->edge == zone.edge;
    overlay.child(
        view()
            .enabled(false)
            .style(
                absolute()
                    .left(px(zone.target_rect.x))
                    .top(px(zone.target_rect.y))
                    .width(px(zone.target_rect.width))
                    .height(px(zone.target_rect.height))
                    .background(
                        active
                            ? glm::vec4(
                                  theme.accent.r,
                                  theme.accent.g,
                                  theme.accent.b,
                                  0.28f
                              )
                            : glm::vec4(
                                  theme.panel_raised_background.r,
                                  theme.panel_raised_background.g,
                                  theme.panel_raised_background.b,
                                  0.92f
                              )
                    )
                    .radius(14.0f)
                    .border(
                        2.0f,
                        active
                            ? glm::vec4(
                                  theme.accent.r,
                                  theme.accent.g,
                                  theme.accent.b,
                                  0.88f
                              )
                            : glm::vec4(
                                  theme.accent.r,
                                  theme.accent.g,
                                  theme.accent.b,
                                  0.38f
                              )
                    )
            )
    );
  }

  return overlay;
}

ui::dsl::NodeSpec WorkspaceShellSystem::build_layout_node(
    const LayoutNode &node,
    const std::string &path
) {
  using namespace ui::dsl;
  using namespace ui::dsl::styles;
  const auto &theme = workspace_shell_theme();

  switch (node.kind) {
    case LayoutNodeKind::Split: {
      const bool first_visible =
          node.first != nullptr && layout_node_visible(*node.first);
      const bool second_visible =
          node.second != nullptr && layout_node_visible(*node.second);

      if (!first_visible && !second_visible) {
        return build_empty_workspace_state(
            "No panels open",
            "Use the toolbar pane to reopen a tool in this workspace."
        );
      }

      if (!first_visible && node.second != nullptr) {
        return build_layout_node(*node.second, path + "/1");
      }

      if (!second_visible && node.first != nullptr) {
        return build_layout_node(*node.first, path + "/0");
      }

      auto &runtime = m_split_runtime_nodes[path];
      auto container = node.split_axis == ui::FlexDirection::Row
                           ? ui::dsl::row()
                           : ui::dsl::column();
      const bool has_fixed_first_extent =
          node.split_behavior.first_extent.unit != ui::UILengthUnit::Auto;
      const float split_ratio = std::clamp(node.split_ratio, 0.1f, 0.9f);
      const PanelMinimumSize first_minimum =
          node.first != nullptr ? layout_node_minimum_size(*node.first)
                                : PanelMinimumSize{};
      const PanelMinimumSize second_minimum =
          node.second != nullptr ? layout_node_minimum_size(*node.second)
                                 : PanelMinimumSize{};
      auto first_style =
          (has_fixed_first_extent ? shrink() : shrink())
              .basis(
                  has_fixed_first_extent
                      ? node.split_behavior.first_extent
                      : ui::UILength::percent(split_ratio)
              )
              .min_width(ui::UILength::pixels(first_minimum.width))
              .min_height(ui::UILength::pixels(first_minimum.height));
      if (node.split_axis == ui::FlexDirection::Row) {
        first_style.height(ui::UILength::percent(1.0f));
        if (has_fixed_first_extent &&
            node.split_behavior.first_extent.unit == ui::UILengthUnit::Pixels &&
            !node.split_behavior.resizable) {
          const float locked_width = std::max(
              node.split_behavior.first_extent.value, first_minimum.width
          );
          first_style
              .basis(px(locked_width))
              .min_width(px(locked_width))
              .max_width(px(locked_width));
        }
      } else {
        first_style.width(ui::UILength::percent(1.0f));
        if (has_fixed_first_extent &&
            node.split_behavior.first_extent.unit == ui::UILengthUnit::Pixels &&
            !node.split_behavior.resizable) {
          const float locked_height = std::max(
              node.split_behavior.first_extent.value, first_minimum.height
          );
          first_style
              .basis(px(locked_height))
              .min_height(px(locked_height))
              .max_height(px(locked_height));
        }
      }
      auto first = ui::dsl::view()
                       .bind(runtime.first)
                       .style(std::move(first_style));
      first.child(build_layout_node(*node.first, path + "/0"));

      auto second_style = flex(1.0f)
                              .min_width(ui::UILength::pixels(second_minimum.width))
                              .min_height(ui::UILength::pixels(second_minimum.height));
      auto second = ui::dsl::view()
                        .bind(runtime.second)
                        .style(std::move(second_style));
      second.child(build_layout_node(*node.second, path + "/1"));

      if (!node.split_behavior.show_divider) {
        return container.style(fill().gap(0.0f))
            .children(std::move(first), std::move(second));
      }

      if (!node.split_behavior.resizable) {
        auto divider = node.split_axis == ui::FlexDirection::Row
                           ? ui::dsl::view().style(
                                 width(px(6.0f))
                                     .height(ui::UILength::percent(1.0f))
                                     .background(theme.accent_soft)
                             )
                           : ui::dsl::view().style(
                                 width(ui::UILength::percent(1.0f))
                                     .height(px(6.0f))
                                     .background(theme.accent_soft)
                             );
        return container.style(fill().gap(0.0f)).children(std::move(first), std::move(divider), std::move(second));
      }

      return container.style(fill().gap(0.0f)).children(std::move(first), splitter().style(background(theme.accent_soft)), std::move(second));
    }

    case LayoutNodeKind::Tabs: {
      std::vector<std::string> visible_tabs;
      visible_tabs.reserve(node.tabs.size());

      for (const auto &tab_id : node.tabs) {
        const auto it = m_active_snapshot->panels.find(tab_id);
        if (it != m_active_snapshot->panels.end() && it->second.open) {
          visible_tabs.push_back(tab_id);
        }
      }

      auto header_row = ui::dsl::row().style(
          fill_x()
              .padding(10.0f)
              .gap(8.0f)
              .items_center()
              .background(theme.panel_background)
      );

      if (visible_tabs.empty()) {
        return build_empty_workspace_state(
            "No panels open",
            "Use the toolbar pane to reopen a tool in this workspace."
        );
      } else {
        const std::string active_tab_id = resolved_active_tab(node);
        for (const auto &tab_id : visible_tabs) {
          const auto panel_it = m_active_snapshot->panels.find(tab_id);
          const std::string title =
              panel_it != m_active_snapshot->panels.end() &&
                      !panel_it->second.title.empty()
                  ? panel_it->second.title
                  : tab_id;
          const bool active = tab_id == active_tab_id;
          header_row.child(
              button(
                  title,
                  [this, path, tab_id]() {
                    m_pending_tab_activation =
                        PendingTabActivation{.path = path, .tab_id = tab_id};
                  }
              )
                  .style(
                      padding_xy(12.0f, 8.0f)
                          .radius(10.0f)
                          .background(
                              active ? theme.panel_raised_background
                                     : theme.panel_background
                          )
                          .border(
                              1.0f, active ? theme.accent : theme.panel_border
                          )
                          .text_color(
                              active ? theme.text_primary : theme.text_muted
                          )
                          .hover(
                              ui::dsl::styles::state().background(
                                  theme.panel_raised_background
                              )
                          )
                          .pressed(
                              ui::dsl::styles::state().background(
                                  theme.accent_soft
                              )
                          )
                          .focused(
                              ui::dsl::styles::state().border(2.0f, theme.accent)
                          )
                  )
          );
        }
      }

      auto content = ui::dsl::view().style(fill_x().flex(1.0f));
      const std::string active_tab_id = resolved_active_tab(node);
      if (!active_tab_id.empty()) {
        content.child(build_leaf_panel(active_tab_id));
      } else {
        content.child(
            ui::dsl::column()
                .style(
                    fill()
                        .justify_center()
                        .items_center()
                        .background(theme.panel_background)
                )
                .children(
                    text("No active panel")
                        .style(font_size(18.0f).text_color(theme.text_primary)),
                    text("Use the toolbar pane to reopen a tool in this workspace.")
                        .style(font_size(13.0f).text_color(theme.text_muted))
                )
        );
      }

      return ui::dsl::column()
          .style(
              fill()
                  .gap(0.0f)
                  .background(theme.panel_background)
                  .radius(16.0f)
                  .border(1.0f, theme.panel_border)
          )
          .children(std::move(header_row), std::move(content));
    }

    case LayoutNodeKind::Leaf:
    default:
      return build_leaf_panel(node.panel_instance_id);
  }
}

ui::dsl::NodeSpec
WorkspaceShellSystem::build_leaf_panel(std::string_view panel_instance_id) {
  using namespace ui::dsl;
  using namespace ui::dsl::styles;
  const auto &theme = workspace_shell_theme();

  const std::string instance_id(panel_instance_id);
  const PanelMinimumSize minimum_size = panel_minimum_size(panel_instance_id);
  const auto panel_it = m_active_snapshot->panels.find(instance_id);
  const bool open =
      panel_it != m_active_snapshot->panels.end() ? panel_it->second.open : false;
  const std::string title =
      panel_it != m_active_snapshot->panels.end() && !panel_it->second.title.empty()
          ? panel_it->second.title
          : instance_id;
  const bool has_shell_frame = panel_instance_has_shell_frame(panel_instance_id);

  auto build_header = [&]() {
    return ui::dsl::row()
        .style(
            fill_x()
                .padding_xy(14.0f, 12.0f)
                .items_center()
                .gap(12.0f)
                .background(theme.panel_raised_background)
                .border(0.0f, theme.panel_border)
        )
        .children(
            text(title)
                .style(font_size(14.0f).text_color(theme.text_primary)),
            spacer(),
            build_panel_close_button([this, instance_id]() {
              m_pending_panel_visibility.emplace_back(instance_id, false);
            })
        );
  };

  if (!open) {
    auto panel = ui::dsl::column().style(
        fill()
            .gap(0.0f)
            .min_width(px(minimum_size.width))
            .min_height(px(minimum_size.height))
            .background(theme.panel_background)
            .radius(16.0f)
            .border(1.0f, theme.panel_border)
    );

    if (has_shell_frame) {
      panel.child(build_header());
    }

    panel.child(
        ui::dsl::column()
            .style(
                fill_x()
                    .flex(1.0f)
                    .justify_center()
                    .items_center()
                    .gap(10.0f)
                    .padding(20.0f)
            )
            .children(
                text("Panel closed")
                    .style(font_size(18.0f).text_color(theme.text_primary)),
                button(
                    "Reopen",
                    [this, instance_id]() {
                      m_pending_panel_visibility.emplace_back(instance_id, true);
                    }
                )
                    .style(
                        padding_xy(12.0f, 8.0f)
                            .radius(10.0f)
                            .background(theme.accent_soft)
                            .border(1.0f, theme.accent)
                            .text_color(theme.text_primary)
                    )
            )
    );
    return panel;
  }

  const auto mounted_it = m_panels.find(instance_id);
  if (mounted_it == m_panels.end() || mounted_it->second.controller == nullptr) {
    auto panel = ui::dsl::column().style(
        fill()
            .gap(0.0f)
            .min_width(px(minimum_size.width))
            .min_height(px(minimum_size.height))
            .background(theme.panel_background)
            .radius(16.0f)
            .border(1.0f, theme.panel_border)
    );

    if (has_shell_frame) {
      panel.child(build_header());
    }

    panel.child(
        ui::dsl::column()
            .style(
                fill_x()
                    .flex(1.0f)
                    .justify_center()
                    .items_center()
                    .padding(20.0f)
                    .gap(8.0f)
            )
            .children(
                text("Missing panel provider")
                    .style(font_size(18.0f).text_color(theme.text_primary)),
                text(
                    "The panel is registered in the workspace, but its provider is not available."
                )
                    .style(font_size(13.0f).text_color(theme.text_muted))
            )
    );
    return panel;
  }

  auto panel = ui::dsl::column().style(
      fill()
          .gap(0.0f)
          .min_width(px(minimum_size.width))
          .min_height(px(minimum_size.height))
          .background(theme.panel_background)
          .radius(16.0f)
          .border(1.0f, theme.panel_border)
  );

  if (has_shell_frame) {
    panel.child(build_header());
  }

  panel.child(
      ui::dsl::view()
          .bind(mounted_it->second.content_host_node)
          .style(fill_x().flex(1.0f).min_height(px(0.0f)).overflow_hidden())
  );
  return panel;
}

ui::dsl::NodeSpec
WorkspaceShellSystem::build_floating_panel(std::string_view panel_instance_id) {
  using namespace ui::dsl;
  using namespace ui::dsl::styles;
  const auto &theme = workspace_shell_theme();

  const std::string instance_id(panel_instance_id);
  const auto panel_it = m_active_snapshot->panels.find(instance_id);
  const std::string title =
      panel_it != m_active_snapshot->panels.end() && !panel_it->second.title.empty()
          ? panel_it->second.title
          : instance_id;
  const PanelMinimumSize minimum_size = panel_minimum_size(panel_instance_id);
  const WorkspacePanelResolvedFrame frame =
      resolve_workspace_panel_frame(panel_instance_id);
  const bool has_shell_frame = panel_instance_has_shell_frame(panel_instance_id);
  const auto *workspace = workspace_registry()->find(m_active_workspace_id);
  const auto *panel_spec = find_workspace_panel_spec(workspace, panel_instance_id);
  const auto dock_slot = panel_dock_slot(panel_instance_id);
  const bool floating_draggable =
      panel_spec != nullptr ? panel_spec->floating_draggable : true;
  const bool floating_resizable =
      panel_spec != nullptr ? panel_spec->floating_resizable : true;
  const float floating_shell_opacity =
      panel_spec != nullptr ? panel_spec->floating_shell_opacity : 1.0f;

  auto scaled_alpha = [floating_shell_opacity](glm::vec4 color) {
    color.a *= floating_shell_opacity;
    return color;
  };

  auto floating_panel_style = [&]() {
    auto style = absolute()
                     .left(px(frame.x))
                     .top(px(frame.y))
                     .width(px(frame.width))
                     .height(px(frame.height))
                     .min_width(px(minimum_size.width))
                     .min_height(px(minimum_size.height))
                     .overflow_hidden()
                     .background(scaled_alpha(theme.panel_background))
                     .radius(16.0f)
                     .border(1.0f, scaled_alpha(theme.panel_border));
    if (dock_slot.has_value()) {
      style.handle(12.0f);
    }

    if (floating_resizable) {
      if (dock_slot.has_value()) {
        switch (dock_slot->edge) {
          case WorkspaceDockEdge::Left:
            style.resizable(ui::ResizeMode::Horizontal, ui::k_resize_edge_right);
            break;
          case WorkspaceDockEdge::Top:
            style.resizable(ui::ResizeMode::Vertical, ui::k_resize_edge_bottom);
            break;
          case WorkspaceDockEdge::Right:
            style.resizable(ui::ResizeMode::Horizontal, ui::k_resize_edge_left);
            break;
          case WorkspaceDockEdge::Bottom:
            style.resizable(ui::ResizeMode::Vertical, ui::k_resize_edge_top);
            break;
          case WorkspaceDockEdge::Center:
            break;
        }
      } else {
        style.resizable_all();
      }
    }
    if (floating_draggable) {
      style.draggable();
    }

    return style;
  };

  auto build_header = [&]() {
    return ui::dsl::row()
        .style(
            fill_x()
                .padding_xy(14.0f, 12.0f)
                .items_center()
                .gap(12.0f)
                .background(scaled_alpha(theme.panel_raised_background))
                .border(0.0f, scaled_alpha(theme.panel_border))
                .drag_handle()
        )
        .children(
            text(title)
                .style(font_size(14.0f).text_color(theme.text_primary)),
            spacer(),
            build_panel_close_button([this, instance_id]() {
              m_pending_panel_visibility.emplace_back(instance_id, false);
            })
        );
  };

  const auto mounted_it = m_panels.find(instance_id);
  if (mounted_it == m_panels.end() || mounted_it->second.controller == nullptr) {
    auto panel = ui::dsl::column()
                     .bind(m_floating_panel_nodes[instance_id])
                     .style(floating_panel_style());

    if (has_shell_frame) {
      panel.child(build_header());
    }

    panel.child(
        ui::dsl::column()
            .style(
                fill_x()
                    .flex(1.0f)
                    .justify_center()
                    .items_center()
                    .padding(20.0f)
                    .gap(8.0f)
            )
            .children(
                text("Missing panel provider")
                    .style(font_size(18.0f).text_color(theme.text_primary)),
                text(
                    "The panel is registered in the workspace, but its provider is not available."
                )
                    .style(font_size(13.0f).text_color(theme.text_muted))
            )
    );
    return panel;
  }

  auto panel = ui::dsl::column()
                   .bind(m_floating_panel_nodes[instance_id])
                   .style(floating_panel_style());

  if (has_shell_frame) {
    panel.child(build_header());
  }

  panel.child(
      ui::dsl::view()
          .bind(mounted_it->second.content_host_node)
          .style(fill_x().flex(1.0f).min_height(px(0.0f)).overflow_hidden())
  );
  return panel;
}

} // namespace astralix::editor
