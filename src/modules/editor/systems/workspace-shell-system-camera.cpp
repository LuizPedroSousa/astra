#include "systems/workspace-shell-system-internal.hpp"

#include "editor-camera-navigation-store.hpp"
#include "editor-viewport-navigation-store.hpp"
#include "systems/camera-system/camera-controller-system.hpp"
#include "systems/transform-system/transform-system.hpp"

#include <iomanip>
#include <sstream>
#include <string_view>
#include <type_traits>

namespace astralix::editor {
using namespace workspace_shell_detail;

namespace {

constexpr float k_min_camera_distance = 0.25f;
constexpr float k_default_focus_distance = 6.0f;
constexpr float k_max_focus_distance = 24.0f;
constexpr float k_default_pivot_distance = 10.0f;
constexpr float k_min_orthographic_scale = 0.1f;
constexpr float k_zoom_factor_base = 0.85f;
constexpr float k_clip_override_min_near_plane = 0.02f;
constexpr float k_clip_override_min_far_plane = 8.0f;
constexpr float k_clip_override_perspective_near_ratio = 0.1f;
constexpr float k_clip_override_perspective_far_ratio = 8.0f;
constexpr float k_clip_override_orthographic_scale_ratio = 4.0f;
constexpr float k_clip_override_orthographic_distance_ratio = 2.0f;
constexpr float k_numpad_pan_step_ratio = 0.12f;
constexpr float k_numpad_pan_step_min_pixels = 56.0f;
constexpr float k_axis_view_alignment_threshold = 0.92f;
constexpr input::KeyCode k_navigation_preset_toggle_key = input::KeyCode::O;

bool shift_down() {
  return input::IS_KEY_DOWN(input::KeyCode::LeftShift) ||
         input::IS_KEY_DOWN(input::KeyCode::RightShift);
}

bool control_down() {
  return input::IS_KEY_DOWN(input::KeyCode::LeftControl) ||
         input::IS_KEY_DOWN(input::KeyCode::RightControl);
}

float active_window_aspect_ratio() {
  auto window = window_manager()->active_window();
  if (window == nullptr || window->height() == 0) {
    return 1.0f;
  }

  return static_cast<float>(window->width()) /
         static_cast<float>(window->height());
}

scene::CameraController *find_camera_controller(
    Scene &scene,
    EntityID entity_id
) {
  return scene.world().get<scene::CameraController>(entity_id);
}

void update_inverse_side_shortcut_latch(
    input::KeyCode key,
    bool inverse_modifier_down,
    bool &active,
    bool &inverse
) {
  if (input::IS_KEY_DOWN(key)) {
    if (!active) {
      active = true;
      inverse = inverse_modifier_down;
    }
    return;
  }

  if (!input::IS_KEY_RELEASED(key)) {
    active = false;
    inverse = false;
  }
}

void clear_inverse_side_shortcut_latch(bool &active, bool &inverse) {
  active = false;
  inverse = false;
}

glm::vec3 normalized_or_fallback(glm::vec3 value, glm::vec3 fallback) {
  const float length_squared = glm::dot(value, value);
  if (length_squared <= gizmo::k_epsilon) {
    return fallback;
  }

  return glm::normalize(value);
}

glm::vec3 camera_forward(const SelectedCamera &selection) {
  return normalized_or_fallback(
      selection.camera->front,
      glm::vec3(0.0f, 0.0f, -1.0f)
  );
}

glm::vec3 camera_up(const SelectedCamera &selection) {
  return normalized_or_fallback(
      selection.camera->up,
      glm::vec3(0.0f, 1.0f, 0.0f)
  );
}

std::string trim_decimal_string(std::string value) {
  const size_t decimal = value.find('.');
  if (decimal == std::string::npos) {
    return value;
  }

  while (!value.empty() && value.back() == '0') {
    value.pop_back();
  }
  if (!value.empty() && value.back() == '.') {
    value.pop_back();
  }

  return value.empty() ? std::string("0") : value;
}

std::string format_number(float value, int precision, bool show_sign = false) {
  if (std::abs(value) <= gizmo::k_epsilon) {
    value = 0.0f;
  }

  std::ostringstream stream;
  stream << std::fixed << std::setprecision(precision);
  if (show_sign) {
    stream << std::showpos;
  }
  stream << value;
  return trim_decimal_string(stream.str());
}

std::string format_delta_component(std::string_view axis, float value) {
  return std::string("D") + std::string(axis) + " " +
         format_number(value, 2, true);
}

std::string axis_label(const glm::vec3 &axis) {
  if (axis.x > 0.5f) {
    return "X";
  }
  if (axis.y > 0.5f) {
    return "Y";
  }
  return "Z";
}

std::string plane_label_from_locked_axis(const glm::vec3 &axis) {
  if (axis.x > 0.5f) {
    return "YZ";
  }
  if (axis.y > 0.5f) {
    return "XZ";
  }
  return "XY";
}

std::string other_axes_hint(const glm::vec3 &axis) {
  if (axis.x > 0.5f) {
    return "Y/Z axis";
  }
  if (axis.y > 0.5f) {
    return "X/Z axis";
  }
  return "X/Y axis";
}

std::string fallback_entity_label(EntityID entity_id) {
  return "Entity " + std::to_string(static_cast<uint64_t>(entity_id));
}

std::string selected_target_label(
    Scene *scene,
    std::optional<EntityID> entity_id
) {
  if (!entity_id.has_value()) {
    return {};
  }

  if (scene != nullptr && scene->world().contains(*entity_id)) {
    return std::string(scene->world().name(*entity_id));
  }

  return fallback_entity_label(*entity_id);
}

std::string gizmo_mode_title(EditorGizmoMode mode, bool modal_active) {
  switch (mode) {
    case EditorGizmoMode::Rotate:
      return modal_active ? "Rotate" : "Rotate Tool";
    case EditorGizmoMode::Scale:
      return modal_active ? "Scale" : "Scale Tool";
    case EditorGizmoMode::Translate:
    default:
      return modal_active ? "Move" : "Move Tool";
  }
}

std::string navigation_preset_label(EditorCameraNavigationPreset preset) {
  return preset == EditorCameraNavigationPreset::Orbit ? "Orbit Nav"
                                                       : "Free Move";
}

std::string navigation_preset_transient(EditorCameraNavigationPreset preset) {
  return preset == EditorCameraNavigationPreset::Orbit ? "Orbit Navigation"
                                                       : "Free Navigation";
}

std::string snap_chip_label(EditorGizmoMode mode) {
  switch (mode) {
    case EditorGizmoMode::Rotate:
      return "Snap 5deg";
    case EditorGizmoMode::Scale:
      return "Snap 0.1x";
    case EditorGizmoMode::Translate:
    default:
      return "Snap 1m";
  }
}

std::string axis_view_label(const glm::vec3 &forward) {
  const glm::vec3 normalized =
      normalized_or_fallback(forward, glm::vec3(0.0f, 0.0f, -1.0f));
  const float abs_x = std::abs(normalized.x);
  const float abs_y = std::abs(normalized.y);
  const float abs_z = std::abs(normalized.z);
  const float dominant = std::max({abs_x, abs_y, abs_z});
  if (dominant < k_axis_view_alignment_threshold) {
    return {};
  }

  if (dominant == abs_y) {
    return normalized.y < 0.0f ? "Top" : "Bottom";
  }
  if (dominant == abs_x) {
    return normalized.x < 0.0f ? "Right" : "Left";
  }
  return normalized.z < 0.0f ? "Front" : "Back";
}

std::string projection_chip_label(const SelectedCamera &selection) {
  const std::string orientation = axis_view_label(camera_forward(selection));
  if (selection.camera->orthographic) {
    return orientation.empty() ? "Ortho" : "Ortho " + orientation;
  }

  return orientation.empty() ? "Perspective" : "Perspective " + orientation;
}

std::string projection_transient(const SelectedCamera &selection) {
  return selection.camera->orthographic ? "Orthographic View"
                                        : "Perspective View";
}

float safe_scale_ratio(float current, float origin) {
  if (std::abs(origin) <= gizmo::k_epsilon) {
    return 1.0f;
  }

  return current / origin;
}

std::optional<glm::vec3> selected_entity_pivot(Scene &scene) {
  const auto selected_entity = editor_selection_store()->selected_entity();
  if (!selected_entity.has_value() ||
      !scene.world().contains(*selected_entity) ||
      !scene.world().has<scene::Transform>(*selected_entity)) {
    return std::nullopt;
  }

  const auto *transform = scene.world().get<scene::Transform>(*selected_entity);
  return transform != nullptr ? std::optional<glm::vec3>(transform->position)
                              : std::nullopt;
}

std::optional<glm::vec3> controller_target_pivot(
    Scene &scene,
    const scene::CameraController *controller
) {
  if (controller == nullptr || !controller->target.has_value() ||
      !scene.world().contains(*controller->target) ||
      !scene.world().has<scene::Transform>(*controller->target)) {
    return std::nullopt;
  }

  const auto *transform = scene.world().get<scene::Transform>(*controller->target);
  return transform != nullptr ? std::optional<glm::vec3>(transform->position)
                              : std::nullopt;
}

glm::vec3 fallback_navigation_pivot(
    const SelectedCamera &selection,
    const scene::CameraController *controller
) {
  float distance = controller != nullptr
                       ? std::max(controller->orbit_distance, k_min_camera_distance)
                       : k_default_pivot_distance;
  if (selection.camera->orthographic) {
    distance = std::max(distance, selection.camera->orthographic_scale * 2.0f);
  }

  return selection.transform->position + camera_forward(selection) * distance;
}

glm::vec3 resolve_navigation_pivot(
    Scene &scene,
    const SelectedCamera &selection,
    const scene::CameraController *controller
) {
  if (const auto selected = selected_entity_pivot(scene); selected.has_value()) {
    return *selected;
  }

  if (const auto target = controller_target_pivot(scene, controller);
      target.has_value()) {
    return *target;
  }

  return fallback_navigation_pivot(selection, controller);
}

void clear_camera_clip_plane_overrides(rendering::Camera &camera) {
  camera.runtime_near_plane_override.reset();
  camera.runtime_far_plane_override.reset();
}

void update_camera_clip_plane_overrides(
    Scene &scene,
    const SelectedCamera &selection,
    const scene::CameraController *controller
) {
  const glm::vec3 pivot =
      resolve_navigation_pivot(scene, selection, controller);
  const float focus_distance = std::max(
      glm::length(pivot - selection.transform->position),
      k_min_camera_distance
  );

  float near_plane = selection.camera->near_plane;
  float far_plane = selection.camera->far_plane;

  if (selection.camera->orthographic) {
    const float half_depth_span = std::max(
        k_clip_override_min_far_plane * 0.5f,
        std::max(
            selection.camera->orthographic_scale *
                k_clip_override_orthographic_scale_ratio,
            focus_distance * k_clip_override_orthographic_distance_ratio
        )
    );

    near_plane = std::max(
        selection.camera->near_plane,
        std::max(
            k_clip_override_min_near_plane,
            focus_distance - half_depth_span
        )
    );
    far_plane = std::min(
        selection.camera->far_plane,
        focus_distance + half_depth_span
    );
  } else {
    near_plane = std::max(
        selection.camera->near_plane,
        std::max(
            k_clip_override_min_near_plane,
            focus_distance * k_clip_override_perspective_near_ratio
        )
    );
    far_plane = std::min(
        selection.camera->far_plane,
        std::max(
            k_clip_override_min_far_plane,
            focus_distance * k_clip_override_perspective_far_ratio
        )
    );
  }

  if (far_plane <= near_plane + 0.001f) {
    clear_camera_clip_plane_overrides(*selection.camera);
    return;
  }

  selection.camera->runtime_near_plane_override = near_plane;
  selection.camera->runtime_far_plane_override = far_plane;
}

void sync_controller_orientation(
    scene::CameraController *controller,
    const glm::vec3 &forward
) {
  if (controller == nullptr) {
    return;
  }

  const glm::vec3 normalized_forward =
      normalized_or_fallback(forward, glm::vec3(0.0f, 0.0f, -1.0f));
  controller->yaw =
      glm::degrees(std::atan2(normalized_forward.z, normalized_forward.x));
  controller->pitch = std::clamp(
      glm::degrees(std::asin(std::clamp(normalized_forward.y, -1.0f, 1.0f))),
      -89.0f,
      89.0f
  );
}

void refresh_camera_projection(const SelectedCamera &selection) {
  const float aspect_ratio = active_window_aspect_ratio();
  if (selection.camera->orthographic) {
    scene::recalculate_camera_orthographic_matrix(
        *selection.camera,
        *selection.transform,
        aspect_ratio
    );
    return;
  }

  scene::recalculate_camera_projection_matrix(
      *selection.camera,
      *selection.transform,
      aspect_ratio
  );
}

void commit_camera_state(
    Scene &scene,
    const SelectedCamera &selection,
    scene::CameraController *controller,
    const glm::vec3 &position,
    const glm::vec3 &forward,
    const glm::vec3 &up
) {
  const glm::vec3 normalized_forward =
      normalized_or_fallback(forward, glm::vec3(0.0f, 0.0f, -1.0f));
  const glm::vec3 normalized_up =
      normalized_or_fallback(up, glm::vec3(0.0f, 1.0f, 0.0f));

  selection.transform->position = position;
  selection.transform->dirty = true;
  scene::recalculate_transform(*selection.transform);

  selection.camera->front = normalized_forward;
  selection.camera->direction = normalized_forward;
  selection.camera->up = normalized_up;

  sync_controller_orientation(controller, normalized_forward);
  update_camera_clip_plane_overrides(scene, selection, controller);

  scene::recalculate_camera_view_matrix(
      *selection.camera,
      *selection.transform,
      active_window_aspect_ratio()
  );
  refresh_camera_projection(selection);

  scene.world().touch();
}

void orbit_camera(
    Scene &scene,
    const SelectedCamera &selection,
    scene::CameraController *controller,
    const glm::vec3 &pivot,
    glm::vec2 mouse_delta
) {
  const glm::vec3 current_forward = camera_forward(selection);
  const float distance = std::max(
      glm::length(pivot - selection.transform->position),
      k_min_camera_distance
  );
  const float sensitivity = controller != nullptr ? controller->sensitivity : 0.08f;

  float yaw = glm::degrees(std::atan2(current_forward.z, current_forward.x));
  float pitch = glm::degrees(
      std::asin(std::clamp(current_forward.y, -1.0f, 1.0f))
  );

  yaw += mouse_delta.x * sensitivity;
  pitch -= mouse_delta.y * sensitivity;
  pitch = std::clamp(pitch, -89.0f, 89.0f);

  const glm::vec3 next_forward = glm::normalize(glm::vec3(std::cos(glm::radians(yaw)) * std::cos(glm::radians(pitch)), std::sin(glm::radians(pitch)), std::sin(glm::radians(yaw)) * std::cos(glm::radians(pitch))));
  const glm::vec3 next_position = pivot - next_forward * distance;

  commit_camera_state(
      scene,
      selection,
      controller,
      next_position,
      next_forward,
      glm::vec3(0.0f, 1.0f, 0.0f)
  );

  if (controller != nullptr) {
    controller->yaw = yaw;
    controller->pitch = pitch;
    controller->orbit_distance = distance;
  }
}

void pan_camera(
    Scene &scene,
    const SelectedCamera &selection,
    scene::CameraController *controller,
    const ui::UIRect &interaction_rect,
    const glm::vec3 &pivot,
    glm::vec2 mouse_delta
) {
  const gizmo::CameraFrame frame =
      gizmo::make_camera_frame(*selection.transform, *selection.camera);
  const glm::vec3 forward = frame.forward;
  const glm::vec3 up = camera_up(selection);
  glm::vec3 right = glm::cross(forward, up);
  if (glm::dot(right, right) <= gizmo::k_epsilon) {
    right = glm::vec3(1.0f, 0.0f, 0.0f);
  } else {
    right = glm::normalize(right);
  }

  const float units_per_pixel =
      gizmo::world_units_per_pixel(frame, pivot, interaction_rect.height);
  const glm::vec3 translation =
      (-right * mouse_delta.x + up * mouse_delta.y) * units_per_pixel;

  commit_camera_state(
      scene,
      selection,
      controller,
      selection.transform->position + translation,
      forward,
      up
  );
}

void pan_camera_step(
    Scene &scene,
    const SelectedCamera &selection,
    scene::CameraController *controller,
    const ui::UIRect &interaction_rect,
    const glm::vec3 &pivot,
    glm::vec2 normalized_step
) {
  const glm::vec2 step_pixels(
      normalized_step.x *
          std::max(
              k_numpad_pan_step_min_pixels,
              interaction_rect.width * k_numpad_pan_step_ratio
          ),
      normalized_step.y *
          std::max(
              k_numpad_pan_step_min_pixels,
              interaction_rect.height * k_numpad_pan_step_ratio
          )
  );
  pan_camera(
      scene,
      selection,
      controller,
      interaction_rect,
      pivot,
      step_pixels
  );
}

void zoom_camera(
    Scene &scene,
    const SelectedCamera &selection,
    scene::CameraController *controller,
    const glm::vec3 &pivot,
    float wheel_delta
) {
  if (std::abs(wheel_delta) <= gizmo::k_epsilon) {
    return;
  }

  if (selection.camera->orthographic) {
    selection.camera->orthographic_scale = std::max(
        k_min_orthographic_scale,
        selection.camera->orthographic_scale *
            std::pow(k_zoom_factor_base, wheel_delta)
    );
    commit_camera_state(
        scene,
        selection,
        controller,
        selection.transform->position,
        camera_forward(selection),
        camera_up(selection)
    );
    return;
  }

  const glm::vec3 view_direction = normalized_or_fallback(
      pivot - selection.transform->position,
      camera_forward(selection)
  );
  const float distance = std::max(
      glm::length(pivot - selection.transform->position),
      k_min_camera_distance
  );
  const float next_distance = std::max(
      k_min_camera_distance,
      distance * std::pow(k_zoom_factor_base, wheel_delta)
  );
  const glm::vec3 next_position = pivot - view_direction * next_distance;

  commit_camera_state(
      scene,
      selection,
      controller,
      next_position,
      camera_forward(selection),
      camera_up(selection)
  );

  if (controller != nullptr) {
    controller->orbit_distance = next_distance;
  }
}

void toggle_projection_mode(
    Scene &scene,
    const SelectedCamera &selection,
    scene::CameraController *controller,
    const glm::vec3 &pivot
) {
  if (!selection.camera->orthographic) {
    const float distance = std::max(
        glm::length(pivot - selection.transform->position),
        k_min_camera_distance
    );
    selection.camera->orthographic_scale = std::max(
        k_min_orthographic_scale,
        distance * std::tan(glm::radians(selection.camera->fov_degrees) * 0.5f)
    );
    selection.camera->orthographic = true;
  } else {
    selection.camera->orthographic = false;
  }

  commit_camera_state(
      scene,
      selection,
      controller,
      selection.transform->position,
      camera_forward(selection),
      camera_up(selection)
  );
}

void snap_axis_view(
    Scene &scene,
    const SelectedCamera &selection,
    scene::CameraController *controller,
    const glm::vec3 &pivot,
    glm::vec3 forward,
    glm::vec3 up
);

void snap_to_front_view(
    Scene &scene,
    const SelectedCamera &selection,
    scene::CameraController *controller,
    const glm::vec3 &pivot
) {
  snap_axis_view(
      scene,
      selection,
      controller,
      pivot,
      glm::vec3(0.0f, 0.0f, -1.0f),
      glm::vec3(0.0f, 1.0f, 0.0f)
  );
  editor_viewport_hud_store()->show_transient_message("Front View");
}

void snap_to_back_view(
    Scene &scene,
    const SelectedCamera &selection,
    scene::CameraController *controller,
    const glm::vec3 &pivot
) {
  snap_axis_view(
      scene,
      selection,
      controller,
      pivot,
      glm::vec3(0.0f, 0.0f, 1.0f),
      glm::vec3(0.0f, 1.0f, 0.0f)
  );
  editor_viewport_hud_store()->show_transient_message("Back View");
}

void snap_to_right_view(
    Scene &scene,
    const SelectedCamera &selection,
    scene::CameraController *controller,
    const glm::vec3 &pivot
) {
  snap_axis_view(
      scene,
      selection,
      controller,
      pivot,
      glm::vec3(-1.0f, 0.0f, 0.0f),
      glm::vec3(0.0f, 1.0f, 0.0f)
  );
  editor_viewport_hud_store()->show_transient_message("Right View");
}

void snap_to_left_view(
    Scene &scene,
    const SelectedCamera &selection,
    scene::CameraController *controller,
    const glm::vec3 &pivot
) {
  snap_axis_view(
      scene,
      selection,
      controller,
      pivot,
      glm::vec3(1.0f, 0.0f, 0.0f),
      glm::vec3(0.0f, 1.0f, 0.0f)
  );
  editor_viewport_hud_store()->show_transient_message("Left View");
}

void snap_to_top_view(
    Scene &scene,
    const SelectedCamera &selection,
    scene::CameraController *controller,
    const glm::vec3 &pivot
) {
  snap_axis_view(
      scene,
      selection,
      controller,
      pivot,
      glm::vec3(0.0f, -1.0f, 0.0f),
      glm::vec3(0.0f, 0.0f, -1.0f)
  );
  editor_viewport_hud_store()->show_transient_message("Top View");
}

void snap_to_bottom_view(
    Scene &scene,
    const SelectedCamera &selection,
    scene::CameraController *controller,
    const glm::vec3 &pivot
) {
  snap_axis_view(
      scene,
      selection,
      controller,
      pivot,
      glm::vec3(0.0f, 1.0f, 0.0f),
      glm::vec3(0.0f, 0.0f, 1.0f)
  );
  editor_viewport_hud_store()->show_transient_message("Bottom View");
}

void execute_navigation_action(
    Scene &scene,
    const SelectedCamera &selection,
    scene::CameraController *controller,
    EditorViewportNavigationAction action
) {
  const glm::vec3 pivot =
      resolve_navigation_pivot(scene, selection, controller);

  switch (action) {
    case EditorViewportNavigationAction::Front:
      snap_to_front_view(scene, selection, controller, pivot);
      return;
    case EditorViewportNavigationAction::Back:
      snap_to_back_view(scene, selection, controller, pivot);
      return;
    case EditorViewportNavigationAction::Right:
      snap_to_right_view(scene, selection, controller, pivot);
      return;
    case EditorViewportNavigationAction::Left:
      snap_to_left_view(scene, selection, controller, pivot);
      return;
    case EditorViewportNavigationAction::Top:
      snap_to_top_view(scene, selection, controller, pivot);
      return;
    case EditorViewportNavigationAction::Bottom:
      snap_to_bottom_view(scene, selection, controller, pivot);
      return;
    case EditorViewportNavigationAction::ToggleProjection:
      toggle_projection_mode(scene, selection, controller, pivot);
      editor_viewport_hud_store()->show_transient_message(
          projection_transient(selection)
      );
      return;
  }
}

void snap_axis_view(
    Scene &scene,
    const SelectedCamera &selection,
    scene::CameraController *controller,
    const glm::vec3 &pivot,
    glm::vec3 forward,
    glm::vec3 up
) {
  const float distance = std::max(
      glm::length(pivot - selection.transform->position),
      k_min_camera_distance
  );

  commit_camera_state(
      scene,
      selection,
      controller,
      pivot - glm::normalize(forward) * distance,
      glm::normalize(forward),
      glm::normalize(up)
  );

  if (controller != nullptr) {
    controller->orbit_distance = distance;
  }
}

void focus_selected_entity(
    Scene &scene,
    const SelectedCamera &selection,
    scene::CameraController *controller
) {
  const auto pivot = selected_entity_pivot(scene);
  if (!pivot.has_value()) {
    return;
  }

  const glm::vec3 forward = camera_forward(selection);
  const float distance = std::clamp(
      glm::length(*pivot - selection.transform->position),
      k_default_focus_distance,
      k_max_focus_distance
  );
  const glm::vec3 next_position = *pivot - forward * distance;

  commit_camera_state(
      scene,
      selection,
      controller,
      next_position,
      forward,
      camera_up(selection)
  );

  if (controller != nullptr) {
    controller->orbit_distance = distance;
  }
}

template <typename ModalState>
std::string modal_constraint_label(const ModalState &state) {
  using ConstraintKind = typename std::remove_cvref_t<ModalState>::ConstraintKind;
  if (state.mode == EditorGizmoMode::Rotate) {
    if (state.constraint == ConstraintKind::None) {
      return "Screen Rotate";
    }

    const std::string axis = axis_label(state.axis);
    return "Axis " + axis + " rotation";
  }

  if (state.mode == EditorGizmoMode::Scale &&
      state.constraint == ConstraintKind::None) {
    return "Uniform Scale";
  }

  if (state.constraint == ConstraintKind::Axis) {
    return "Axis " + axis_label(state.axis) + " only";
  }

  if (state.constraint == ConstraintKind::Plane) {
    return "Plane " + plane_label_from_locked_axis(state.axis) + ", " +
           axis_label(state.axis) + " locked";
  }

  return {};
}

template <typename ModalState>
std::optional<std::string> modal_detail_line(
    const ModalState &state,
    const scene::Transform &transform
) {
  using ConstraintKind = typename std::remove_cvref_t<ModalState>::ConstraintKind;
  switch (state.mode) {
    case EditorGizmoMode::Translate: {
      const glm::vec3 delta =
          transform.position - state.origin_transform.position;
      if (state.constraint == ConstraintKind::Axis) {
        return format_delta_component(
            axis_label(state.axis),
            glm::dot(delta, state.axis)
        );
      }

      if (state.constraint == ConstraintKind::Plane) {
        if (state.axis.x > 0.5f) {
          return format_delta_component("Y", delta.y) + " · " +
                 format_delta_component("Z", delta.z);
        }
        if (state.axis.y > 0.5f) {
          return format_delta_component("X", delta.x) + " · " +
                 format_delta_component("Z", delta.z);
        }
        return format_delta_component("X", delta.x) + " · " +
               format_delta_component("Y", delta.y);
      }

      return format_delta_component("X", delta.x) + " · " +
             format_delta_component("Y", delta.y) + " · " +
             format_delta_component("Z", delta.z);
    }

    case EditorGizmoMode::Rotate: {
      const glm::quat delta_rotation = glm::normalize(
          transform.rotation * glm::inverse(state.origin_transform.rotation)
      );
      glm::vec3 delta_axis(
          delta_rotation.x,
          delta_rotation.y,
          delta_rotation.z
      );
      float angle_radians = 2.0f * std::atan2(
          glm::length(delta_axis),
          delta_rotation.w
      );
      if (angle_radians > glm::pi<float>()) {
        angle_radians -= glm::two_pi<float>();
      }

      if (glm::length2(delta_axis) > gizmo::k_epsilon) {
        const glm::vec3 reference_axis = normalized_or_fallback(
            state.plane_normal,
            glm::vec3(0.0f, 1.0f, 0.0f)
        );
        if (glm::dot(glm::normalize(delta_axis), reference_axis) < 0.0f) {
          angle_radians = -std::abs(angle_radians);
        } else {
          angle_radians = std::abs(angle_radians);
        }
      }

      return "Angle " + format_number(glm::degrees(angle_radians), 1, true) +
             "deg";
    }

    case EditorGizmoMode::Scale: {
      const glm::vec3 ratio(
          safe_scale_ratio(transform.scale.x, state.origin_transform.scale.x),
          safe_scale_ratio(transform.scale.y, state.origin_transform.scale.y),
          safe_scale_ratio(transform.scale.z, state.origin_transform.scale.z)
      );

      if (state.constraint == ConstraintKind::Axis) {
        const std::string axis = axis_label(state.axis);
        const float value = state.axis.x > 0.5f
                                ? ratio.x
                                : (state.axis.y > 0.5f ? ratio.y : ratio.z);
        return "Scale " + axis + " " + format_number(value, 2) + "x";
      }

      if (state.constraint == ConstraintKind::Plane) {
        if (state.axis.x > 0.5f) {
          if (std::abs(ratio.y - ratio.z) <= 0.01f) {
            return "Scale YZ " + format_number(ratio.y, 2) + "x";
          }
          return "Scale Y " + format_number(ratio.y, 2) + "x · Z " +
                 format_number(ratio.z, 2) + "x";
        }
        if (state.axis.y > 0.5f) {
          if (std::abs(ratio.x - ratio.z) <= 0.01f) {
            return "Scale XZ " + format_number(ratio.x, 2) + "x";
          }
          return "Scale X " + format_number(ratio.x, 2) + "x · Z " +
                 format_number(ratio.z, 2) + "x";
        }
        if (std::abs(ratio.x - ratio.y) <= 0.01f) {
          return "Scale XY " + format_number(ratio.x, 2) + "x";
        }
        return "Scale X " + format_number(ratio.x, 2) + "x · Y " +
               format_number(ratio.y, 2) + "x";
      }

      if (std::abs(ratio.x - ratio.y) <= 0.01f &&
          std::abs(ratio.x - ratio.z) <= 0.01f) {
        return "Scale " + format_number(ratio.x, 2) + "x";
      }

      return "Scale X " + format_number(ratio.x, 2) + "x · Y " +
             format_number(ratio.y, 2) + "x · Z " +
             format_number(ratio.z, 2) + "x";
    }
  }

  return std::nullopt;
}

template <typename ModalState>
std::vector<std::string> modal_hints(const ModalState &state) {
  using ConstraintKind = typename std::remove_cvref_t<ModalState>::ConstraintKind;

  if (state.mode == EditorGizmoMode::Rotate) {
    return {"X/Y/Z axis", "Ctrl 5deg snap", "Enter/Esc"};
  }

  if (state.constraint == ConstraintKind::None) {
    return {"X/Y/Z axis", "Shift plane", "Ctrl snap", "Enter/Esc"};
  }

  if (state.constraint == ConstraintKind::Axis) {
    return {
        other_axes_hint(state.axis),
        "Shift plane",
        "Ctrl snap",
        "Enter/Esc",
    };
  }

  return {"X/Y/Z axis", "Ctrl snap", "Enter/Esc"};
}

} // namespace

void WorkspaceShellSystem::exit_local_view() {
  if (!m_local_view_state.has_value()) {
    return;
  }

  LocalViewState state = std::move(*m_local_view_state);
  m_local_view_state.reset();

  auto scene_manager = SceneManager::get();
  Scene *active_scene =
      scene_manager != nullptr ? scene_manager->get_active_scene() : nullptr;
  if (state.scene == nullptr || state.scene != active_scene) {
    return;
  }

  auto &world = active_scene->world();
  for (const auto &[entity_id, was_active] : state.renderable_states) {
    if (!world.contains(entity_id)) {
      continue;
    }

    world.set_active(entity_id, was_active);
  }

  if (world.contains(state.focus_entity_id)) {
    world.set_active(state.focus_entity_id, state.focus_entity_was_active);
  }
}

void WorkspaceShellSystem::toggle_local_view(std::optional<EntityID> focus_entity_id) {
  if (m_local_view_state.has_value()) {
    if (!focus_entity_id.has_value() ||
        *focus_entity_id == m_local_view_state->focus_entity_id) {
      exit_local_view();
      editor_viewport_hud_store()->show_transient_message("Local View Disabled");
      return;
    }

    exit_local_view();
  }

  if (!focus_entity_id.has_value()) {
    return;
  }

  auto scene_manager = SceneManager::get();
  Scene *scene =
      scene_manager != nullptr ? scene_manager->get_active_scene() : nullptr;
  if (scene == nullptr || !scene->world().contains(*focus_entity_id)) {
    return;
  }

  auto &world = scene->world();
  LocalViewState state{
      .scene = scene,
      .focus_entity_id = *focus_entity_id,
      .focus_entity_was_active = world.active(*focus_entity_id),
  };

  world.each<rendering::Renderable>([&](EntityID entity_id, rendering::Renderable &) {
    if (entity_id == *focus_entity_id) {
      return;
    }

    const bool was_active = world.active(entity_id);
    state.renderable_states.emplace_back(entity_id, was_active);
    if (was_active) {
      world.set_active(entity_id, false);
    }
  });

  if (!state.focus_entity_was_active) {
    world.set_active(*focus_entity_id, true);
  }

  m_local_view_state = std::move(state);
  editor_viewport_hud_store()->show_transient_message("Local View Enabled");
}

void WorkspaceShellSystem::sync_camera_navigation_preset() {
  const EditorCameraNavigationPreset preset =
      editor_camera_navigation_store()->preset();
  auto window = window_manager()->active_window();
  if (window == nullptr) {
    m_last_camera_navigation_preset = preset;
    return;
  }

  if (preset == EditorCameraNavigationPreset::Orbit &&
      window->cursor_captured()) {
    window->capture_cursor(false);
  }

  if (preset != m_last_camera_navigation_preset) {
    if (preset == EditorCameraNavigationPreset::Free) {
      window->capture_cursor(true);
    }
    editor_viewport_hud_store()->show_transient_message(
        navigation_preset_transient(preset)
    );
    m_camera_navigation_drag_state.reset();
    m_last_camera_navigation_preset = preset;
  }
}

void WorkspaceShellSystem::update_viewport_camera_navigation(double) {
  auto scene_manager = SceneManager::get();
  Scene *scene =
      scene_manager != nullptr ? scene_manager->get_active_scene() : nullptr;
  if (m_local_view_state.has_value() &&
      (scene == nullptr || m_local_view_state->scene != scene ||
       !m_local_view_state->scene->world().contains(
           m_local_view_state->focus_entity_id))) {
    exit_local_view();
  }

  const auto interaction_rect = editor_gizmo_store()->interaction_rect();
  if (scene == nullptr || !interaction_rect.has_value()) {
    m_camera_navigation_drag_state.reset();
    return;
  }

  const auto selection = select_camera(*scene);
  if (!selection.has_value() || selection->transform == nullptr ||
      selection->camera == nullptr) {
    m_camera_navigation_drag_state.reset();
    return;
  }

  if (m_modal_transform_state.has_value()) {
    m_camera_navigation_drag_state.reset();
    return;
  }

  scene::CameraController *controller =
      find_camera_controller(*scene, selection->entity_id);
  auto sync_clip_plane_overrides = [&]() {
    if (editor_camera_navigation_store()->preset() ==
        EditorCameraNavigationPreset::Orbit) {
      update_camera_clip_plane_overrides(*scene, *selection, controller);
    } else {
      clear_camera_clip_plane_overrides(*selection->camera);
    }
    refresh_camera_projection(*selection);
  };
  sync_clip_plane_overrides();

  if (const auto pending_action =
          editor_viewport_navigation_store()->consume_action_request();
      pending_action.has_value()) {
    execute_navigation_action(
        *scene,
        *selection,
        controller,
        *pending_action
    );
    sync_clip_plane_overrides();
  }

  update_inverse_side_shortcut_latch(
      input::KeyCode::KP2,
      control_down(),
      m_kp2_view_shortcut_active,
      m_kp2_view_shortcut_pan
  );
  update_inverse_side_shortcut_latch(
      input::KeyCode::KP4,
      control_down(),
      m_kp4_view_shortcut_active,
      m_kp4_view_shortcut_pan
  );
  update_inverse_side_shortcut_latch(
      input::KeyCode::KP6,
      control_down(),
      m_kp6_view_shortcut_active,
      m_kp6_view_shortcut_pan
  );
  update_inverse_side_shortcut_latch(
      input::KeyCode::KP8,
      control_down(),
      m_kp8_view_shortcut_active,
      m_kp8_view_shortcut_pan
  );
  update_inverse_side_shortcut_latch(
      input::KeyCode::KP1,
      control_down(),
      m_kp1_view_shortcut_active,
      m_kp1_view_shortcut_inverse
  );
  update_inverse_side_shortcut_latch(
      input::KeyCode::KP3,
      control_down(),
      m_kp3_view_shortcut_active,
      m_kp3_view_shortcut_inverse
  );
  update_inverse_side_shortcut_latch(
      input::KeyCode::KP7,
      control_down(),
      m_kp7_view_shortcut_active,
      m_kp7_view_shortcut_inverse
  );

  const auto cursor_position = input::CURSOR_POSITION();
  const glm::vec2 cursor(
      static_cast<float>(cursor_position.x),
      static_cast<float>(cursor_position.y)
  );
  const bool interaction_hovered =
      editor_gizmo_store()->point_in_interaction_region(cursor);
  const bool cursor_captured = input::IS_CURSOR_CAPTURED();

  bool shortcuts_active =
      interaction_hovered || m_camera_navigation_drag_state.has_value() ||
      (editor_camera_navigation_store()->preset() ==
           EditorCameraNavigationPreset::Free &&
       cursor_captured);

  if (shortcuts_active &&
      input::IS_KEY_RELEASED(k_navigation_preset_toggle_key)) {
    editor_camera_navigation_store()->toggle_preset();
    sync_camera_navigation_preset();
    sync_clip_plane_overrides();
    shortcuts_active =
        interaction_hovered || m_camera_navigation_drag_state.has_value() ||
        (editor_camera_navigation_store()->preset() ==
             EditorCameraNavigationPreset::Free &&
         input::IS_CURSOR_CAPTURED());
  }

  if (shortcuts_active &&
      input::IS_KEY_RELEASED(input::KeyCode::KP5)) {
    execute_navigation_action(
        *scene,
        *selection,
        controller,
        EditorViewportNavigationAction::ToggleProjection
    );
    sync_clip_plane_overrides();
  }

  if (shortcuts_active && input::IS_KEY_RELEASED(input::KeyCode::KP2)) {
    if (m_kp2_view_shortcut_pan) {
      pan_camera_step(
          *scene,
          *selection,
          controller,
          *interaction_rect,
          resolve_navigation_pivot(*scene, *selection, controller),
          glm::vec2(0.0f, 1.0f)
      );
      editor_viewport_hud_store()->show_transient_message("Pan Down");
    }
    clear_inverse_side_shortcut_latch(
        m_kp2_view_shortcut_active,
        m_kp2_view_shortcut_pan
    );
  } else if (shortcuts_active && input::IS_KEY_RELEASED(input::KeyCode::KP4)) {
    if (m_kp4_view_shortcut_pan) {
      pan_camera_step(
          *scene,
          *selection,
          controller,
          *interaction_rect,
          resolve_navigation_pivot(*scene, *selection, controller),
          glm::vec2(-1.0f, 0.0f)
      );
      editor_viewport_hud_store()->show_transient_message("Pan Left");
    }
    clear_inverse_side_shortcut_latch(
        m_kp4_view_shortcut_active,
        m_kp4_view_shortcut_pan
    );
  } else if (shortcuts_active && input::IS_KEY_RELEASED(input::KeyCode::KP6)) {
    if (m_kp6_view_shortcut_pan) {
      pan_camera_step(
          *scene,
          *selection,
          controller,
          *interaction_rect,
          resolve_navigation_pivot(*scene, *selection, controller),
          glm::vec2(1.0f, 0.0f)
      );
      editor_viewport_hud_store()->show_transient_message("Pan Right");
    }
    clear_inverse_side_shortcut_latch(
        m_kp6_view_shortcut_active,
        m_kp6_view_shortcut_pan
    );
  } else if (shortcuts_active && input::IS_KEY_RELEASED(input::KeyCode::KP8)) {
    if (m_kp8_view_shortcut_pan) {
      pan_camera_step(
          *scene,
          *selection,
          controller,
          *interaction_rect,
          resolve_navigation_pivot(*scene, *selection, controller),
          glm::vec2(0.0f, -1.0f)
      );
      editor_viewport_hud_store()->show_transient_message("Pan Up");
    }
    clear_inverse_side_shortcut_latch(
        m_kp8_view_shortcut_active,
        m_kp8_view_shortcut_pan
    );
  } else if (shortcuts_active && input::IS_KEY_RELEASED(input::KeyCode::KP1)) {
    execute_navigation_action(
        *scene,
        *selection,
        controller,
        m_kp1_view_shortcut_inverse
            ? EditorViewportNavigationAction::Back
            : EditorViewportNavigationAction::Front
    );
    sync_clip_plane_overrides();
    clear_inverse_side_shortcut_latch(
        m_kp1_view_shortcut_active,
        m_kp1_view_shortcut_inverse
    );
  } else if (shortcuts_active && input::IS_KEY_RELEASED(input::KeyCode::KP3)) {
    execute_navigation_action(
        *scene,
        *selection,
        controller,
        m_kp3_view_shortcut_inverse
            ? EditorViewportNavigationAction::Left
            : EditorViewportNavigationAction::Right
    );
    sync_clip_plane_overrides();
    clear_inverse_side_shortcut_latch(
        m_kp3_view_shortcut_active,
        m_kp3_view_shortcut_inverse
    );
  } else if (shortcuts_active && input::IS_KEY_RELEASED(input::KeyCode::KP7)) {
    execute_navigation_action(
        *scene,
        *selection,
        controller,
        m_kp7_view_shortcut_inverse
            ? EditorViewportNavigationAction::Bottom
            : EditorViewportNavigationAction::Top
    );
    sync_clip_plane_overrides();
    clear_inverse_side_shortcut_latch(
        m_kp7_view_shortcut_active,
        m_kp7_view_shortcut_inverse
    );
  }

  if (shortcuts_active && input::IS_KEY_RELEASED(input::KeyCode::F)) {
    focus_selected_entity(*scene, *selection, controller);
  }

  if (shortcuts_active &&
      input::IS_KEY_RELEASED(input::KeyCode::KPDecimal)) {
    toggle_local_view(editor_selection_store()->selected_entity());
  }

  if (editor_camera_navigation_store()->preset() !=
      EditorCameraNavigationPreset::Orbit) {
    sync_clip_plane_overrides();
    m_camera_navigation_drag_state.reset();
    return;
  }

  const auto wheel_delta = input::MOUSE_WHEEL_DELTA();
  if (interaction_hovered && std::abs(wheel_delta.y) > gizmo::k_epsilon) {
    zoom_camera(
        *scene,
        *selection,
        controller,
        resolve_navigation_pivot(*scene, *selection, controller),
        static_cast<float>(wheel_delta.y)
    );
  }

  if (input::IS_MOUSE_BUTTON_PRESSED(input::MouseButton::Middle) &&
      interaction_hovered) {
    m_camera_navigation_drag_state = CameraNavigationDragState{
        .pivot = resolve_navigation_pivot(*scene, *selection, controller),
    };
  }

  if (!input::IS_MOUSE_BUTTON_DOWN(input::MouseButton::Middle)) {
    m_camera_navigation_drag_state.reset();
    return;
  }

  if (!m_camera_navigation_drag_state.has_value()) {
    return;
  }

  const auto mouse_delta = input::MOUSE_DELTA();
  const glm::vec2 delta(
      static_cast<float>(mouse_delta.x),
      static_cast<float>(mouse_delta.y)
  );
  if (glm::dot(delta, delta) <= gizmo::k_epsilon) {
    return;
  }

  if (shift_down()) {
    pan_camera(
        *scene,
        *selection,
        controller,
        *interaction_rect,
        m_camera_navigation_drag_state->pivot,
        delta
    );
    return;
  }

  orbit_camera(
      *scene,
      *selection,
      controller,
      m_camera_navigation_drag_state->pivot,
      delta
  );
}

void WorkspaceShellSystem::sync_viewport_hud() {
  EditorViewportHudSnapshot snapshot;

  auto scene_manager = SceneManager::get();
  Scene *scene =
      scene_manager != nullptr ? scene_manager->get_active_scene() : nullptr;
  const auto selected_entity = editor_selection_store()->selected_entity();
  const bool modal_active = m_modal_transform_state.has_value();
  const EditorGizmoMode mode = modal_active ? m_modal_transform_state->mode
                                            : editor_gizmo_store()->mode();

  snapshot.title = gizmo_mode_title(mode, modal_active);
  snapshot.target = selected_target_label(scene, selected_entity);

  if (!modal_active) {
    snapshot.chips.push_back(EditorViewportHudChip{
        .label = navigation_preset_label(editor_camera_navigation_store()->preset()),
        .tone = EditorViewportHudTone::Accent,
    });
  }

  if (scene != nullptr) {
    const auto camera_selection = select_camera(*scene);
    if (camera_selection.has_value() &&
        camera_selection->transform != nullptr &&
        camera_selection->camera != nullptr) {
      snapshot.chips.push_back(EditorViewportHudChip{
          .label = projection_chip_label(*camera_selection),
          .tone = EditorViewportHudTone::Neutral,
      });
    }
  }

  if (m_local_view_state.has_value()) {
    snapshot.chips.push_back(EditorViewportHudChip{
        .label = "Local View",
        .tone = EditorViewportHudTone::Success,
    });
  }

  if (modal_active) {
    const auto &modal_state = *m_modal_transform_state;
    if (const std::string constraint = modal_constraint_label(modal_state);
        !constraint.empty()) {
      snapshot.chips.insert(
          snapshot.chips.begin(),
          EditorViewportHudChip{
              .label = constraint,
              .tone = EditorViewportHudTone::Accent,
          }
      );
    }

    if (control_down()) {
      snapshot.chips.push_back(EditorViewportHudChip{
          .label = snap_chip_label(modal_state.mode),
          .tone = EditorViewportHudTone::Accent,
      });
    }

    if (scene != nullptr && scene->world().contains(modal_state.entity_id) &&
        scene->world().has<scene::Transform>(modal_state.entity_id)) {
      if (const auto *transform =
              scene->world().get<scene::Transform>(modal_state.entity_id);
          transform != nullptr) {
        snapshot.detail_line = modal_detail_line(modal_state, *transform);
      }
    }

    snapshot.hints = modal_hints(modal_state);
  }

  editor_viewport_hud_store()->set_snapshot(std::move(snapshot));
}

} // namespace astralix::editor
