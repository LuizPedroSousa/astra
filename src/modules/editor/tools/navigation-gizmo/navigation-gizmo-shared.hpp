#pragma once

#include "components/camera.hpp"
#include "editor-viewport-navigation-store.hpp"
#include "glm/geometric.hpp"
#include "managers/scene-manager.hpp"
#include "systems/render-system/scene-selection.hpp"

#include <algorithm>
#include <array>
#include <optional>
#include <string_view>
#include <vector>

namespace astralix::editor::navigation_gizmo {

inline constexpr float k_body_size = 132.0f;
inline constexpr float k_primary_marker_size = 30.0f;
inline constexpr float k_secondary_marker_size = 22.0f;
inline constexpr float k_projection_button_height = 28.0f;
inline constexpr float k_active_alignment_threshold = 0.985f;

inline constexpr float k_camera_distance = 3.25f;
inline constexpr float k_orthographic_extent = 1.0f;
inline constexpr float k_positive_tip_length = 0.62f;
inline constexpr float k_negative_tip_length = 0.42f;
inline constexpr float k_positive_shaft_length = 0.48f;
inline constexpr float k_negative_shaft_length = 0.34f;
inline constexpr float k_positive_shaft_radius = 0.045f;
inline constexpr float k_negative_shaft_radius = 0.034f;
inline constexpr float k_positive_head_radius = 0.095f;
inline constexpr float k_negative_cap_half = 0.060f;
inline constexpr float k_origin_cube_half = 0.085f;
inline constexpr float k_epsilon = 0.0001f;

inline glm::vec3 normalized_or_fallback(glm::vec3 value, glm::vec3 fallback) {
  const float length_squared = glm::dot(value, value);
  if (length_squared <= k_epsilon) {
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
  float tip_length = k_positive_tip_length;
};

struct NavigationMarkerInstance {
  NavigationMarkerConfig config;
  glm::vec3 view_axis = glm::vec3(0.0f);
  bool active = false;
};

struct NavigationMarkerProjection {
  glm::vec2 center = glm::vec2(0.0f);
  float size = 0.0f;
};

inline std::array<NavigationMarkerConfig, 6u> navigation_marker_configs() {
  return {{
      NavigationMarkerConfig{
          .id = "x_pos",
          .label = "X",
          .axis = glm::vec3(1.0f, 0.0f, 0.0f),
          .action = EditorViewportNavigationAction::Right,
          .color = glm::vec4(0.90f, 0.25f, 0.25f, 1.0f),
          .prominent = true,
          .tip_length = k_positive_tip_length,
      },
      NavigationMarkerConfig{
          .id = "x_neg",
          .label = "-X",
          .axis = glm::vec3(-1.0f, 0.0f, 0.0f),
          .action = EditorViewportNavigationAction::Left,
          .color = glm::vec4(0.90f, 0.25f, 0.25f, 1.0f),
          .prominent = false,
          .tip_length = k_negative_tip_length,
      },
      NavigationMarkerConfig{
          .id = "y_pos",
          .label = "Y",
          .axis = glm::vec3(0.0f, 1.0f, 0.0f),
          .action = EditorViewportNavigationAction::Top,
          .color = glm::vec4(0.40f, 0.84f, 0.32f, 1.0f),
          .prominent = true,
          .tip_length = k_positive_tip_length,
      },
      NavigationMarkerConfig{
          .id = "y_neg",
          .label = "-Y",
          .axis = glm::vec3(0.0f, -1.0f, 0.0f),
          .action = EditorViewportNavigationAction::Bottom,
          .color = glm::vec4(0.40f, 0.84f, 0.32f, 1.0f),
          .prominent = false,
          .tip_length = k_negative_tip_length,
      },
      NavigationMarkerConfig{
          .id = "z_pos",
          .label = "Z",
          .axis = glm::vec3(0.0f, 0.0f, 1.0f),
          .action = EditorViewportNavigationAction::Front,
          .color = glm::vec4(0.30f, 0.50f, 0.95f, 1.0f),
          .prominent = true,
          .tip_length = k_positive_tip_length,
      },
      NavigationMarkerConfig{
          .id = "z_neg",
          .label = "-Z",
          .axis = glm::vec3(0.0f, 0.0f, -1.0f),
          .action = EditorViewportNavigationAction::Back,
          .color = glm::vec4(0.30f, 0.50f, 0.95f, 1.0f),
          .prominent = false,
          .tip_length = k_negative_tip_length,
      },
  }};
}

inline glm::vec3 navigation_action_forward(
    EditorViewportNavigationAction action
) {
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

inline std::optional<rendering::CameraSelection>
active_viewport_camera_selection() {
  auto scene_manager = SceneManager::get();
  Scene *scene =
      scene_manager != nullptr ? scene_manager->get_active_scene() : nullptr;
  if (scene == nullptr) {
    return std::nullopt;
  }

  return rendering::select_main_camera(scene->world());
}

inline std::vector<NavigationMarkerInstance> navigation_marker_instances(
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
            k_active_alignment_threshold,
    });
  }

  std::sort(
      instances.begin(),
      instances.end(),
      [](const NavigationMarkerInstance &lhs,
         const NavigationMarkerInstance &rhs) {
        return lhs.view_axis.z < rhs.view_axis.z;
      }
  );

  return instances;
}

inline NavigationMarkerProjection project_marker_to_rect(
    const NavigationMarkerInstance &marker,
    const ui::UIRect &rect
) {
  const float base_size =
      marker.config.prominent ? k_primary_marker_size : k_secondary_marker_size;
  const float depth_factor =
      0.84f + 0.16f * ((marker.view_axis.z + 1.0f) * 0.5f);
  const float half_extent = std::min(rect.width, rect.height) * 0.5f;

  return NavigationMarkerProjection{
      .center =
          glm::vec2(
              rect.x + rect.width * 0.5f,
              rect.y + rect.height * 0.5f
          ) +
          glm::vec2(marker.view_axis.x, -marker.view_axis.y) *
              half_extent * marker.config.tip_length,
      .size = base_size * depth_factor,
  };
}

} // namespace astralix::editor::navigation_gizmo
