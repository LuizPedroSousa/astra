#include "tools/viewport/viewport-panel-controller.hpp"

#include "dsl/widgets/composites/button.hpp"
#include "dsl/widgets/layout/column.hpp"
#include "dsl/widgets/layout/scroll-view.hpp"
#include "editor-viewport-navigation-store.hpp"
#include "editor-theme.hpp"
#include "fnv1a.hpp"
#include "managers/scene-manager.hpp"
#include "systems/render-system/scene-selection.hpp"
#include "tools/viewport/gizmo-math.hpp"
#include "trace.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace astralix::editor {

using namespace ui::dsl;
using namespace ui::dsl::styles;

namespace {

constexpr float k_navigation_gizmo_size = 112.0f;
constexpr float k_navigation_gizmo_top_offset = 14.0f;
constexpr float k_navigation_gizmo_right_offset = 14.0f;
constexpr float k_navigation_marker_radius = 34.0f;
constexpr float k_navigation_primary_marker_size = 30.0f;
constexpr float k_navigation_secondary_marker_size = 22.0f;
constexpr float k_navigation_projection_button_size = 24.0f;
constexpr float k_navigation_active_alignment_threshold = 0.985f;

size_t navigation_preset_index(EditorCameraNavigationPreset preset) {
  return preset == EditorCameraNavigationPreset::Orbit ? 1u : 0u;
}

EditorCameraNavigationPreset navigation_preset_from_index(size_t index) {
  return index == 1u ? EditorCameraNavigationPreset::Orbit
                     : EditorCameraNavigationPreset::Free;
}

glm::vec4 blend_color(
    const glm::vec4 &lhs,
    const glm::vec4 &rhs,
    float factor
) {
  return lhs * (1.0f - factor) + rhs * factor;
}

glm::vec3 normalized_or_fallback(glm::vec3 value, glm::vec3 fallback) {
  const float length_squared = glm::dot(value, value);
  if (length_squared <= gizmo::k_epsilon) {
    return fallback;
  }

  return glm::normalize(value);
}

struct NavigationMarkerConfig {
  std::string_view id;
  std::string_view label;
  glm::vec3 axis = glm::vec3(0.0f);
  EditorViewportNavigationAction action =
      EditorViewportNavigationAction::Front;
  glm::vec4 color = glm::vec4(1.0f);
  bool prominent = false;
};

struct NavigationMarkerInstance {
  NavigationMarkerConfig config;
  glm::vec3 view_axis = glm::vec3(0.0f);
  bool active = false;
};

std::array<NavigationMarkerConfig, 6u> navigation_marker_configs() {
  return {{
      NavigationMarkerConfig{
          .id = "x_pos",
          .label = "X",
          .axis = glm::vec3(1.0f, 0.0f, 0.0f),
          .action = EditorViewportNavigationAction::Right,
          .color = glm::vec4(0.90f, 0.25f, 0.25f, 1.0f),
          .prominent = true,
      },
      NavigationMarkerConfig{
          .id = "x_neg",
          .label = "-X",
          .axis = glm::vec3(-1.0f, 0.0f, 0.0f),
          .action = EditorViewportNavigationAction::Left,
          .color = glm::vec4(0.90f, 0.25f, 0.25f, 1.0f),
          .prominent = false,
      },
      NavigationMarkerConfig{
          .id = "y_pos",
          .label = "Y",
          .axis = glm::vec3(0.0f, 1.0f, 0.0f),
          .action = EditorViewportNavigationAction::Top,
          .color = glm::vec4(0.40f, 0.84f, 0.32f, 1.0f),
          .prominent = true,
      },
      NavigationMarkerConfig{
          .id = "y_neg",
          .label = "-Y",
          .axis = glm::vec3(0.0f, -1.0f, 0.0f),
          .action = EditorViewportNavigationAction::Bottom,
          .color = glm::vec4(0.40f, 0.84f, 0.32f, 1.0f),
          .prominent = false,
      },
      NavigationMarkerConfig{
          .id = "z_pos",
          .label = "Z",
          .axis = glm::vec3(0.0f, 0.0f, 1.0f),
          .action = EditorViewportNavigationAction::Front,
          .color = glm::vec4(0.30f, 0.50f, 0.95f, 1.0f),
          .prominent = true,
      },
      NavigationMarkerConfig{
          .id = "z_neg",
          .label = "-Z",
          .axis = glm::vec3(0.0f, 0.0f, -1.0f),
          .action = EditorViewportNavigationAction::Back,
          .color = glm::vec4(0.30f, 0.50f, 0.95f, 1.0f),
          .prominent = false,
      },
  }};
}

glm::vec3 navigation_action_forward(EditorViewportNavigationAction action) {
  switch (action) {
    case EditorViewportNavigationAction::Back:
      return glm::vec3(0.0f, 0.0f, 1.0f);
    case EditorViewportNavigationAction::Right:
      return glm::vec3(-1.0f, 0.0f, 0.0f);
    case EditorViewportNavigationAction::Left:
      return glm::vec3(1.0f, 0.0f, 0.0f);
    case EditorViewportNavigationAction::Top:
      return glm::vec3(0.0f, -1.0f, 0.0f);
    case EditorViewportNavigationAction::Bottom:
      return glm::vec3(0.0f, 1.0f, 0.0f);
    case EditorViewportNavigationAction::ToggleProjection:
    case EditorViewportNavigationAction::Front:
    default:
      return glm::vec3(0.0f, 0.0f, -1.0f);
  }
}

std::optional<rendering::CameraSelection> active_viewport_camera_selection() {
  auto scene_manager = SceneManager::get();
  Scene *scene =
      scene_manager != nullptr ? scene_manager->get_active_scene() : nullptr;
  if (scene == nullptr) {
    return std::nullopt;
  }

  return rendering::select_main_camera(scene->world());
}

std::vector<NavigationMarkerInstance> navigation_marker_instances(
    const rendering::CameraSelection &selection
) {
  const glm::mat3 view_rotation(selection.camera->view_matrix);
  const glm::vec3 forward = normalized_or_fallback(
      selection.camera->front,
      glm::vec3(0.0f, 0.0f, -1.0f)
  );

  std::vector<NavigationMarkerInstance> instances;
  instances.reserve(6u);

  for (const auto &config : navigation_marker_configs()) {
    const glm::vec3 view_axis = normalized_or_fallback(
        view_rotation * config.axis,
        config.axis
    );
    instances.push_back(NavigationMarkerInstance{
        .config = config,
        .view_axis = view_axis,
        .active =
            glm::dot(forward, navigation_action_forward(config.action)) >=
            k_navigation_active_alignment_threshold,
    });
  }

  std::sort(
      instances.begin(),
      instances.end(),
      [](const NavigationMarkerInstance &lhs, const NavigationMarkerInstance &rhs) {
        return lhs.view_axis.z < rhs.view_axis.z;
      }
  );

  return instances;
}

void draw_attachment_preview(
    ui::im::Children &parent,
    std::string_view local_name,
    const std::string &label,
    RenderImageExportKey render_image_key,
    float preview_exposure,
    std::function<void()> on_click,
    const ViewportPanelTheme &theme,
    const WorkspaceShellTheme &shell_theme
) {
  auto preview = parent.pressable(local_name)
                     .on_click(std::move(on_click))
                     .style(
                         ui::dsl::styles::column()
                             .width(px(160.0f))
                             .gap(6.0f)
                             .shrink(0.0f)
                             .padding(8.0f)
                             .background(shell_theme.panel_background)
                             .border(1.0f, shell_theme.panel_border)
                             .radius(14.0f)
                             .cursor_pointer()
                             .hover(
                                 state()
                                     .background(shell_theme.accent_soft)
                                     .border(1.0f, shell_theme.accent)
                             )
                             .pressed(
                                 state()
                                     .background(
                                         shell_theme.panel_raised_background
                                     )
                                     .border(1.0f, shell_theme.accent)
                             )
                     );
  preview.text("label", label)
      .style(font_size(11.0f).text_color(shell_theme.text_muted));
  preview.render_image_view("image", render_image_key)
      .style(
          background(theme.surface)
              .fill_x()
              .height(px(120.0f))
              .border(1.0f, shell_theme.panel_border)
              .radius(12.0f)
              .overflow_hidden()
              .tint(glm::vec4(preview_exposure, preview_exposure, preview_exposure, 1.0f))
      );
}

ui::dsl::StateStyleBuilder build_toggle_hover_style(
    const WorkspaceShellTheme &shell_theme
) {
  return state()
      .background(shell_theme.accent_soft)
      .border(1.0f, shell_theme.accent);
}

ui::dsl::StyleBuilder build_toolbar_pill(
    bool active,
    const WorkspaceShellTheme &shell_theme
) {
  return background(active ? shell_theme.accent_soft
                           : theme_alpha(shell_theme.panel_background, 0.0f))
      .border(1.0f, active ? shell_theme.accent : shell_theme.panel_border)
      .radius(10.0f)
      .padding_xy(14.0f, 6.0f)
      .text_color(active ? shell_theme.text_primary : shell_theme.text_muted)
      .hover(build_toggle_hover_style(shell_theme))
      .pressed(
          state()
              .background(shell_theme.panel_background)
              .border(1.0f, shell_theme.accent)
      );
}

ui::dsl::StyleBuilder build_navigation_gizmo_style(
    const ViewportPanelTheme &theme
) {
  return absolute()
      .right(px(k_navigation_gizmo_right_offset))
      .top(px(k_navigation_gizmo_top_offset))
      .width(px(k_navigation_gizmo_size))
      .height(px(k_navigation_gizmo_size))
      .padding(0.0f)
      .radius(20.0f)
      .background(theme_alpha(theme.surface, 0.86f))
      .border(1.0f, theme.hud_border)
      .overflow_hidden();
}

ui::dsl::StyleBuilder build_navigation_marker_style(
    const ViewportPanelTheme &theme,
    const NavigationMarkerInstance &marker,
    float left,
    float top,
    float size
) {
  const float color_blend =
      marker.config.prominent
          ? (marker.active ? 0.90f : 0.78f)
          : (marker.active ? 0.60f : 0.42f);
  glm::vec4 fill =
      blend_color(theme.surface, marker.config.color, color_blend);
  fill.a = marker.config.prominent ? 0.98f : 0.90f;

  const glm::vec4 border = marker.active
                               ? theme.hud_transient_text
                               : blend_color(
                                     theme.hud_border,
                                     marker.config.color,
                                     marker.config.prominent ? 0.58f : 0.34f
                                 );

  return absolute()
      .left(px(left))
      .top(px(top))
      .width(px(size))
      .height(px(size))
      .padding(0.0f)
      .items_center()
      .justify_center()
      .radius(size * 0.5f)
      .background(fill)
      .border(1.0f, border)
      .cursor_pointer()
      .hover(
          state()
              .background(blend_color(fill, glm::vec4(1.0f), 0.10f))
              .border(1.0f, marker.active ? theme.hud_transient_text
                                           : theme.hud_text_primary)
      )
      .pressed(
          state()
              .background(blend_color(fill, theme.surface, 0.16f))
              .border(1.0f, theme.hud_transient_text)
      );
}

ui::dsl::StyleBuilder build_navigation_projection_toggle_style(
    const ViewportPanelTheme &theme,
    bool orthographic
) {
  const glm::vec4 accent =
      orthographic ? theme.hud_chip_success_text : theme.hud_transient_text;
  const glm::vec4 background = orthographic
                                   ? theme.hud_chip_success_background
                                   : theme.hud_transient_background;
  const glm::vec4 border = orthographic
                               ? theme.hud_chip_success_border
                               : theme.hud_transient_border;
  const float offset =
      k_navigation_gizmo_size - k_navigation_projection_button_size - 8.0f;

  return absolute()
      .left(px(offset))
      .top(px(offset))
      .width(px(k_navigation_projection_button_size))
      .height(px(k_navigation_projection_button_size))
      .padding(0.0f)
      .items_center()
      .justify_center()
      .radius(k_navigation_projection_button_size * 0.5f)
      .background(background)
      .border(1.0f, border)
      .text_color(accent)
      .cursor_pointer()
      .hover(
          state()
              .background(blend_color(background, glm::vec4(1.0f), 0.06f))
              .border(1.0f, accent)
      )
      .pressed(
          state()
              .background(blend_color(background, theme.surface, 0.18f))
              .border(1.0f, accent)
      );
}

void draw_navigation_gizmo(
    ui::im::Children &viewport_shell,
    const ViewportPanelTheme &theme,
    const rendering::CameraSelection &selection
) {
  auto navigation =
      viewport_shell.view("viewport_navigation")
          .style(build_navigation_gizmo_style(theme));

  const glm::vec2 center(
      k_navigation_gizmo_size * 0.5f,
      k_navigation_gizmo_size * 0.5f
  );

  for (const auto &marker : navigation_marker_instances(selection)) {
    const float depth_factor =
        0.84f + 0.16f * ((marker.view_axis.z + 1.0f) * 0.5f);
    const float base_size = marker.config.prominent
                                ? k_navigation_primary_marker_size
                                : k_navigation_secondary_marker_size;
    const float size = base_size * depth_factor;
    const glm::vec2 marker_center =
        center + glm::vec2(marker.view_axis.x, -marker.view_axis.y) *
                     k_navigation_marker_radius;

    auto button =
        navigation.pressable(std::string(marker.config.id) + "_button")
            .on_click([action = marker.config.action]() {
              editor_viewport_navigation_store()->request_action(action);
            })
            .style(
                build_navigation_marker_style(
                    theme,
                    marker,
                    marker_center.x - size * 0.5f,
                    marker_center.y - size * 0.5f,
                    size
                )
            );
    button.text("label", std::string(marker.config.label))
        .style(
            font_size(marker.config.prominent ? 12.5f : 8.0f)
                .text_color(
                    marker.active ? theme.hud_text_primary
                                  : theme_alpha(theme.hud_text_primary, 0.88f)
                )
        );
  }

  auto projection_toggle =
      navigation.pressable("projection_toggle")
          .on_click([]() {
            editor_viewport_navigation_store()->request_action(
                EditorViewportNavigationAction::ToggleProjection
            );
          })
          .style(
              build_navigation_projection_toggle_style(
                  theme,
                  selection.camera != nullptr &&
                      selection.camera->orthographic
              )
          );
  projection_toggle.text(
      "label",
      selection.camera != nullptr && selection.camera->orthographic ? "O"
                                                                    : "P"
  )
      .style(font_size(10.0f));
}

uint64_t append_viewport_navigation_hash(uint64_t hash) {
  const auto selection = active_viewport_camera_selection();
  hash = fnv1a64_append_value(hash, selection.has_value());
  if (!selection.has_value() || selection->camera == nullptr) {
    return hash;
  }

  const glm::vec3 forward = normalized_or_fallback(
      selection->camera->front,
      glm::vec3(0.0f, 0.0f, -1.0f)
  );
  const glm::vec3 up = normalized_or_fallback(
      selection->camera->up,
      glm::vec3(0.0f, 1.0f, 0.0f)
  );

  hash = fnv1a64_append_value(hash, selection->entity_id);
  hash = fnv1a64_append_value(hash, selection->camera->orthographic);
  hash = fnv1a64_append_value(hash, forward.x);
  hash = fnv1a64_append_value(hash, forward.y);
  hash = fnv1a64_append_value(hash, forward.z);
  hash = fnv1a64_append_value(hash, up.x);
  hash = fnv1a64_append_value(hash, up.y);
  hash = fnv1a64_append_value(hash, up.z);
  return hash;
}

} // namespace

