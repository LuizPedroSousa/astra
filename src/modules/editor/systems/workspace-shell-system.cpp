#include "systems/workspace-shell-system.hpp"

#include "components/tags.hpp"
#include "components/ui.hpp"
#include "dsl.hpp"
#include "editor-theme.hpp"
#include "layouts/layout-registry.hpp"
#include "managers/project-manager.hpp"
#include "managers/scene-manager.hpp"
#include "managers/window-manager.hpp"
#include "panels/panel-registry.hpp"
#include "workspaces/workspace-registry.hpp"

#include <algorithm>
#include <cmath>
#include <string_view>
#include <unordered_set>

namespace astralix::editor {
namespace {

const WorkspaceShellTheme &workspace_shell_theme() {
  static const WorkspaceShellTheme theme{};
  return theme;
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
build_panel_close_button(std::string name, std::function<void()> on_click) {
  return ui::dsl::icon_button(
             "icons::close", std::move(on_click), std::move(name)
  )
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

ui::dsl::NodeSpec build_empty_workspace_state(
    std::string name, std::string title, std::string body
) {
  using namespace ui::dsl;
  using namespace ui::dsl::styles;
  const auto &theme = workspace_shell_theme();

  const std::string root_name = name;

  return column(std::move(name))
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
          text(std::move(title), root_name + "_title")
              .style(font_size(18.0f).text_color(theme.text_primary)),
          text(std::move(body), root_name + "_body")
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
  if (!m_active_snapshot.has_value()) {
    return;
  }

  for (auto &[instance_id, panel] : m_panels) {
    if (panel.controller != nullptr && panel_instance_rendered(instance_id)) {
      panel.controller->update(PanelUpdateContext{.dt = dt});
    }
  }

  apply_pending_requests();
  sync_runtime_layout_state();

  if (m_needs_rebuild && root_should_be_visible()) {
    rebuild_workspace_document();
  }

  if (m_needs_save) {
    m_save_accumulator += dt;
    if (m_save_accumulator >= 0.2) {
      save_active_workspace();
      m_save_accumulator = 0.0;
    }
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

  if (m_root_entity_id.has_value() && m_scene->world().contains(*m_root_entity_id)) {
    return;
  }

  auto root = m_scene->spawn_entity("editor_workspace_shell");
  root.emplace<scene::SceneEntity>();
  root.emplace<rendering::UIRoot>(rendering::UIRoot{
      .document = nullptr,
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
        const bool can_restore_layout =
            workspace_id != "studio" ||
            (m_active_snapshot->version < k_workspace_snapshot_version &&
             layout_is_pre_inspector_studio_root(m_active_snapshot->root));

        if (can_restore_layout) {
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
  }

  m_split_runtime_nodes.clear();
  m_floating_panel_nodes.clear();
  m_shell_bar_node = ui::k_invalid_node_id;

  auto document = ui::UIDocument::create();
  document->set_root_font_size(m_default_font_size);
  ui::dsl::mount(*document, build_shell());

  m_document = document;

  if (m_scene != nullptr && m_root_entity_id.has_value()) {
    if (auto *root = m_scene->world().get<rendering::UIRoot>(*m_root_entity_id);
        root != nullptr) {
      root->document = m_document;
      root->default_font_id = m_default_font_id;
      root->default_font_size = m_default_font_size;
      root->sort_order = 200;
    }
  }

  sync_root_visibility();

  for (auto &[instance_id, mounted] : m_panels) {
    if (mounted.controller != nullptr && panel_instance_rendered(instance_id)) {
      mounted.controller->mount(PanelMountContext{
          .document = m_document,
          .default_font_id = m_default_font_id,
          .default_font_size = m_default_font_size,
      });
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
            },
            std::string("workspace_button_") +
                (workspace != nullptr ? workspace->id : std::string("unknown"))
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
              },
              std::string("panel_toggle_") + instance_id
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
      row("editor_shell_workspace_row").style(items_center().gap(10.0f));
  for (auto &button_spec : workspace_buttons) {
    workspace_row.child(std::move(button_spec));
  }

  auto panel_row = row("editor_shell_panel_row")
                       .style(items_center().gap(8.0f).justify_end());
  for (auto &button_spec : panel_buttons) {
    panel_row.child(std::move(button_spec));
  }

  return column("editor_shell_root")
      .style(
          fill()
              .padding(14.0f)
              .gap(12.0f)
              .background(theme.backdrop)
      )
      .children(
          row("editor_shell_bar")
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
                  column("editor_shell_title_copy")
                      .style(
                          items_start()
                              .gap(2.0f)
                      )
                      .children(
                          text("Workspace Shell", "editor_shell_title")
                              .style(
                                  font_size(18.0f).text_color(theme.text_primary)
                              ),
                          text(
                              "Native editor chrome, layouts, and tools registered through plugins",
                              "editor_shell_subtitle"
                          )
                              .style(
                                  font_size(12.5f).text_color(theme.text_muted)
                              )
                      ),
                  workspace_row,
                  spacer("editor_shell_bar_spacer"),
                  panel_row
              ),
          view("editor_shell_body")
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
                            "workspace_shell_empty_state",
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
            },
            std::string("workspace_button_") +
                (workspace != nullptr ? workspace->id : std::string("unknown"))
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
                if (next_open) {
                  m_pending_panel_focus = panel_instance_id;
                }
              },
              std::string("panel_toggle_") + instance_id
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
      row("editor_shell_workspace_row").style(items_center().gap(10.0f));
  for (auto &button_spec : workspace_buttons) {
    workspace_row.child(std::move(button_spec));
  }

  auto panel_row =
      row("editor_shell_panel_row").style(items_center().gap(8.0f));
  for (auto &button_spec : panel_buttons) {
    panel_row.child(std::move(button_spec));
  }

  auto root = view("editor_shell_root").style(fill());
  root.child(
      column("editor_shell_bar")
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
              row("editor_shell_bar_heading")
                  .style(items_center().gap(12.0f))
                  .children(
                      column("editor_shell_title_copy")
                          .style(items_start().gap(2.0f))
                          .children(
                              text("Workspace Shell", "editor_shell_title")
                                  .style(
                                      font_size(18.0f).text_color(
                                          theme.text_primary
                                      )
                                  ),
                              text(
                                  "Open a tool window or switch back to the docked studio workspace.",
                                  "editor_shell_subtitle"
                              )
                                  .style(
                                      font_size(12.5f).text_color(
                                          theme.text_muted
                                      )
                                  )
                          ),
                      button(
                          "Hide",
                          [this]() { set_shell_visible(false); },
                          "editor_shell_hide"
                      )
                          .style(
                              padding_xy(12.0f, 8.0f)
                                  .radius(10.0f)
                                  .background(theme.panel_background)
                                  .border(1.0f, theme.panel_border)
                                  .text_color(theme.text_muted)
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
                                      ui::dsl::styles::state().border(
                                          2.0f, theme.accent
                                      )
                                  )
                          )
                  ),
              workspace_row,
              panel_row
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
            std::string("workspace_split_empty_") + path,
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
                           ? row(std::string("workspace_split_") + path)
                           : column(std::string("workspace_split_") + path);

      auto first = view(std::string("workspace_split_first_") + path).bind(runtime.first).style(fill_y().shrink().raw([axis = node.split_axis, ratio = node.split_ratio](ui::UIStyle &style) {
        style.min_width = ui::UILength::pixels(180.0f);
        style.min_height = ui::UILength::pixels(160.0f);
        if (axis == ui::FlexDirection::Row) {
          style.width = ui::UILength::percent(std::clamp(ratio, 0.1f, 0.9f));
          style.height = ui::UILength::percent(1.0f);
        } else {
          style.height = ui::UILength::percent(std::clamp(ratio, 0.1f, 0.9f));
          style.width = ui::UILength::percent(1.0f);
        }
      }));
      first.child(build_layout_node(*node.first, path + "/0"));

      auto second = view(std::string("workspace_split_second_") + path).bind(runtime.second).style(flex(1.0f).raw([](ui::UIStyle &style) {
        style.min_width = ui::UILength::pixels(180.0f);
        style.min_height = ui::UILength::pixels(160.0f);
      }));
      second.child(build_layout_node(*node.second, path + "/1"));

      return container.style(fill().gap(0.0f))
          .children(
              std::move(first),
              splitter(std::string("workspace_splitter_") + path)
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

      auto header_row = row(std::string("workspace_tabs_header_") + path)
                            .style(
                                fill_x()
                                    .padding(10.0f)
                                    .gap(8.0f)
                                    .items_center()
                                    .background(theme.panel_background)
                            );

      if (visible_tabs.empty()) {
        return build_empty_workspace_state(
            std::string("workspace_tabs_empty_") + path,
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
                  },
                  std::string("workspace_tab_") + path + "_" + tab_id
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

      auto content =
          view(std::string("workspace_tabs_content_") + path).style(fill_x().flex(1.0f));
      const std::string active_tab_id = resolved_active_tab(node);
      if (!active_tab_id.empty()) {
        content.child(build_leaf_panel(active_tab_id));
      } else {
        content.child(
            column("workspace_tabs_closed_placeholder")
                .style(
                    fill()
                        .justify_center()
                        .items_center()
                        .background(theme.panel_background)
                )
                .children(
                    text("No active panel", "workspace_tabs_closed_placeholder_title")
                        .style(font_size(18.0f).text_color(theme.text_primary)),
                    text(
                        "Use the panel toggles in the top bar to reopen a tool in this workspace.",
                        "workspace_tabs_closed_placeholder_body"
                    )
                        .style(font_size(13.0f).text_color(theme.text_muted))
                )
        );
      }

      return column(std::string("workspace_tabs_") + path)
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
  const auto panel_it = m_active_snapshot->panels.find(instance_id);
  const bool open =
      panel_it != m_active_snapshot->panels.end() ? panel_it->second.open : false;
  const std::string title =
      panel_it != m_active_snapshot->panels.end() && !panel_it->second.title.empty()
          ? panel_it->second.title
          : instance_id;

  auto header = row(std::string("workspace_panel_header_") + instance_id)
                    .style(
                        fill_x()
                            .padding_xy(14.0f, 12.0f)
                            .items_center()
                            .gap(12.0f)
                            .background(theme.panel_raised_background)
                            .border(0.0f, theme.panel_border)
                    )
                    .children(
                        text(title, std::string("workspace_panel_title_") + instance_id)
                            .style(
                                font_size(14.0f).text_color(theme.text_primary)
                            ),
                        spacer(std::string("workspace_panel_header_spacer_") + instance_id),
                        build_panel_close_button(
                            std::string("workspace_panel_close_") + instance_id,
                            [this, instance_id]() {
                              m_pending_panel_visibility.emplace_back(
                                  instance_id, false
                              );
                            }
                        )
                    );

  if (!open) {
    return column(std::string("workspace_panel_closed_") + instance_id)
        .style(
            fill()
                .gap(0.0f)
                .background(theme.panel_background)
                .radius(16.0f)
                .border(1.0f, theme.panel_border)
        )
        .children(
            std::move(header),
            column(std::string("workspace_panel_closed_body_") + instance_id)
                .style(fill_x().flex(1.0f).justify_center().items_center().gap(10.0f).padding(20.0f))
                .children(
                    text("Panel closed", std::string("workspace_panel_closed_title_") + instance_id)
                        .style(
                            font_size(18.0f).text_color(theme.text_primary)
                        ),
                    button(
                        "Reopen",
                        [this, instance_id]() {
                          m_pending_panel_visibility.emplace_back(
                              instance_id, true
                          );
                        },
                        std::string("workspace_panel_reopen_") + instance_id
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
    return column(std::string("workspace_panel_missing_") + instance_id)
        .style(
            fill()
                .gap(0.0f)
                .background(theme.panel_background)
                .radius(16.0f)
                .border(1.0f, theme.panel_border)
        )
        .children(
            std::move(header),
            column(std::string("workspace_panel_missing_body_") + instance_id)
                .style(fill_x().flex(1.0f).justify_center().items_center().padding(20.0f).gap(8.0f))
                .children(
                    text("Missing panel provider", std::string("workspace_panel_missing_title_") + instance_id)
                        .style(
                            font_size(18.0f).text_color(theme.text_primary)
                        ),
                    text("The panel is registered in the workspace, but its provider is not available.", std::string("workspace_panel_missing_body_copy_") + instance_id)
                        .style(font_size(13.0f).text_color(theme.text_muted))
                )
        );
  }

  return column(std::string("workspace_panel_") + instance_id)
      .style(
          fill()
              .gap(0.0f)
              .background(theme.panel_background)
              .radius(16.0f)
              .border(1.0f, theme.panel_border)
      )
      .children(
          std::move(header),
          view(std::string("workspace_panel_content_") + instance_id)
              .style(fill_x().flex(1.0f))
              .child(mounted_it->second.controller->build())
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
  const WorkspacePanelFrame frame =
      resolve_floating_panel_frame(panel_instance_id);

  auto header = row(std::string("workspace_panel_header_") + instance_id)
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
                        text(title, std::string("workspace_panel_title_") + instance_id)
                            .style(
                                font_size(14.0f).text_color(theme.text_primary)
                            ),
                        spacer(std::string("workspace_panel_header_spacer_") + instance_id),
                        build_panel_close_button(
                            std::string("workspace_panel_close_") + instance_id,
                            [this, instance_id]() {
                              m_pending_panel_visibility.emplace_back(instance_id, false);
                            }
                        )
                    );

  const auto mounted_it = m_panels.find(instance_id);
  if (mounted_it == m_panels.end() || mounted_it->second.controller == nullptr) {
    return column(std::string("workspace_panel_missing_") + instance_id)
        .bind(m_floating_panel_nodes[instance_id])
        .style(
            absolute()
                .left(px(frame.x))
                .top(px(frame.y))
                .width(px(frame.width))
                .height(px(frame.height))
                .min_width(px(320.0f))
                .min_height(px(200.0f))
                .resizable_all()
                .draggable()
                .overflow_hidden()
                .background(theme.panel_background)
                .radius(16.0f)
                .border(1.0f, theme.panel_border)
        )
        .children(
            std::move(header),
            column(std::string("workspace_panel_missing_body_") + instance_id)
                .style(fill_x().flex(1.0f).justify_center().items_center().padding(20.0f).gap(8.0f))
                .children(
                    text("Missing panel provider", std::string("workspace_panel_missing_title_") + instance_id)
                        .style(
                            font_size(18.0f).text_color(theme.text_primary)
                        ),
                    text("The panel is registered in the workspace, but its provider is not available.", std::string("workspace_panel_missing_body_copy_") + instance_id)
                        .style(font_size(13.0f).text_color(theme.text_muted))
                )
        );
  }

  return column(std::string("workspace_panel_") + instance_id)
      .bind(m_floating_panel_nodes[instance_id])
      .style(
          absolute()
              .left(px(frame.x))
              .top(px(frame.y))
              .width(px(frame.width))
              .height(px(frame.height))
              .min_width(px(320.0f))
              .min_height(px(200.0f))
              .resizable_all()
              .draggable()
              .overflow_hidden()
              .background(theme.panel_background)
              .radius(16.0f)
              .border(1.0f, theme.panel_border)
      )
      .children(
          std::move(header),
          view(std::string("workspace_panel_content_") + instance_id)
              .style(fill_x().flex(1.0f))
              .child(mounted_it->second.controller->build())
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
      it = m_panels.erase(it);
      continue;
    }

    ++it;
  }
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
  m_needs_rebuild = true;
  m_needs_save = true;
  m_save_accumulator = 0.0;
}

WorkspacePanelFrame WorkspaceShellSystem::resolve_floating_panel_frame(
    std::string_view panel_instance_id
) const {
  if (m_active_snapshot.has_value()) {
    const auto it =
        m_active_snapshot->panels.find(std::string(panel_instance_id));
    if (it != m_active_snapshot->panels.end() &&
        it->second.floating_frame.has_value() &&
        it->second.floating_frame->valid()) {
      return *it->second.floating_frame;
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
      return *panel_it->floating_frame;
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
      .width = 720.0f,
      .height = 320.0f,
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

} // namespace astralix::editor
