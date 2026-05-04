#include "systems/workspace-shell-system-internal.hpp"

namespace astralix::editor::workspace_shell_detail {

const WorkspaceShellTheme &workspace_shell_theme() {
  static const WorkspaceShellTheme theme{};
  return theme;
}

void configure_workspace_ui_root(
    rendering::UIRoot &root,
    const Ref<ui::UIDocument> &document,
    const ResourceDescriptorID &default_font_id,
    float default_font_size
) {
  root.document = document;
  root.default_font_id = default_font_id;
  root.default_font_size = default_font_size;
  root.sort_order = 200;
}

std::optional<EntityID> find_workspace_shell_root_entity(ecs::World &world) {
  std::optional<EntityID> root_entity_id;
  std::vector<EntityID> duplicate_roots;

  world.each<rendering::UIRoot>([&](EntityID entity_id, rendering::UIRoot &) {
    if (world.entity(entity_id).name() != k_workspace_shell_root_name) {
      return;
    }

    if (!root_entity_id.has_value()) {
      root_entity_id = entity_id;
      return;
    }

    duplicate_roots.push_back(entity_id);
  });

  for (EntityID duplicate_root_id : duplicate_roots) {
    world.destroy(duplicate_root_id);
  }

  return root_entity_id;
}

ui::dsl::StyleBuilder panel_close_button_style() {
  using namespace ui::dsl::styles;
  const auto &theme = workspace_shell_theme();

  return width(px(34.0f))
      .height(px(34.0f))
      .padding(8.0f)
      .radius(10.0f)
      .background(theme.panel_background)
      .border(1.0f, theme.panel_border)
      .cursor_pointer()
      .hover(state().background(theme.accent_soft))
      .pressed(state().background(theme.panel_raised_background))
      .focused(state().border(2.0f, theme.accent));
}

ui::dsl::NodeSpec build_panel_close_button(std::function<void()> on_click) {
  return ui::dsl::icon_button("icons::close", std::move(on_click))
      .style(panel_close_button_style());
}

bool path_is_root(std::string_view path) { return path == k_root_path; }

bool panel_frames_equal(
    const WorkspacePanelResolvedFrame &lhs,
    const WorkspacePanelResolvedFrame &rhs
) {
  return std::fabs(lhs.x - rhs.x) < 0.5f &&
         std::fabs(lhs.y - rhs.y) < 0.5f &&
         std::fabs(lhs.width - rhs.width) < 0.5f &&
         std::fabs(lhs.height - rhs.height) < 0.5f;
}

bool nearly_equal(float lhs, float rhs, float epsilon) {
  return std::fabs(lhs - rhs) <= epsilon;
}

std::optional<glm::ivec2> cursor_to_framebuffer_pixel(
    const ui::UIRect &rect,
    glm::vec2 cursor,
    const glm::ivec2 &framebuffer_extent
) {
  if (rect.width <= 0.0f || rect.height <= 0.0f || framebuffer_extent.x <= 0 ||
      framebuffer_extent.y <= 0 || !rect.contains(cursor)) {
    return std::nullopt;
  }

  const float normalized_x = std::clamp(
      (cursor.x - rect.x) / rect.width,
      0.0f,
      std::nextafter(1.0f, 0.0f)
  );
  const float normalized_y = std::clamp(
      (cursor.y - rect.y) / rect.height,
      0.0f,
      std::nextafter(1.0f, 0.0f)
  );
  const float framebuffer_y = std::clamp(
      1.0f - normalized_y,
      0.0f,
      std::nextafter(1.0f, 0.0f)
  );

  const int pixel_x = std::clamp(
      static_cast<int>(normalized_x * static_cast<float>(framebuffer_extent.x)),
      0,
      framebuffer_extent.x - 1
  );
  const int pixel_y = std::clamp(
      static_cast<int>(
          framebuffer_y * static_cast<float>(framebuffer_extent.y)
      ),
      0,
      framebuffer_extent.y - 1
  );

  return glm::ivec2(pixel_x, pixel_y);
}

std::optional<SelectedCamera> select_camera(Scene &scene) {
  return rendering::select_main_camera(scene.world());
}

std::optional<glm::vec3> cursor_world_hit(
    const gizmo::CameraFrame &camera_frame,
    const ui::UIRect &viewport_rect,
    glm::vec2 cursor,
    const glm::vec3 &plane_point,
    const glm::vec3 &plane_normal
) {
  return gizmo::intersect_ray_plane(
      gizmo::screen_ray(camera_frame, viewport_rect, cursor),
      plane_point,
      plane_normal
  );
}

float axis_scale_component(
    const glm::vec3 &scale,
    const glm::vec3 &axis
) {
  if (axis.x != 0.0f) {
    return scale.x;
  }
  if (axis.y != 0.0f) {
    return scale.y;
  }
  return scale.z;
}

void set_axis_scale_component(
    glm::vec3 &scale,
    const glm::vec3 &axis,
    float value
) {
  if (axis.x != 0.0f) {
    scale.x = value;
    return;
  }
  if (axis.y != 0.0f) {
    scale.y = value;
    return;
  }
  scale.z = value;
}

PanelMinimumSize combine_split_minimum_sizes(
    ui::FlexDirection axis,
    const PanelMinimumSize &first,
    const PanelMinimumSize &second
) {
  constexpr float k_splitter_thickness = 6.0f;

  if (axis == ui::FlexDirection::Row) {
    return PanelMinimumSize{
        .width = first.width + second.width + k_splitter_thickness,
        .height = std::max(first.height, second.height),
    };
  }

  return PanelMinimumSize{
      .width = std::max(first.width, second.width),
      .height = first.height + second.height + k_splitter_thickness,
  };
}

bool layout_contains_panel_slot(
    const LayoutNode &node,
    std::string_view panel_instance_id
) {
  switch (node.kind) {
    case LayoutNodeKind::Split:
      return (node.first != nullptr &&
              layout_contains_panel_slot(*node.first, panel_instance_id)) ||
             (node.second != nullptr &&
              layout_contains_panel_slot(*node.second, panel_instance_id));

    case LayoutNodeKind::Tabs:
      return std::find(
                 node.tabs.begin(), node.tabs.end(), panel_instance_id
             ) != node.tabs.end();

    case LayoutNodeKind::Leaf:
    default:
      return node.panel_instance_id == panel_instance_id;
  }
}

bool layout_is_leaf(
    const LayoutNode *node,
    std::string_view panel_instance_id
) {
  return node != nullptr && node->kind == LayoutNodeKind::Leaf &&
         node->panel_instance_id == panel_instance_id;
}

const PanelInstanceSpec *find_workspace_panel_spec(
    const WorkspaceDefinition *workspace,
    std::string_view panel_instance_id
) {
  if (workspace == nullptr) {
    return nullptr;
  }

  const auto panel_it = std::find_if(
      workspace->panels.begin(),
      workspace->panels.end(),
      [panel_instance_id](const PanelInstanceSpec &panel) {
        return panel.instance_id == panel_instance_id;
      }
  );
  return panel_it != workspace->panels.end() ? &(*panel_it) : nullptr;
}

std::vector<std::string> ordered_shell_panel_ids(
    const WorkspaceDefinition *workspace,
    const std::optional<WorkspaceSnapshot> &snapshot,
    const std::vector<std::string> &panel_order
) {
  std::vector<std::string> ordered_ids;
  std::unordered_set<std::string> seen;

  if (workspace != nullptr) {
    ordered_ids.reserve(workspace->panels.size() + panel_order.size());
    seen.reserve(workspace->panels.size() + panel_order.size());

    for (const PanelInstanceSpec &panel : workspace->panels) {
      if (panel.instance_id.empty()) {
        continue;
      }

      ordered_ids.push_back(panel.instance_id);
      seen.insert(panel.instance_id);
    }
  } else {
    ordered_ids.reserve(panel_order.size());
    seen.reserve(panel_order.size());
  }

  if (!snapshot.has_value()) {
    return ordered_ids;
  }

  for (const std::string &instance_id : panel_order) {
    if (snapshot->panels.find(instance_id) == snapshot->panels.end()) {
      continue;
    }

    if (seen.insert(instance_id).second) {
      ordered_ids.push_back(instance_id);
    }
  }

  return ordered_ids;
}

LayoutNode *find_layout_node_recursive(LayoutNode *node, std::string_view path) {
  if (node == nullptr) {
    return nullptr;
  }

  if (path_is_root(path)) {
    return node;
  }

  constexpr std::string_view prefix = "root/";
  if (!path.starts_with(prefix)) {
    return nullptr;
  }

  std::string_view remaining = path.substr(prefix.size());
  LayoutNode *current = node;

  while (!remaining.empty() && current != nullptr) {
    const size_t slash = remaining.find('/');
    const std::string_view token =
        slash == std::string_view::npos ? remaining : remaining.substr(0u, slash);

    if (current->kind != LayoutNodeKind::Split) {
      return nullptr;
    }

    if (token == "0") {
      current = current->first.get();
    } else if (token == "1") {
      current = current->second.get();
    } else {
      return nullptr;
    }

    if (slash == std::string_view::npos) {
      break;
    }

    remaining = remaining.substr(slash + 1u);
  }

  return current;
}

ui::dsl::NodeSpec build_empty_workspace_state(std::string title, std::string body) {
  using namespace ui::dsl;
  using namespace ui::dsl::styles;
  const auto &theme = workspace_shell_theme();

  return ui::dsl::column()
      .style(
          fill()
              .justify_center()
              .items_center()
              .gap(8.0f)
              .padding(20.0f)
              .background(theme.panel_background)
              .radius(16.0f)
              .border(1.0f, theme.panel_border)
      )
      .children(
          text(std::move(title))
              .style(font_size(18.0f).text_color(theme.text_primary)),
          text(std::move(body))
              .style(font_size(13.0f).text_color(theme.text_muted))
      );
}

} // namespace astralix::editor::workspace_shell_detail