void ViewportPanelController::render(ui::im::Frame &ui) {
  ASTRA_PROFILE_N("ViewportPanel::render");
  const ViewportPanelTheme theme;
  const WorkspaceShellTheme shell_theme;

  auto root = ui.column("root").style(
      fill()
          .background(theme.surface)
          .gap(0.0f)
          .padding(0.0f)
          .overflow_hidden()
  );

  if (auto toolbar = root.column("toolbar")
          .frozen(m_toolbar_version == m_last_rendered_toolbar_version)
          .style(
              fill_x()
                  .gap(6.0f)
                  .padding(10.0f)
                  .background(shell_theme.panel_background)
          )) {
    m_last_rendered_toolbar_version = m_toolbar_version;
    ASTRA_PROFILE_N("ViewportPanel::toolbar");
    auto controls = toolbar.row("controls").style(fill_x().items_center().gap(8.0f));
    controls
        .segmented_control(
            "mode",
            {"Translate", "Rotate", "Scale"},
            gizmo::mode_to_index(editor_gizmo_store()->mode())
        )
        .accent_colors({
            glm::vec4(0.90f, 0.25f, 0.25f, 1.0f),
            glm::vec4(0.25f, 0.80f, 0.25f, 1.0f),
            glm::vec4(0.30f, 0.50f, 0.95f, 1.0f),
        })
        .style(
            width(px(310.0f)),
            background(theme.surface),
            border(1.0f, shell_theme.panel_border),
            radius(10.0f)
        )
        .on_select([this](size_t index, const std::string &) {
          set_mode(gizmo::mode_from_index(index));
        });
    controls.view("divider_left")
        .style(
            width(px(1.0f))
                .height(px(22.0f))
                .background(shell_theme.panel_border)
        );
    controls.button("grid", "Grid", [this]() { toggle_grid(); })
        .style(build_toolbar_pill(m_show_grid, shell_theme));
    controls.button("snap", "Snap", [this]() { toggle_snap(); })
        .style(build_toolbar_pill(m_snap_enabled, shell_theme));
    controls
        .segmented_control(
            "navigation_mode",
            {"Free Move", "Orbit Nav"},
            navigation_preset_index(editor_camera_navigation_store()->preset())
        )
        .accent_colors({
            glm::vec4(0.37f, 0.65f, 0.96f, 1.0f),
            glm::vec4(0.96f, 0.62f, 0.04f, 1.0f),
        })
        .style(
            width(px(230.0f)),
            background(theme.surface),
            border(1.0f, shell_theme.panel_border),
            radius(10.0f)
        )
        .on_select([](size_t index, const std::string &) {
          editor_camera_navigation_store()->set_preset(
              navigation_preset_from_index(index)
          );
        });
    controls.spacer("toolbar_spacer");
    controls
        .button(
            "attachments_toggle",
            m_show_attachments ? "Hide Buffers" : "Buffers",
            [this]() { toggle_attachments(); }
        )
        .style(build_toolbar_pill(m_show_attachments, shell_theme));
    controls.view("divider_right")
        .style(
            width(px(1.0f))
                .height(px(22.0f))
                .background(shell_theme.panel_border)
        );
    controls.text("view_label_prefix", "View:")
        .style(font_size(11.0f).text_color(shell_theme.text_muted));
    controls.text("view_label", m_main_view.label)
        .style(font_size(11.0f).text_color(shell_theme.text_primary));
  }

  if (auto attachments = root.scroll_view("attachments").visible(m_show_attachments).style(
      fill_x()
          .height(px(170.0f))
          .background(shell_theme.panel_background)
          .border(1.0f, shell_theme.panel_border)
          .radius(14.0f)
          .padding(10.0f)
          .overflow_hidden()
          .scrollbar_auto()
          .scroll_horizontal()
          .scrollbar_thickness(8.0f)
  )) {
    ASTRA_PROFILE_N("ViewportPanel::attachments");
    auto items = attachments.row("items").style(gap(10.0f).items_start());
    for (size_t index = 0u; index < m_attachment_views.size(); ++index) {
      draw_attachment_preview(
          items,
          "attachment_" + std::to_string(index),
          m_attachment_views[index].label,
          m_attachment_views[index].key,
          m_attachment_views[index].preview_exposure,
          [this, index]() { swap_attachment_into_main(index); },
          theme,
          shell_theme
      );
    }
  }

  {
  ASTRA_PROFILE_N("ViewportPanel::viewport_shell");
  auto viewport_shell = root.view("viewport_shell").style(
      fill_x()
          .flex(1.0f)
          .min_height(px(0.0f))
          .background(shell_theme.panel_background)
          .border(1.0f, shell_theme.panel_border)
          .radius(14.0f)
          .overflow_hidden()
  );
  auto image = viewport_shell.render_image_view("image", m_main_view.key).style(
      fill().background(theme.surface)
  );
  m_viewport_image_widget = image.widget_id();

  }
}

