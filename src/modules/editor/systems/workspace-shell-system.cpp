#include "systems/workspace-shell-system.hpp"

#include "components/tags.hpp"
#include "trace.hpp"
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
#include "workspaces/workspace-registry.hpp"

#include "glm/gtx/quaternion.hpp"

#include <algorithm>
#include <cmath>
#include <string_view>
#include <unordered_set>

namespace astralix::editor {
namespace {

constexpr std::string_view k_workspace_shell_root_name =
    "editor_workspace_shell";

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

ui::dsl::NodeSpec
build_panel_close_button(std::function<void()> on_click) {
  return ui::dsl::icon_button("icons::close", std::move(on_click))
      .style(panel_close_button_style());
}

constexpr std::string_view kRootPath = "root";
bool path_is_root(std::string_view path) { return path == kRootPath; }

bool panel_frames_equal(
    const WorkspacePanelFrame &lhs,
    const WorkspacePanelFrame &rhs
) {
  return std::fabs(lhs.x - rhs.x) < 0.5f &&
         std::fabs(lhs.y - rhs.y) < 0.5f &&
         std::fabs(lhs.width - rhs.width) < 0.5f &&
         std::fabs(lhs.height - rhs.height) < 0.5f;
}

bool nearly_equal(float lhs, float rhs, float epsilon = 0.001f) {
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

using SelectedCamera = rendering::CameraSelection;

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

bool layout_is_pre_inspector_studio_root(const LayoutNode &node) {
  if (node.kind != LayoutNodeKind::Split ||
      node.split_axis != ui::FlexDirection::Row ||
      !nearly_equal(node.split_ratio, 0.22f) || node.first == nullptr ||
      node.second == nullptr || !layout_is_leaf(node.first.get(), "scene_hierarchy")) {
    return false;
  }

  const LayoutNode &viewport_split = *node.second;
  if (viewport_split.kind != LayoutNodeKind::Split ||
      viewport_split.split_axis != ui::FlexDirection::Row ||
      !nearly_equal(viewport_split.split_ratio, 0.72f) ||
      viewport_split.first == nullptr || viewport_split.second == nullptr ||
      !layout_is_leaf(viewport_split.first.get(), "viewport")) {
    return false;
  }

  const LayoutNode &runtime_console = *viewport_split.second;
  return runtime_console.kind == LayoutNodeKind::Split &&
         runtime_console.split_axis == ui::FlexDirection::Column &&
         nearly_equal(runtime_console.split_ratio, 0.40f) &&
         layout_is_leaf(runtime_console.first.get(), "runtime") &&
         layout_is_leaf(runtime_console.second.get(), "console");
}

bool migrate_studio_scene_panel_layout(LayoutNode &node) {
  switch (node.kind) {
    case LayoutNodeKind::Split:
      return (node.first != nullptr &&
              migrate_studio_scene_panel_layout(*node.first)) ||
             (node.second != nullptr &&
              migrate_studio_scene_panel_layout(*node.second));

    case LayoutNodeKind::Tabs: {
      const auto runtime_it =
          std::find(node.tabs.begin(), node.tabs.end(), "runtime");
      if (runtime_it == node.tabs.end() ||
          std::find(node.tabs.begin(), node.tabs.end(), "scene") !=
              node.tabs.end()) {
        return false;
      }

      node.tabs.insert(runtime_it, "scene");
      node.active_tab_id = "scene";
      return true;
    }

    case LayoutNodeKind::Leaf:
      if (node.panel_instance_id != "runtime") {
        return false;
      }

      node = LayoutNode::tabs_node({"scene", "runtime"}, "scene");
      return true;
  }

  return false;
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

} // namespace

void WorkspaceShellSystem::start() {
  m_store = std::make_unique<WorkspaceStore>(active_project());
  ensure_scene_root();
  load_initial_workspace();
  rebuild_workspace_document();
}

void WorkspaceShellSystem::fixed_update(double) {}

void WorkspaceShellSystem::pre_update(double) {
  if (m_config.toggle_visibility_key.has_value() &&
      input::IS_KEY_RELEASED(*m_config.toggle_visibility_key)) {
    set_shell_visible(!m_shell_visible);
  }
}

void WorkspaceShellSystem::update(double dt) {
  ASTRA_PROFILE_N("WorkspaceShellSystem::update");

  if (auto scene_manager = SceneManager::get(); scene_manager != nullptr) {
    scene_manager->flush_pending_active_scene_state();
  }

  ensure_scene_root();

  if (!m_active_snapshot.has_value()) {
    editor_gizmo_store()->clear_interaction();
    clear_gizmo_drag_state();
    return;
  }

  for (auto &[instance_id, panel] : m_panels) {
    if (panel.controller != nullptr && panel_instance_rendered(instance_id)) {
      const std::string panel_zone_name =
          panel.spec.title + "::update+render";
      ASTRA_PROFILE_DYN(panel_zone_name.c_str(), panel_zone_name.size());
      panel.controller->update(PanelUpdateContext{.dt = dt});
      render_mounted_panel(panel);
    }
  }

  {
    ASTRA_PROFILE_N("WorkspaceShellSystem::apply_pending_requests");
    apply_pending_requests();
  }
  {
    ASTRA_PROFILE_N("WorkspaceShellSystem::sync_runtime_layout_state");
    sync_runtime_layout_state();
  }

  if (m_needs_rebuild && root_should_be_visible()) {
    ASTRA_PROFILE_N("WorkspaceShellSystem::rebuild_workspace_document");
    rebuild_workspace_document();
  }

  if (m_needs_save) {
    m_save_accumulator += dt;
    if (m_save_accumulator >= 0.2) {
      save_active_workspace();
      m_save_accumulator = 0.0;
    }
  }

  {
    ASTRA_PROFILE_N("WorkspaceShellSystem::gizmo");
    sync_gizmo_capture_state();
    update_gizmo_interaction();
  }
}

bool WorkspaceShellSystem::active_workspace_uses_floating_panels() const {
  return m_active_workspace_presentation ==
         WorkspacePresentation::FloatingPanels;
}

bool WorkspaceShellSystem::root_should_be_visible() const {
  if (active_workspace_uses_floating_panels()) {
    if (m_shell_visible) {
      return true;
    }

    if (!m_active_snapshot.has_value()) {
      return false;
    }

    return std::any_of(
        m_active_snapshot->panels.begin(),
        m_active_snapshot->panels.end(),
        [](const auto &entry) { return entry.second.open; }
    );
  }

  return m_shell_visible;
}

void WorkspaceShellSystem::ensure_scene_root() {
  m_scene = SceneManager::get() != nullptr ? SceneManager::get()->get_active_scene() : nullptr;
  if (m_scene == nullptr) {
    return;
  }

  auto &scene_world = m_scene->world();
  if (auto existing_root_entity_id = find_workspace_shell_root_entity(scene_world);
      existing_root_entity_id.has_value()) {
    m_root_entity_id = *existing_root_entity_id;

    if (auto *root = scene_world.get<rendering::UIRoot>(*m_root_entity_id);
        root != nullptr) {
      configure_workspace_ui_root(
          *root, m_document, m_default_font_id, m_default_font_size
      );
    }

    return;
  }

  auto root = m_scene->spawn_entity(std::string(k_workspace_shell_root_name));
  root.emplace<rendering::UIRoot>(rendering::UIRoot{
      .document = m_document,
      .default_font_id = m_default_font_id,
      .default_font_size = m_default_font_size,
      .sort_order = 200,
      .input_enabled = true,
      .visible = true,
  });
  m_root_entity_id = root.id();
}

void WorkspaceShellSystem::load_initial_workspace() {
  const auto registered = workspace_registry()->workspaces();
  if (registered.empty()) {
    m_active_snapshot.reset();
    m_active_workspace_id.clear();
    return;
  }

  std::string workspace_id = "studio";
  if (auto stored = m_store->load_active_workspace_id(); stored.has_value()) {
    workspace_id = *stored;
  } else if (workspace_registry()->find(workspace_id) == nullptr) {
    workspace_id = registered.front()->id;
  }

  activate_workspace(std::move(workspace_id));
}

void WorkspaceShellSystem::activate_workspace(std::string workspace_id) {
  const auto *workspace = workspace_registry()->find(workspace_id);
  if (workspace == nullptr) {
    const auto registered = workspace_registry()->workspaces();
    if (registered.empty()) {
      return;
    }

    workspace = registered.front();
    workspace_id = workspace->id;
  }

  const auto *layout = layout_registry()->find(workspace->layout_id);
  if (layout == nullptr) {
    return;
  }

  if (!m_active_workspace_id.empty() && m_active_snapshot.has_value()) {
    save_active_workspace();
  }

  m_active_workspace_id = workspace_id;
  m_active_workspace_presentation = workspace->presentation;
  m_panel_order.clear();
  m_panel_order.reserve(workspace->panels.size());
  for (const auto &panel : workspace->panels) {
    m_panel_order.push_back(panel.instance_id);
  }

  if (auto stored_snapshot = m_store->load_snapshot(workspace_id);
      stored_snapshot.has_value()) {
    m_active_snapshot = std::move(*stored_snapshot);

    for (const auto &panel : workspace->panels) {
      auto panel_state = m_active_snapshot->panels.find(panel.instance_id);
      if (panel_state == m_active_snapshot->panels.end()) {
        auto state = WorkspacePanelState{
            .provider_id = panel.provider_id,
            .title = panel.title,
            .open = panel.open,
            .floating_frame = panel.floating_frame,
        };

        if (panel.seed_state) {
          auto seed_ctx = m_store->create_context();
          panel.seed_state(seed_ctx);
          state.state_blob = m_store->encode_panel_state(seed_ctx);
        }

        m_active_snapshot->panels.emplace(panel.instance_id, std::move(state));
      } else if (!panel_state->second.floating_frame.has_value() && panel.floating_frame.has_value()) {
        panel_state->second.floating_frame = panel.floating_frame;
      }
    }

    for (const auto &[instance_id, panel_state] : m_active_snapshot->panels) {
      if (std::find(m_panel_order.begin(), m_panel_order.end(), instance_id) ==
          m_panel_order.end()) {
        m_panel_order.push_back(instance_id);
      }
    }

    if (workspace->presentation == WorkspacePresentation::Docked) {
      const bool layout_is_missing_workspace_panel = std::any_of(
          workspace->panels.begin(),
          workspace->panels.end(),
          [this](const PanelInstanceSpec &panel) {
            return !layout_contains_panel_slot(
                m_active_snapshot->root, panel.instance_id
            );
          }
      );

      if (layout_is_missing_workspace_panel) {
        if (workspace_id == "studio" &&
            m_active_snapshot->version < k_workspace_snapshot_version) {
          if (layout_is_pre_inspector_studio_root(m_active_snapshot->root)) {
            m_active_snapshot->root = layout->root;
          } else if (!layout_contains_panel_slot(m_active_snapshot->root, "scene")) {
            (void)migrate_studio_scene_panel_layout(m_active_snapshot->root);
          }
        } else if (workspace_id != "studio") {
          m_active_snapshot->root = layout->root;
        }
      }
    }
  } else {
    m_active_snapshot = snapshot_from_definition(*workspace, *layout);
  }

  if (m_active_snapshot.has_value()) {
    m_active_snapshot->workspace_id = workspace_id;
  }

  mount_panels_from_snapshot();
  m_store->save_active_workspace_id(workspace_id);
  m_needs_rebuild = true;
  m_needs_save = true;
  m_save_accumulator = 0.0;
}

void WorkspaceShellSystem::rebuild_workspace_document() {
  if (!m_active_snapshot.has_value()) {
    return;
  }

  ensure_scene_root();
  destroy_unmounted_panels();

  for (auto &[instance_id, mounted] : m_panels) {
    if (mounted.controller != nullptr) {
      mounted.controller->unmount();
    }
    reset_mounted_panel_runtime(mounted);
  }

  m_split_runtime_nodes.clear();
  m_floating_panel_nodes.clear();
  m_shell_bar_node = ui::k_invalid_node_id;
  m_panel_row_node = ui::k_invalid_node_id;

  auto document = ui::UIDocument::create();
  document->set_root_font_size(m_default_font_size);
  ui::dsl::mount(*document, build_shell());

  m_document = document;

  if (m_scene != nullptr && m_root_entity_id.has_value()) {
    if (auto *root = m_scene->world().get<rendering::UIRoot>(*m_root_entity_id);
        root != nullptr) {
      configure_workspace_ui_root(
          *root, m_document, m_default_font_id, m_default_font_size
      );
    }
  }

  sync_root_visibility();

  for (auto &[instance_id, mounted] : m_panels) {
    if (mounted.controller != nullptr && panel_instance_rendered(instance_id)) {
      mount_rendered_panel(instance_id, mounted);
    }
  }

  m_needs_rebuild = false;
}

void WorkspaceShellSystem::set_shell_visible(bool visible) {
  if (m_shell_visible == visible) {
    return;
  }

  if (active_workspace_uses_floating_panels()) {
    m_shell_visible = visible;
    if (m_shell_visible && m_needs_rebuild) {
      rebuild_workspace_document();
      return;
    }
    sync_root_visibility();
    return;
  }

  if (visible) {
    m_shell_visible = true;
    resume_shell_panels();
    return;
  }

  suspend_shell_panels();
  m_shell_visible = false;
  sync_root_visibility();
}

void WorkspaceShellSystem::suspend_shell_panels() {
  if (!m_active_snapshot.has_value()) {
    return;
  }

  for (auto &[instance_id, mounted] : m_panels) {
    auto panel_state = m_active_snapshot->panels.find(instance_id);
    if (panel_state != m_active_snapshot->panels.end() &&
        mounted.controller != nullptr) {
      auto ctx = m_store->create_context();
      mounted.controller->save_state(ctx);
      panel_state->second.state_blob = m_store->encode_panel_state(ctx);
    }

    if (mounted.controller != nullptr) {
      mounted.controller->unmount();
    }
    reset_mounted_panel_runtime(mounted);
  }

  m_panels.clear();
  save_active_workspace();
}

void WorkspaceShellSystem::resume_shell_panels() {
  if (!m_active_snapshot.has_value()) {
    sync_root_visibility();
    return;
  }

  mount_panels_from_snapshot();
  rebuild_workspace_document();
  sync_root_visibility();
}

void WorkspaceShellSystem::sync_root_visibility() {
  ensure_scene_root();
  if (m_scene == nullptr || !m_root_entity_id.has_value()) {
    return;
  }

  if (auto *root = m_scene->world().get<rendering::UIRoot>(*m_root_entity_id);
      root != nullptr) {
    const bool visible = root_should_be_visible();
    root->visible = visible;
    root->input_enabled = visible;
    if (root->document != nullptr &&
        m_shell_bar_node != ui::k_invalid_node_id) {
      root->document->set_visible(
          m_shell_bar_node,
          !active_workspace_uses_floating_panels() || m_shell_visible
      );
    }
    if (visible && root->document != nullptr) {
      root->document->mark_layout_dirty();
    }
  }
}

std::optional<ui::UIRect>
WorkspaceShellSystem::visible_document_rect(ui::UINodeId node_id) const {
  if (m_document == nullptr || node_id == ui::k_invalid_node_id) {
    return std::nullopt;
  }

  const auto *node = m_document->node(node_id);
  if (node == nullptr || !node->visible || node->layout.bounds.width <= 0.0f ||
      node->layout.bounds.height <= 0.0f) {
    return std::nullopt;
  }

  return node->layout.bounds;
}

void WorkspaceShellSystem::clear_gizmo_drag_state() {
  m_gizmo_drag_state.reset();
  editor_gizmo_store()->set_active_handle(std::nullopt);
}

std::optional<EntityID> WorkspaceShellSystem::pick_entity_at_cursor(
    glm::vec2 cursor,
    const ui::UIRect &interaction_rect
) const {
  auto system_manager = SystemManager::get();
  if (system_manager == nullptr) {
    return std::nullopt;
  }

  auto *render_system = system_manager->get_system<RenderSystem>();
  if (render_system == nullptr) {
    return std::nullopt;
  }

  const auto framebuffer_extent = render_system->entity_selection_extent();
  if (!framebuffer_extent.has_value()) {
    LOG_DEBUG(
        "[WorkspaceShellSystem] viewport pick aborted: missing framebuffer extent"
    );
    return std::nullopt;
  }

  const auto pixel =
      cursor_to_framebuffer_pixel(interaction_rect, cursor, *framebuffer_extent);
  if (!pixel.has_value()) {
    LOG_DEBUG(
        "[WorkspaceShellSystem] viewport pick aborted: cursor outside interaction rect",
        "cursor=(",
        cursor.x,
        ",",
        cursor.y,
        ") rect=(",
        interaction_rect.x,
        ",",
        interaction_rect.y,
        ",",
        interaction_rect.width,
        ",",
        interaction_rect.height,
        ")"
    );
    return std::nullopt;
  }

  const auto entity_id = render_system->read_entity_id_at_pixel(pixel->x, pixel->y);
  LOG_DEBUG(
      "[WorkspaceShellSystem] viewport pick",
      "cursor=(",
      cursor.x,
      ",",
      cursor.y,
      ") rect=(",
      interaction_rect.x,
      ",",
      interaction_rect.y,
      ",",
      interaction_rect.width,
      ",",
      interaction_rect.height,
      ") framebuffer_extent=(",
      framebuffer_extent->x,
      ",",
      framebuffer_extent->y,
      ") pixel=(",
      pixel->x,
      ",",
      pixel->y,
      ") entity=",
      entity_id.has_value() ? static_cast<uint64_t>(*entity_id) : 0ull
  );
  return entity_id;
}

void WorkspaceShellSystem::sync_gizmo_capture_state() {
  auto store = editor_gizmo_store();

  if (auto window = window_manager()->active_window(); window != nullptr) {
    store->set_window_rect(ui::UIRect{
        .x = 0.0f,
        .y = 0.0f,
        .width = static_cast<float>(window->width()),
        .height = static_cast<float>(window->height()),
    });
  } else {
    store->set_window_rect(std::nullopt);
  }

  const bool floating_workspace = active_workspace_uses_floating_panels();
  store->set_window_capture_enabled(floating_workspace);

  std::vector<ui::UIRect> blocked_rects;
  if (floating_workspace) {
    if (const auto shell_bar_rect = visible_document_rect(m_shell_bar_node);
        shell_bar_rect.has_value()) {
      blocked_rects.push_back(*shell_bar_rect);
    }

    for (const auto &instance_id : m_panel_order) {
      if (instance_id == "viewport" || !panel_instance_open(instance_id)) {
        continue;
      }

      const auto node_it = m_floating_panel_nodes.find(instance_id);
      if (node_it == m_floating_panel_nodes.end()) {
        continue;
      }

      if (const auto panel_rect = visible_document_rect(node_it->second);
          panel_rect.has_value()) {
        blocked_rects.push_back(*panel_rect);
      }
    }
  }

  store->set_blocked_rects(std::move(blocked_rects));
}

void WorkspaceShellSystem::update_gizmo_interaction() {
  auto store = editor_gizmo_store();
  const auto interaction_rect = store->interaction_rect();
  if (!interaction_rect.has_value()) {
    clear_gizmo_drag_state();
    store->clear_hover_and_active_handles();
    return;
  }

  if (input::IS_CURSOR_CAPTURED()) {
    clear_gizmo_drag_state();
    store->clear_hover_and_active_handles();
    return;
  }

  const auto cursor_position = input::CURSOR_POSITION();
  const glm::vec2 cursor(
      static_cast<float>(cursor_position.x),
      static_cast<float>(cursor_position.y)
  );
  const bool interaction_hovered = store->point_in_interaction_region(cursor);

  if (!m_gizmo_drag_state.has_value() && interaction_hovered) {
    if (input::IS_KEY_RELEASED(input::KeyCode::W)) {
      store->set_mode(EditorGizmoMode::Translate);
    } else if (input::IS_KEY_RELEASED(input::KeyCode::E)) {
      store->set_mode(EditorGizmoMode::Rotate);
    } else if (input::IS_KEY_RELEASED(input::KeyCode::R)) {
      store->set_mode(EditorGizmoMode::Scale);
    }
  }

  auto scene_manager = SceneManager::get();
  Scene *scene =
      scene_manager != nullptr ? scene_manager->get_active_scene() : nullptr;
  const auto selected_entity = editor_selection_store()->selected_entity();
  scene::Transform *transform = nullptr;
  std::optional<SelectedCamera> camera_selection;
  std::optional<gizmo::CameraFrame> camera_frame;
  std::optional<glm::vec3> pivot;
  float gizmo_scale = 1.0f;
  bool selection_ready = false;

  if (scene != nullptr && selected_entity.has_value() &&
      scene->world().contains(*selected_entity)) {
    transform =
        scene->world().get<astralix::scene::Transform>(*selected_entity);
    camera_selection = select_camera(*scene);
    if (transform != nullptr && camera_selection.has_value() &&
        camera_selection->transform != nullptr &&
        camera_selection->camera != nullptr) {
      camera_frame = gizmo::make_camera_frame(
          *camera_selection->transform,
          *camera_selection->camera
      );
      pivot = transform->position;
      gizmo_scale = gizmo::gizmo_scale_world(
          *camera_frame,
          *pivot,
          interaction_rect->height
      );
      selection_ready = true;
    }
  }

  if (m_gizmo_drag_state.has_value()) {
    if (!selection_ready ||
        !input::IS_MOUSE_BUTTON_DOWN(input::MouseButton::Left) ||
        static_cast<uint64_t>(m_gizmo_drag_state->entity_id) !=
            static_cast<uint64_t>(*selected_entity)) {
      clear_gizmo_drag_state();
      return;
    }

    store->set_hovered_handle(m_gizmo_drag_state->handle);
    store->set_active_handle(m_gizmo_drag_state->handle);

    const auto current_hit = cursor_world_hit(
        *camera_frame,
        *interaction_rect,
        cursor,
        m_gizmo_drag_state->pivot,
        m_gizmo_drag_state->plane_normal
    );
    if (!current_hit.has_value()) {
      return;
    }

    switch (gizmo::mode_for_handle(m_gizmo_drag_state->handle)) {
      case EditorGizmoMode::Translate: {
        const float delta = glm::dot(
            *current_hit - m_gizmo_drag_state->start_hit_point,
            m_gizmo_drag_state->axis
        );
        transform->position = m_gizmo_drag_state->start_transform.position +
                              m_gizmo_drag_state->axis * delta;
        break;
      }

      case EditorGizmoMode::Rotate: {
        const glm::vec3 ring_vector = *current_hit - m_gizmo_drag_state->pivot;
        if (glm::length2(ring_vector) <= gizmo::k_epsilon) {
          return;
        }

        const float angle = gizmo::signed_angle_on_axis(
            m_gizmo_drag_state->start_ring_vector,
            ring_vector,
            m_gizmo_drag_state->axis
        );
        transform->rotation = glm::normalize(
            glm::angleAxis(angle, m_gizmo_drag_state->axis) *
            m_gizmo_drag_state->start_transform.rotation
        );
        break;
      }

      case EditorGizmoMode::Scale: {
        const float delta = glm::dot(
            *current_hit - m_gizmo_drag_state->start_hit_point,
            m_gizmo_drag_state->axis
        );
        const float factor = gizmo::scale_factor_from_axis_delta(
            delta,
            m_gizmo_drag_state->gizmo_scale
        );
        glm::vec3 next_scale = m_gizmo_drag_state->start_transform.scale;
        const float scaled_component = std::max(
            gizmo::k_min_scale_component,
            axis_scale_component(
                m_gizmo_drag_state->start_transform.scale,
                m_gizmo_drag_state->axis
            ) *
                factor
        );
        set_axis_scale_component(
            next_scale,
            m_gizmo_drag_state->axis,
            scaled_component
        );
        transform->scale = next_scale;
        break;
      }
    }

    transform->dirty = true;
    scene->world().touch();
    return;
  }

  std::optional<EditorGizmoHandle> hovered_handle;
  if (selection_ready && interaction_hovered) {
    hovered_handle = gizmo::pick_handle(
        gizmo::build_line_segments(store->mode(), *pivot, gizmo_scale),
        *camera_frame,
        *interaction_rect,
        cursor
    );
  }
  store->set_hovered_handle(hovered_handle);
  store->set_active_handle(std::nullopt);

  if (!interaction_hovered ||
      !input::IS_MOUSE_BUTTON_PRESSED(input::MouseButton::Left)) {
    if (!selection_ready) {
      clear_gizmo_drag_state();
    }
    return;
  }

  if (!hovered_handle.has_value()) {
    const auto picked_entity = pick_entity_at_cursor(cursor, *interaction_rect);
    LOG_DEBUG(
        "[WorkspaceShellSystem] applying viewport selection",
        picked_entity.has_value() ? static_cast<uint64_t>(*picked_entity) : 0ull
    );
    editor_selection_store()->set_selected_entity(picked_entity);
    clear_gizmo_drag_state();
    store->set_hovered_handle(std::nullopt);
    return;
  }

  if (!selection_ready) {
    clear_gizmo_drag_state();
    return;
  }

  const glm::vec3 axis = gizmo::axis_vector(*hovered_handle);
  if (glm::length2(axis) <= gizmo::k_epsilon) {
    return;
  }

  GizmoDragState drag_state{
      .handle = *hovered_handle,
      .entity_id = *selected_entity,
      .start_transform = *transform,
      .pivot = *pivot,
      .axis = axis,
      .gizmo_scale = gizmo_scale,
  };

  switch (gizmo::mode_for_handle(*hovered_handle)) {
    case EditorGizmoMode::Translate:
    case EditorGizmoMode::Scale: {
      drag_state.plane_normal =
          gizmo::translation_drag_plane_normal(*camera_frame, axis, *pivot);
      const auto start_hit = cursor_world_hit(
          *camera_frame,
          *interaction_rect,
          cursor,
          *pivot,
          drag_state.plane_normal
      );
      if (!start_hit.has_value()) {
        return;
      }

      drag_state.start_hit_point = *start_hit;
      break;
    }

    case EditorGizmoMode::Rotate: {
      drag_state.plane_normal = axis;
      const auto start_hit = cursor_world_hit(
          *camera_frame,
          *interaction_rect,
          cursor,
          *pivot,
          drag_state.plane_normal
      );
      if (!start_hit.has_value()) {
        return;
      }

      const glm::vec3 ring_vector = *start_hit - *pivot;
      if (glm::length2(ring_vector) <= gizmo::k_epsilon) {
        return;
      }

      drag_state.start_hit_point = *start_hit;
      drag_state.start_ring_vector = glm::normalize(ring_vector);
      break;
    }
  }

  m_gizmo_drag_state = drag_state;
  store->set_active_handle(*hovered_handle);
}

ui::dsl::NodeSpec WorkspaceShellSystem::build_shell() {
  using namespace ui::dsl;
  using namespace ui::dsl::styles;
  const auto &theme = workspace_shell_theme();

  if (active_workspace_uses_floating_panels()) {
    return build_floating_shell();
  }

  std::vector<ui::dsl::NodeSpec> workspace_buttons;
  for (const auto *workspace : workspace_registry()->workspaces()) {
    const bool active =
        workspace != nullptr && workspace->id == m_active_workspace_id;
    workspace_buttons.push_back(
        button(
            workspace != nullptr ? workspace->title : "Workspace",
            [this, workspace_id = workspace != nullptr ? workspace->id : std::string{}]() {
              if (!workspace_id.empty()) {
                m_requested_workspace_id = workspace_id;
              }
            }
        )
            .style(
                padding_xy(14.0f, 9.0f)
                    .radius(12.0f)
                    .background(
                        active ? theme.accent_soft : theme.panel_background
                    )
                    .border(
                        1.0f, active ? theme.accent : theme.panel_border
                    )
                    .text_color(
                        active ? theme.text_primary : theme.text_muted
                    )
                    .hover(ui::dsl::styles::state().background(theme.panel_raised_background))
                    .pressed(
                        ui::dsl::styles::state().background(theme.accent_soft)
                    )
                    .focused(
                        ui::dsl::styles::state().border(2.0f, theme.accent)
                    )
            )
    );
  }

  std::vector<ui::dsl::NodeSpec> panel_buttons;
  const auto *workspace = workspace_registry()->find(m_active_workspace_id);
  const std::vector<std::string> panel_ids =
      ordered_shell_panel_ids(workspace, m_active_snapshot, m_panel_order);
  if (m_active_snapshot.has_value()) {
    for (const auto &instance_id : panel_ids) {
      const auto it = m_active_snapshot->panels.find(instance_id);
      if (it == m_active_snapshot->panels.end()) {
        continue;
      }

      const bool open = it->second.open;
      const std::string title =
          !it->second.title.empty() ? it->second.title : instance_id;
      panel_buttons.push_back(
          button(
              title,
              [this, panel_instance_id = instance_id, next_open = !open]() {
                m_pending_panel_visibility.emplace_back(panel_instance_id, next_open);
              }
          )
              .style(
                  padding_xy(12.0f, 8.0f)
                      .radius(12.0f)
                      .background(
                          open ? theme.panel_raised_background
                               : theme.panel_background
                      )
                      .border(1.0f, open ? theme.accent : theme.panel_border)
                      .text_color(open ? theme.text_primary : theme.text_muted)
                      .hover(ui::dsl::styles::state().background(theme.panel_raised_background))
                      .pressed(
                          ui::dsl::styles::state().background(theme.accent_soft)
                      )
                      .focused(
                          ui::dsl::styles::state().border(2.0f, theme.accent)
                      )
              )
      );
    }
  }

  auto workspace_row =
      ui::dsl::row().style(items_center().gap(10.0f));
  for (auto &button_spec : workspace_buttons) {
    workspace_row.child(std::move(button_spec));
  }

  auto panel_row =
      ui::dsl::row().style(items_center().gap(8.0f).justify_end());
  for (auto &button_spec : panel_buttons) {
    panel_row.child(std::move(button_spec));
  }

  return ui::dsl::column()
      .style(
          fill()
              .padding(14.0f)
              .gap(12.0f)
              .background(theme.backdrop)
      )
      .children(
          ui::dsl::row()
              .bind(m_shell_bar_node)
              .style(
                  fill_x()
                      .padding(14.0f)
                      .gap(16.0f)
                      .radius(18.0f)
                      .background(theme.bar_background)
                      .border(1.0f, theme.panel_border)
                      .items_center()
              )
              .children(
                  ui::dsl::column()
                      .style(
                          items_start()
                              .gap(2.0f)
                      )
                      .children(
                          text("Workspace Shell")
                              .style(
                                  font_size(18.0f).text_color(theme.text_primary)
                              ),
                          text("Native editor chrome, layouts, and tools registered through plugins")
                              .style(
                                  font_size(12.5f).text_color(theme.text_muted)
                              )
                      ),
                  workspace_row,
                  spacer(),
                  ui::dsl::row()
                      .style(items_center().gap(10.0f).shrink(0.0f))
                      .child(std::move(panel_row))
              ),
          ui::dsl::view()
              .style(
                  fill_x()
                      .flex(1.0f)
                      .padding(4.0f)
                      .radius(20.0f)
                      .background(theme.bar_background)
                      .border(1.0f, theme.panel_border)
              )
              .child(
                  layout_node_visible(m_active_snapshot->root)
                      ? build_layout_node(m_active_snapshot->root, std::string(kRootPath))
                      : build_empty_workspace_state(
                            "No panels open",
                            "Use the panel toggles in the top bar to reopen a tool in this workspace."
                        )
              )
      );
}

ui::dsl::NodeSpec WorkspaceShellSystem::build_floating_shell() {
  using namespace ui::dsl;
  using namespace ui::dsl::styles;
  const auto &theme = workspace_shell_theme();

  std::vector<ui::dsl::NodeSpec> workspace_buttons;
  for (const auto *workspace : workspace_registry()->workspaces()) {
    const bool active =
        workspace != nullptr && workspace->id == m_active_workspace_id;
    workspace_buttons.push_back(
        button(
            workspace != nullptr ? workspace->title : "Workspace",
            [this, workspace_id = workspace != nullptr ? workspace->id : std::string{}]() {
              if (!workspace_id.empty()) {
                m_requested_workspace_id = workspace_id;
              }
            }
        )
            .style(
                padding_xy(14.0f, 9.0f)
                    .radius(12.0f)
                    .background(
                        active ? theme.accent_soft : theme.panel_background
                    )
                    .border(1.0f, active ? theme.accent : theme.panel_border)
                    .text_color(active ? theme.text_primary : theme.text_muted)
                    .hover(ui::dsl::styles::state().background(theme.panel_raised_background))
                    .pressed(
                        ui::dsl::styles::state().background(theme.accent_soft)
                    )
                    .focused(
                        ui::dsl::styles::state().border(2.0f, theme.accent)
                    )
            )
    );
  }

  auto panel_buttons = build_floating_panel_buttons();

  auto workspace_row =
      ui::dsl::row().style(items_center().gap(10.0f));
  for (auto &button_spec : workspace_buttons) {
    workspace_row.child(std::move(button_spec));
  }

  auto panel_row = ui::dsl::row().bind(m_panel_row_node).style(items_center().gap(8.0f));
  for (auto &button_spec : panel_buttons) {
    panel_row.child(std::move(button_spec));
  }

  auto root = ui::dsl::view().style(fill());
  root.child(
      ui::dsl::column()
          .bind(m_shell_bar_node)
          .visible(m_shell_visible)
          .style(
              absolute()
                  .left(px(14.0f))
                  .top(px(14.0f))
                  .padding(14.0f)
                  .gap(12.0f)
                  .radius(18.0f)
                  .background(theme.bar_background)
                  .border(1.0f, theme.panel_border)
          )
          .children(
              ui::dsl::row()
                  .style(items_center().gap(12.0f))
                  .children(
                      ui::dsl::column()
                          .style(items_start().gap(2.0f))
                          .children(
                              text("Workspace Shell")
                                  .style(
                                      font_size(18.0f).text_color(
                                          theme.text_primary
                                      )
                                  ),
                              text("Open a tool window or switch back to the docked studio workspace.")
                                  .style(
                                      font_size(12.5f).text_color(
                                          theme.text_muted
                                      )
                                  )
                          ),
                      spacer(),
                      ui::dsl::row()
                          .style(items_center().gap(8.0f).shrink(0.0f))
                          .children(
                              build_panel_close_button(
                                  [this]() { set_shell_visible(false); }
                              )
                          )
                  ),
              workspace_row,
              std::move(panel_row)
          )
  );

  if (m_active_snapshot.has_value()) {
    for (const auto &instance_id : m_panel_order) {
      if (panel_instance_open(instance_id)) {
        root.child(build_floating_panel(instance_id));
      }
    }
  }

  return root;
}

ui::dsl::NodeSpec WorkspaceShellSystem::build_layout_node(
    const LayoutNode &node, const std::string &path
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
            "Use the panel toggles in the top bar to reopen a tool in this workspace."
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
      const PanelMinimumSize first_minimum =
          node.first != nullptr ? layout_node_minimum_size(*node.first)
                                : PanelMinimumSize{};
      const PanelMinimumSize second_minimum =
          node.second != nullptr ? layout_node_minimum_size(*node.second)
                                 : PanelMinimumSize{};

      auto first_style = fill_y()
                             .shrink()
                             .min_width(ui::UILength::pixels(first_minimum.width))
                             .min_height(ui::UILength::pixels(first_minimum.height));
      if (node.split_axis == ui::FlexDirection::Row) {
        first_style.width(
            ui::UILength::percent(std::clamp(node.split_ratio, 0.1f, 0.9f))
        ).height(ui::UILength::percent(1.0f));
      } else {
        first_style.height(
            ui::UILength::percent(std::clamp(node.split_ratio, 0.1f, 0.9f))
        ).width(ui::UILength::percent(1.0f));
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

      return container.style(fill().gap(0.0f))
          .children(
              std::move(first),
              splitter()
                  .style(background(theme.accent_soft)),
              std::move(second)
          );
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

      auto header_row = ui::dsl::row()
                            .style(
                                fill_x()
                                    .padding(10.0f)
                                    .gap(8.0f)
                                    .items_center()
                                    .background(theme.panel_background)
                            );

      if (visible_tabs.empty()) {
        return build_empty_workspace_state(
            "No panels open",
            "Use the panel toggles in the top bar to reopen a tool in this workspace."
        );
      } else {
        const std::string active_tab_id = resolved_active_tab(node);
        for (const auto &tab_id : visible_tabs) {
          const auto panel_it = m_active_snapshot->panels.find(tab_id);
          const std::string title =
              panel_it != m_active_snapshot->panels.end() && !panel_it->second.title.empty()
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
                          .hover(ui::dsl::styles::state().background(theme.panel_raised_background))
                          .pressed(ui::dsl::styles::state().background(theme.accent_soft))
                          .focused(ui::dsl::styles::state().border(2.0f, theme.accent))
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
                    text("Use the panel toggles in the top bar to reopen a tool in this workspace.")
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

  auto header = ui::dsl::row()
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
                            .style(
                                font_size(14.0f).text_color(theme.text_primary)
                            ),
                        spacer(),
                        build_panel_close_button(
                            [this, instance_id]() {
                              m_pending_panel_visibility.emplace_back(
                                  instance_id, false
                              );
                            }
                        )
                    );

  if (!open) {
    return ui::dsl::column()
        .style(
            fill()
                .gap(0.0f)
                .min_width(px(minimum_size.width))
                .min_height(px(minimum_size.height))
                .background(theme.panel_background)
                .radius(16.0f)
                .border(1.0f, theme.panel_border)
        )
        .children(
            std::move(header),
            ui::dsl::column()
                .style(fill_x().flex(1.0f).justify_center().items_center().gap(10.0f).padding(20.0f))
                .children(
                    text("Panel closed")
                        .style(
                            font_size(18.0f).text_color(theme.text_primary)
                        ),
                    button(
                        "Reopen",
                        [this, instance_id]() {
                          m_pending_panel_visibility.emplace_back(
                              instance_id, true
                          );
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
  }

  const auto mounted_it = m_panels.find(instance_id);
  if (mounted_it == m_panels.end() || mounted_it->second.controller == nullptr) {
    return ui::dsl::column()
        .style(
            fill()
                .gap(0.0f)
                .min_width(px(minimum_size.width))
                .min_height(px(minimum_size.height))
                .background(theme.panel_background)
                .radius(16.0f)
                .border(1.0f, theme.panel_border)
        )
        .children(
            std::move(header),
            ui::dsl::column()
                .style(fill_x().flex(1.0f).justify_center().items_center().padding(20.0f).gap(8.0f))
                .children(
                    text("Missing panel provider")
                        .style(
                            font_size(18.0f).text_color(theme.text_primary)
                        ),
                    text("The panel is registered in the workspace, but its provider is not available.")
                        .style(font_size(13.0f).text_color(theme.text_muted))
                )
        );
  }

  return ui::dsl::column()
      .style(
          fill()
              .gap(0.0f)
              .min_width(px(minimum_size.width))
              .min_height(px(minimum_size.height))
              .background(theme.panel_background)
              .radius(16.0f)
              .border(1.0f, theme.panel_border)
      )
      .children(
          std::move(header),
          [&]() {
            return ui::dsl::view()
                .bind(mounted_it->second.content_host_node)
                .style(fill_x().flex(1.0f).min_height(px(0.0f)).overflow_hidden());
          }()
      );
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
  const WorkspacePanelFrame frame =
      resolve_floating_panel_frame(panel_instance_id);

  auto header = ui::dsl::row()
                    .style(
                        fill_x()
                            .padding_xy(14.0f, 12.0f)
                            .items_center()
                            .gap(12.0f)
                            .background(theme.panel_raised_background)
                            .border(0.0f, theme.panel_border)
                            .drag_handle()
                    )
                    .children(
                        text(title)
                            .style(
                                font_size(14.0f).text_color(theme.text_primary)
                            ),
                        spacer(),
                        build_panel_close_button(
                            [this, instance_id]() {
                              m_pending_panel_visibility.emplace_back(instance_id, false);
                            }
                        )
                    );

  const auto mounted_it = m_panels.find(instance_id);
  if (mounted_it == m_panels.end() || mounted_it->second.controller == nullptr) {
    return ui::dsl::column()
        .bind(m_floating_panel_nodes[instance_id])
        .style(
            absolute()
                .left(px(frame.x))
                .top(px(frame.y))
                .width(px(frame.width))
                .height(px(frame.height))
                .min_width(px(minimum_size.width))
                .min_height(px(minimum_size.height))
                .resizable_all()
                .draggable()
                .overflow_hidden()
                .background(theme.panel_background)
                .radius(16.0f)
                .border(1.0f, theme.panel_border)
        )
        .children(
            std::move(header),
            ui::dsl::column()
                .style(fill_x().flex(1.0f).justify_center().items_center().padding(20.0f).gap(8.0f))
                .children(
                    text("Missing panel provider")
                        .style(
                            font_size(18.0f).text_color(theme.text_primary)
                        ),
                    text("The panel is registered in the workspace, but its provider is not available.")
                        .style(font_size(13.0f).text_color(theme.text_muted))
                )
        );
  }

  return ui::dsl::column()
      .bind(m_floating_panel_nodes[instance_id])
      .style(
          absolute()
              .left(px(frame.x))
              .top(px(frame.y))
              .width(px(frame.width))
              .height(px(frame.height))
              .min_width(px(minimum_size.width))
              .min_height(px(minimum_size.height))
              .resizable_all()
              .draggable()
              .overflow_hidden()
              .background(theme.panel_background)
              .radius(16.0f)
              .border(1.0f, theme.panel_border)
      )
      .children(
          std::move(header),
          [&]() {
            return ui::dsl::view()
                .bind(mounted_it->second.content_host_node)
                .style(fill_x().flex(1.0f).min_height(px(0.0f)).overflow_hidden());
          }()
      );
}

void WorkspaceShellSystem::mount_panels_from_snapshot() {
  if (!m_active_snapshot.has_value()) {
    return;
  }

  const auto panel_ids = m_panel_order;
  for (const auto &instance_id : panel_ids) {
    const auto panel_it = m_active_snapshot->panels.find(instance_id);
    if (panel_it == m_active_snapshot->panels.end() || !panel_it->second.open) {
      continue;
    }

    if (m_panels.contains(instance_id)) {
      auto &mounted = m_panels.at(instance_id);
      mounted.spec.open = true;
      mounted.spec.provider_id = panel_it->second.provider_id;
      mounted.spec.title = panel_it->second.title;
      continue;
    }

    const auto *provider = panel_registry()->find(panel_it->second.provider_id);
    if (provider == nullptr || !provider->factory) {
      continue;
    }

    auto controller = provider->factory();
    if (controller == nullptr) {
      continue;
    }

    if (auto panel_state = m_store->decode_panel_state(panel_it->second.state_blob);
        panel_state.has_value()) {
      controller->load_state(*panel_state);
    }

    m_panels.emplace(
        instance_id,
        MountedPanel{
            .spec =
                PanelInstanceSpec{
                    .instance_id = instance_id,
                    .provider_id = panel_it->second.provider_id,
                    .title = panel_it->second.title,
                    .open = panel_it->second.open,
                },
            .controller = std::move(controller),
        }
    );
  }

  destroy_unmounted_panels();
}

void WorkspaceShellSystem::destroy_unmounted_panels() {
  if (!m_active_snapshot.has_value()) {
    for (auto &[instance_id, mounted] : m_panels) {
      if (mounted.controller != nullptr) {
        mounted.controller->unmount();
      }
      reset_mounted_panel_runtime(mounted);
    }
    m_panels.clear();
    return;
  }

  std::unordered_set<std::string> keep;
  for (const auto &[instance_id, panel] : m_active_snapshot->panels) {
    if (panel.open) {
      keep.insert(instance_id);
    }
  }

  for (auto it = m_panels.begin(); it != m_panels.end();) {
    if (!keep.contains(it->first)) {
      if (it->second.controller != nullptr) {
        auto state = m_active_snapshot->panels.find(it->first);
        if (state != m_active_snapshot->panels.end()) {
          auto ctx = m_store->create_context();
          it->second.controller->save_state(ctx);
          state->second.state_blob = m_store->encode_panel_state(ctx);
        }

        it->second.controller->unmount();
      }
      reset_mounted_panel_runtime(it->second);
      it = m_panels.erase(it);
      continue;
    }

    ++it;
  }
}

void WorkspaceShellSystem::reset_mounted_panel_runtime(MountedPanel &mounted) {
  mounted.runtime.reset();
  mounted.content_host_node = ui::k_invalid_node_id;
  mounted.last_render_version.reset();
}

void WorkspaceShellSystem::mount_rendered_panel(
    std::string_view,
    MountedPanel &mounted
) {
  if (mounted.controller == nullptr || m_document == nullptr ||
      mounted.content_host_node == ui::k_invalid_node_id) {
    return;
  }

  mounted.runtime =
      create_scope<ui::im::Runtime>(m_document, mounted.content_host_node);

  mounted.controller->mount(PanelMountContext{
      .runtime = mounted.runtime.get(),
      .default_font_id = m_default_font_id,
      .default_font_size = m_default_font_size,
  });
  render_mounted_panel(mounted);
}

void WorkspaceShellSystem::render_mounted_panel(MountedPanel &mounted) {
  const std::string render_zone_name =
      mounted.spec.title + "::render_mounted_panel";
  ASTRA_PROFILE_DYN(render_zone_name.c_str(), render_zone_name.size());

  if (mounted.controller == nullptr || mounted.runtime == nullptr) {
    return;
  }

  const auto render_version = mounted.controller->render_version();
  if (render_version.has_value() &&
      mounted.last_render_version.has_value() &&
      *mounted.last_render_version == *render_version) {
    return;
  }

  mounted.runtime->render(
#ifdef ASTRA_TRACE
      [&controller = mounted.controller,
       &title = mounted.spec.title](ui::im::Frame &ui) {
        const std::string zone_name = title + "::render";
        ASTRA_PROFILE_DYN(zone_name.c_str(), zone_name.size());
        controller->render(ui);
      }
#else
      [&controller = mounted.controller](ui::im::Frame &ui) {
        controller->render(ui);
      }
#endif
  );

  mounted.last_render_version = render_version;
}

void WorkspaceShellSystem::apply_pending_requests() {
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

      const WorkspacePanelFrame next_frame{
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

    const float first_size =
        layout_node->split_axis == ui::FlexDirection::Row
            ? first_node->layout.measured_size.x
            : first_node->layout.measured_size.y;
    const float second_size =
        layout_node->split_axis == ui::FlexDirection::Row
            ? second_node->layout.measured_size.x
            : second_node->layout.measured_size.y;
    const float total = first_size + second_size;
    if (total <= 1.0f) {
      continue;
    }

    const float next_ratio = std::clamp(first_size / total, 0.1f, 0.9f);
    if (std::fabs(layout_node->split_ratio - next_ratio) > 0.001f) {
      layout_node->split_ratio = next_ratio;
      changed = true;
    }
  }

  if (changed) {
    m_needs_save = true;
    m_save_accumulator = 0.0;
  }
}

void WorkspaceShellSystem::save_active_workspace() {
  if (!m_active_snapshot.has_value()) {
    return;
  }

  auto snapshot = make_snapshot();
  m_store->save_active_workspace_id(snapshot.workspace_id);
  m_store->save_snapshot(snapshot);
  m_active_snapshot = snapshot;
  m_needs_save = false;
}

WorkspaceSnapshot WorkspaceShellSystem::make_snapshot() const {
  WorkspaceSnapshot snapshot =
      m_active_snapshot.has_value() ? *m_active_snapshot : WorkspaceSnapshot{};
  snapshot.version = k_workspace_snapshot_version;
  snapshot.workspace_id = m_active_workspace_id;

  for (const auto &[instance_id, mounted] : m_panels) {
    auto state_it = snapshot.panels.find(instance_id);
    if (state_it == snapshot.panels.end()) {
      state_it = snapshot.panels
                     .emplace(
                         instance_id,
                         WorkspacePanelState{
                             .provider_id = mounted.spec.provider_id,
                             .title = mounted.spec.title,
                             .open = mounted.spec.open,
                             .floating_frame =
                                 active_workspace_uses_floating_panels()
                                     ? std::optional<WorkspacePanelFrame>(
                                           resolve_floating_panel_frame(instance_id)
                                       )
                                     : std::nullopt,
                         }
                     )
                     .first;
    }

    state_it->second.provider_id = mounted.spec.provider_id;
    state_it->second.title = mounted.spec.title;
    state_it->second.open = mounted.spec.open;
    if (active_workspace_uses_floating_panels() &&
        !state_it->second.floating_frame.has_value()) {
      state_it->second.floating_frame =
          resolve_floating_panel_frame(instance_id);
    }

    if (mounted.controller != nullptr) {
      auto ctx = m_store->create_context();
      mounted.controller->save_state(ctx);
      state_it->second.state_blob = m_store->encode_panel_state(ctx);
    }
  }

  return snapshot;
}

WorkspaceSnapshot WorkspaceShellSystem::snapshot_from_definition(
    const WorkspaceDefinition &workspace, const LayoutTemplate &layout
) const {
  WorkspaceSnapshot snapshot;
  snapshot.version = k_workspace_snapshot_version;
  snapshot.workspace_id = workspace.id;
  snapshot.root = layout.root;

  for (const auto &panel : workspace.panels) {
    WorkspacePanelState state{
        .provider_id = panel.provider_id,
        .title = panel.title,
        .open = panel.open,
        .floating_frame = panel.floating_frame,
    };

    if (panel.seed_state && m_store != nullptr) {
      auto ctx = m_store->create_context();
      panel.seed_state(ctx);
      state.state_blob = m_store->encode_panel_state(ctx);
    }

    snapshot.panels.emplace(panel.instance_id, std::move(state));
  }

  return snapshot;
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
    const auto panel_it = std::find_if(
        workspace->panels.begin(),
        workspace->panels.end(),
        [panel_instance_id](const PanelInstanceSpec &panel) {
          return panel.instance_id == panel_instance_id;
        }
    );
    if (panel_it != workspace->panels.end()) {
      if (const auto minimum =
              minimum_size_for_provider(panel_it->provider_id);
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
    m_needs_save = true;
    m_save_accumulator = 0.0;
    return;
  }

  m_needs_rebuild = true;
  m_needs_save = true;
  m_save_accumulator = 0.0;
}

WorkspacePanelFrame WorkspaceShellSystem::resolve_floating_panel_frame(
    std::string_view panel_instance_id
) const {
  const PanelMinimumSize minimum_size = panel_minimum_size(panel_instance_id);

  if (m_active_snapshot.has_value()) {
    const auto it =
        m_active_snapshot->panels.find(std::string(panel_instance_id));
    if (it != m_active_snapshot->panels.end() &&
        it->second.floating_frame.has_value() &&
        it->second.floating_frame->valid()) {
      WorkspacePanelFrame frame = *it->second.floating_frame;
      frame.width = std::max(frame.width, minimum_size.width);
      frame.height = std::max(frame.height, minimum_size.height);
      return frame;
    }
  }

  if (const auto *workspace = workspace_registry()->find(m_active_workspace_id);
      workspace != nullptr) {
    const auto panel_it = std::find_if(
        workspace->panels.begin(),
        workspace->panels.end(),
        [panel_instance_id](const PanelInstanceSpec &panel) {
          return panel.instance_id == panel_instance_id;
        }
    );
    if (panel_it != workspace->panels.end() &&
        panel_it->floating_frame.has_value() &&
        panel_it->floating_frame->valid()) {
      WorkspacePanelFrame frame = *panel_it->floating_frame;
      frame.width = std::max(frame.width, minimum_size.width);
      frame.height = std::max(frame.height, minimum_size.height);
      return frame;
    }
  }

  const auto order_it = std::find(
      m_panel_order.begin(), m_panel_order.end(), panel_instance_id
  );
  const size_t panel_index = order_it != m_panel_order.end()
                                 ? static_cast<size_t>(
                                       std::distance(m_panel_order.begin(), order_it)
                                   )
                                 : 0u;
  return WorkspacePanelFrame{
      .x = 48.0f + static_cast<float>(panel_index) * 36.0f,
      .y = 112.0f + static_cast<float>(panel_index) * 28.0f,
      .width = std::max(720.0f, minimum_size.width),
      .height = std::max(320.0f, minimum_size.height),
  };
}

void WorkspaceShellSystem::set_panel_open(std::string_view panel_instance_id, bool open) {
  if (!m_active_snapshot.has_value()) {
    return;
  }

  auto it = m_active_snapshot->panels.find(std::string(panel_instance_id));
  if (it == m_active_snapshot->panels.end() || it->second.open == open) {
    return;
  }

  it->second.open = open;
  if (open && active_workspace_uses_floating_panels() &&
      !it->second.floating_frame.has_value()) {
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

    rebuild_panel_toggle_buttons();
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
  m_needs_rebuild = true;
  m_needs_save = true;
  m_save_accumulator = 0.0;
  sync_root_visibility();
}

std::vector<ui::dsl::NodeSpec>
WorkspaceShellSystem::build_floating_panel_buttons() {
  using namespace ui::dsl::styles;
  const auto &theme = workspace_shell_theme();

  std::vector<ui::dsl::NodeSpec> buttons;
  const auto *workspace = workspace_registry()->find(m_active_workspace_id);
  const std::vector<std::string> panel_ids =
      ordered_shell_panel_ids(workspace, m_active_snapshot, m_panel_order);

  if (!m_active_snapshot.has_value()) {
    return buttons;
  }

  for (const auto &instance_id : panel_ids) {
    const auto it = m_active_snapshot->panels.find(instance_id);
    if (it == m_active_snapshot->panels.end()) {
      continue;
    }

    const bool open = it->second.open;
    const std::string title =
        !it->second.title.empty() ? it->second.title : instance_id;
    buttons.push_back(
        ui::dsl::button(
            title,
            [this, panel_instance_id = instance_id, next_open = !open]() {
              m_pending_panel_visibility.emplace_back(
                  panel_instance_id, next_open
              );
              if (next_open) {
                m_pending_panel_focus = panel_instance_id;
              }
            }
        )
            .style(
                padding_xy(12.0f, 8.0f)
                    .radius(12.0f)
                    .background(
                        open ? theme.panel_raised_background
                             : theme.panel_background
                    )
                    .border(1.0f, open ? theme.accent : theme.panel_border)
                    .text_color(open ? theme.text_primary : theme.text_muted)
                    .hover(ui::dsl::styles::state().background(theme.panel_raised_background))
                    .pressed(
                        ui::dsl::styles::state().background(theme.accent_soft)
                    )
                    .focused(
                        ui::dsl::styles::state().border(2.0f, theme.accent)
                    )
            )
    );
  }

  return buttons;
}

void WorkspaceShellSystem::rebuild_panel_toggle_buttons() {
  if (m_document == nullptr || m_panel_row_node == ui::k_invalid_node_id) {
    return;
  }

  m_document->clear_children(m_panel_row_node);

  for (auto &button_spec : build_floating_panel_buttons()) {
    ui::dsl::append(*m_document, m_panel_row_node, std::move(button_spec));
  }
}

} // namespace astralix::editor