void ViewportPanelController::mount(const PanelMountContext &context) {
  LOG_INFO("[ViewportPanelController] mount() called, runtime=", context.runtime);
  m_runtime = context.runtime;
  m_last_rendered_toolbar_version = std::numeric_limits<uint64_t>::max();
  sync_panel_rect();
}

std::optional<uint64_t> ViewportPanelController::render_version() const {
  uint64_t hash = k_fnv1a64_offset_basis;
  hash = fnv1a64_append_value(hash, editor_gizmo_store()->mode());
  hash = fnv1a64_append_value(
      hash,
      editor_camera_navigation_store()->revision()
  );
  hash = fnv1a64_append_value(hash, m_show_attachments);
  hash = fnv1a64_append_value(hash, m_show_grid);
  hash = fnv1a64_append_value(hash, m_snap_enabled);
  hash = fnv1a64_append_string(m_main_view.label, hash);
  hash = fnv1a64_append_value(hash, m_main_view.key);

  for (const RenderViewSlot &slot : m_attachment_views) {
    hash = fnv1a64_append_string(slot.label, hash);
    hash = fnv1a64_append_value(hash, slot.key);
  }

  return hash;
}

void ViewportPanelController::unmount() {
  m_runtime = nullptr;
  m_viewport_image_widget = ui::im::k_invalid_widget_id;
  m_last_camera_navigation_revision = 0u;
  m_last_rendered_toolbar_version = std::numeric_limits<uint64_t>::max();
  editor_gizmo_store()->set_panel_rect(std::nullopt);
}

void ViewportPanelController::update(const PanelUpdateContext &context) {
  static int update_count = 0;
  if (update_count++ < 3) {
    LOG_INFO("[ViewportPanelController] update() called");
  }

  const uint64_t navigation_revision =
      editor_camera_navigation_store()->revision();
  if (navigation_revision != m_last_camera_navigation_revision) {
    m_last_camera_navigation_revision = navigation_revision;
    ++m_toolbar_version;
  }

  sync_panel_rect();
}

void ViewportPanelController::set_mode(EditorGizmoMode mode) {
  editor_gizmo_store()->set_mode(mode);
  ++m_toolbar_version;
}

void ViewportPanelController::toggle_attachments() {
  m_show_attachments = !m_show_attachments;
  ++m_toolbar_version;
}

void ViewportPanelController::toggle_grid() {
  m_show_grid = !m_show_grid;
  ++m_toolbar_version;
}

void ViewportPanelController::toggle_snap() {
  m_snap_enabled = !m_snap_enabled;
  ++m_toolbar_version;
}

void ViewportPanelController::swap_attachment_into_main(size_t index) {
  if (index >= m_attachment_views.size()) {
    return;
  }

  std::swap(m_main_view, m_attachment_views[index]);
}

void ViewportPanelController::sync_panel_rect() {
  if (m_runtime != nullptr && m_viewport_image_widget) {
    auto bounds = m_runtime->layout_bounds(m_viewport_image_widget);
    static int sync_log_count = 0;
    if (sync_log_count++ < 5) {
      LOG_INFO("[ViewportPanelController] sync_panel_rect: widget=",
               m_viewport_image_widget.value,
               " bounds=", bounds.has_value());
    }
    editor_gizmo_store()->set_panel_rect(bounds);
    return;
  }
  static int sync_fail_count = 0;
  if (sync_fail_count++ < 5) {
    LOG_WARN("[ViewportPanelController] sync_panel_rect: runtime=",
             m_runtime != nullptr, " widget=",
             m_viewport_image_widget.value);
  }
  editor_gizmo_store()->set_panel_rect(std::nullopt);
}

} // namespace astralix::editor
