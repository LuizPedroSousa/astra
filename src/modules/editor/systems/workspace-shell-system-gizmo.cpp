#include "systems/workspace-shell-system-internal.hpp"

#include "console.hpp"
#include "entities/serializers/scene-snapshot.hpp"
#include "systems/ui-system/ui-system.hpp"

namespace astralix::editor {
using namespace workspace_shell_detail;

namespace {

constexpr float k_translate_snap_increment = 1.0f;
constexpr float k_scale_snap_increment = 0.1f;
constexpr float k_modal_scale_factor_per_pixel = 0.01f;
constexpr float k_rotation_snap_degrees = 5.0f;
constexpr size_t k_editor_undo_limit = 128u;

bool gizmo_shortcut_modifiers_active() {
  return input::IS_KEY_DOWN(input::KeyCode::LeftShift) ||
         input::IS_KEY_DOWN(input::KeyCode::RightShift) ||
         input::IS_KEY_DOWN(input::KeyCode::LeftControl) ||
         input::IS_KEY_DOWN(input::KeyCode::RightControl) ||
         input::IS_KEY_DOWN(input::KeyCode::LeftAlt) ||
         input::IS_KEY_DOWN(input::KeyCode::RightAlt) ||
         input::IS_KEY_DOWN(input::KeyCode::LeftSuper) ||
         input::IS_KEY_DOWN(input::KeyCode::RightSuper);
}

bool shift_down() {
  return input::IS_KEY_DOWN(input::KeyCode::LeftShift) ||
         input::IS_KEY_DOWN(input::KeyCode::RightShift);
}

bool control_down() {
  return input::IS_KEY_DOWN(input::KeyCode::LeftControl) ||
         input::IS_KEY_DOWN(input::KeyCode::RightControl);
}

bool alt_down() {
  return input::IS_KEY_DOWN(input::KeyCode::LeftAlt) ||
         input::IS_KEY_DOWN(input::KeyCode::RightAlt);
}

bool super_down() {
  return input::IS_KEY_DOWN(input::KeyCode::LeftSuper) ||
         input::IS_KEY_DOWN(input::KeyCode::RightSuper);
}

void update_shortcut_modifier_latch(
    input::KeyCode key,
    auto &latch
) {
  if (input::IS_KEY_DOWN(key)) {
    if (!latch.active) {
      latch.active = true;
      latch.shift = shift_down();
      latch.control = control_down();
      latch.alt = alt_down();
      latch.super = super_down();
    }
    return;
  }

  if (!input::IS_KEY_RELEASED(key)) {
    latch = {};
  }
}

bool undo_shortcut_requested(
    const auto &latch
) {
  return latch.active &&
         latch.control &&
         !latch.shift &&
         !latch.alt &&
         !latch.super &&
         input::IS_KEY_RELEASED(input::KeyCode::Z);
}

bool duplicate_shortcut_requested(
    const auto &latch
) {
  return latch.active &&
         latch.shift &&
         !latch.control &&
         !latch.alt &&
         !latch.super &&
         input::IS_KEY_RELEASED(input::KeyCode::D);
}

bool add_entity_shortcut_requested(
    const auto &latch
) {
  return latch.active &&
         latch.shift &&
         !latch.control &&
         !latch.alt &&
         !latch.super &&
         input::IS_KEY_RELEASED(input::KeyCode::A);
}

bool visibility_shortcut_requested(
    const auto &latch
) {
  return latch.active &&
         !latch.control &&
         !latch.super &&
         input::IS_KEY_RELEASED(input::KeyCode::H);
}

bool confirm_modal_transform_requested() {
  return input::IS_MOUSE_BUTTON_PRESSED(input::MouseButton::Left) ||
         input::IS_KEY_RELEASED(input::KeyCode::Enter) ||
         input::IS_KEY_RELEASED(input::KeyCode::KPEnter);
}

bool cancel_modal_transform_requested() {
  return input::IS_MOUSE_BUTTON_PRESSED(input::MouseButton::Right) ||
         input::IS_KEY_RELEASED(input::KeyCode::Escape);
}

std::string fallback_entity_label(EntityID entity_id) {
  return "Entity " + std::to_string(static_cast<uint64_t>(entity_id));
}

std::string scene_entity_label(Scene &scene, EntityID entity_id) {
  if (!scene.world().contains(entity_id)) {
    return fallback_entity_label(entity_id);
  }

  const std::string name = std::string(scene.world().name(entity_id));
  return name.empty() ? fallback_entity_label(entity_id) : name;
}

std::string duplicate_entity_name(Scene &scene, std::string_view source_name) {
  const std::string base_name =
      source_name.empty() ? std::string("Entity") : std::string(source_name);
  const std::string copy_base = base_name + " Copy";

  std::unordered_set<std::string> names;
  scene.world().each<scene::SceneEntity>([&](EntityID entity_id, const scene::SceneEntity &) {
    names.insert(std::string(scene.world().name(entity_id)));
  });

  if (!names.contains(copy_base)) {
    return copy_base;
  }

  for (size_t index = 2u;; ++index) {
    const std::string candidate =
        copy_base + " " + std::to_string(index);
    if (!names.contains(candidate)) {
      return candidate;
    }
  }
}

bool should_duplicate_component(std::string_view component_name) {
  return component_name != "DerivedEntity" &&
         component_name != "MetaEntityOwner" &&
         component_name != "GeneratorSpec" &&
         component_name != "MainCamera";
}

std::optional<EntityID> duplicate_scene_entity(Scene &scene, EntityID entity_id) {
  if (!scene.world().contains(entity_id)) {
    return std::nullopt;
  }

  auto source = scene.world().entity(entity_id);
  auto components = serialization::collect_entity_component_snapshots(source);
  components.erase(
      std::remove_if(
          components.begin(),
          components.end(),
          [](const serialization::ComponentSnapshot &component) {
            return !should_duplicate_component(component.name);
          }
      ),
      components.end()
  );

  auto duplicate = scene.spawn_entity(
      duplicate_entity_name(scene, source.name()), source.active()
  );
  for (const auto &component : components) {
    serialization::apply_component_snapshot(duplicate, component);
  }

  if (auto *transform = duplicate.get<scene::Transform>(); transform != nullptr) {
    transform->dirty = true;
  }

  scene.world().touch();
  return duplicate.id();
}

float snap_scalar(float value, float increment) {
  if (increment <= gizmo::k_epsilon) {
    return value;
  }

  return std::round(value / increment) * increment;
}

glm::vec3 camera_view_direction(
    const gizmo::CameraFrame &camera_frame,
    const glm::vec3 &pivot
) {
  if (camera_frame.orthographic) {
    return glm::length2(camera_frame.forward) > gizmo::k_epsilon
               ? glm::normalize(camera_frame.forward)
               : glm::vec3(0.0f, 0.0f, -1.0f);
  }

  const glm::vec3 direction = pivot - camera_frame.position;
  if (glm::length2(direction) <= gizmo::k_epsilon) {
    return glm::length2(camera_frame.forward) > gizmo::k_epsilon
               ? glm::normalize(camera_frame.forward)
               : glm::vec3(0.0f, 0.0f, -1.0f);
  }

  return glm::normalize(direction);
}

std::optional<glm::vec3> released_constraint_axis() {
  if (input::IS_KEY_RELEASED(input::KeyCode::X)) {
    return glm::vec3(1.0f, 0.0f, 0.0f);
  }
  if (input::IS_KEY_RELEASED(input::KeyCode::Y)) {
    return glm::vec3(0.0f, 1.0f, 0.0f);
  }
  if (input::IS_KEY_RELEASED(input::KeyCode::Z)) {
    return glm::vec3(0.0f, 0.0f, 1.0f);
  }

  return std::nullopt;
}

EditorGizmoHandle handle_for_mode_axis(
    EditorGizmoMode mode,
    const glm::vec3 &axis
) {
  if (axis.x > 0.5f) {
    switch (mode) {
      case EditorGizmoMode::Rotate:
        return EditorGizmoHandle::RotateX;
      case EditorGizmoMode::Scale:
        return EditorGizmoHandle::ScaleX;
      case EditorGizmoMode::Translate:
      default:
        return EditorGizmoHandle::TranslateX;
    }
  }

  if (axis.y > 0.5f) {
    switch (mode) {
      case EditorGizmoMode::Rotate:
        return EditorGizmoHandle::RotateY;
      case EditorGizmoMode::Scale:
        return EditorGizmoHandle::ScaleY;
      case EditorGizmoMode::Translate:
      default:
        return EditorGizmoHandle::TranslateY;
    }
  }

  switch (mode) {
    case EditorGizmoMode::Rotate:
      return EditorGizmoHandle::RotateZ;
    case EditorGizmoMode::Scale:
      return EditorGizmoHandle::ScaleZ;
    case EditorGizmoMode::Translate:
    default:
      return EditorGizmoHandle::TranslateZ;
  }
}

void apply_transform_snapshot(
    Scene &scene,
    scene::Transform &transform,
    const scene::Transform &snapshot
) {
  transform = snapshot;
  transform.dirty = true;
  scene.world().touch();
}

glm::vec3 snap_translation_in_plane(
    const glm::vec3 &delta,
    const glm::vec3 &plane_normal
) {
  const auto [tangent, bitangent] =
      gizmo::orthonormal_basis(glm::normalize(plane_normal));
  const float tangent_delta = snap_scalar(
      glm::dot(delta, tangent), k_translate_snap_increment
  );
  const float bitangent_delta = snap_scalar(
      glm::dot(delta, bitangent), k_translate_snap_increment
  );
  return tangent * tangent_delta + bitangent * bitangent_delta;
}

glm::vec3 snap_translation_on_locked_plane(
    glm::vec3 delta,
    const glm::vec3 &plane_normal
) {
  if (plane_normal.x > 0.5f) {
    delta.x = 0.0f;
    delta.y = snap_scalar(delta.y, k_translate_snap_increment);
    delta.z = snap_scalar(delta.z, k_translate_snap_increment);
    return delta;
  }

  if (plane_normal.y > 0.5f) {
    delta.x = snap_scalar(delta.x, k_translate_snap_increment);
    delta.y = 0.0f;
    delta.z = snap_scalar(delta.z, k_translate_snap_increment);
    return delta;
  }

  delta.x = snap_scalar(delta.x, k_translate_snap_increment);
  delta.y = snap_scalar(delta.y, k_translate_snap_increment);
  delta.z = 0.0f;
  return delta;
}

float modal_scale_factor(glm::vec2 cursor_delta, bool snap_active) {
  float factor = std::max(
      gizmo::k_min_scale_component,
      1.0f +
          (cursor_delta.x - cursor_delta.y) * k_modal_scale_factor_per_pixel
  );
  if (snap_active) {
    factor = std::max(
        gizmo::k_min_scale_component,
        snap_scalar(factor, k_scale_snap_increment)
    );
  }

  return factor;
}

void apply_uniform_scale(glm::vec3 &scale, float factor) {
  scale.x = std::max(gizmo::k_min_scale_component, scale.x * factor);
  scale.y = std::max(gizmo::k_min_scale_component, scale.y * factor);
  scale.z = std::max(gizmo::k_min_scale_component, scale.z * factor);
}

bool transforms_match(
    const scene::Transform &lhs,
    const scene::Transform &rhs
) {
  const bool same_position =
      glm::length2(lhs.position - rhs.position) <= 0.000001f;
  const bool same_scale = glm::length2(lhs.scale - rhs.scale) <= 0.000001f;
  const bool same_rotation =
      std::abs(glm::dot(lhs.rotation, rhs.rotation)) >= 0.999999f;
  return same_position && same_scale && same_rotation;
}

bool rebase_modal_transform_state(
    auto &state,
    const scene::Transform &transform,
    const gizmo::CameraFrame &camera_frame,
    const ui::UIRect &interaction_rect,
    glm::vec2 cursor
) {
  state.start_transform = transform;
  state.start_cursor = cursor;
  state.pivot = transform.position;
  state.gizmo_scale = gizmo::gizmo_scale_world(
      camera_frame,
      state.pivot,
      interaction_rect.height
  );
  state.visual_handle =
      state.constraint == decltype(state.constraint)::None
          ? std::nullopt
          : std::optional<EditorGizmoHandle>(
                handle_for_mode_axis(state.mode, state.axis)
            );

  switch (state.mode) {
    case EditorGizmoMode::Translate: {
      if (state.constraint == decltype(state.constraint)::Axis) {
        state.plane_normal = gizmo::translation_drag_plane_normal(
            camera_frame,
            state.axis,
            state.pivot
        );
      } else if (state.constraint == decltype(state.constraint)::Plane) {
        state.plane_normal = state.axis;
      } else {
        state.plane_normal = camera_view_direction(camera_frame, state.pivot);
      }

      const auto start_hit = cursor_world_hit(
          camera_frame,
          interaction_rect,
          cursor,
          state.pivot,
          state.plane_normal
      );
      if (!start_hit.has_value()) {
        return false;
      }

      state.start_hit_point = *start_hit;
      return true;
    }

    case EditorGizmoMode::Rotate: {
      state.plane_normal =
          state.constraint == decltype(state.constraint)::None
              ? camera_view_direction(camera_frame, state.pivot)
              : state.axis;
      const auto start_hit = cursor_world_hit(
          camera_frame,
          interaction_rect,
          cursor,
          state.pivot,
          state.plane_normal
      );
      if (!start_hit.has_value()) {
        return false;
      }

      const glm::vec3 ring_vector = *start_hit - state.pivot;
      if (glm::length2(ring_vector) <= gizmo::k_epsilon) {
        return false;
      }

      state.start_hit_point = *start_hit;
      state.start_ring_vector = glm::normalize(ring_vector);
      return true;
    }

    case EditorGizmoMode::Scale:
    default:
      return true;
  }
}

bool update_modal_transform(
    scene::Transform &transform,
    const auto &state,
    const gizmo::CameraFrame &camera_frame,
    const ui::UIRect &interaction_rect,
    glm::vec2 cursor,
    bool snap_active
) {
  switch (state.mode) {
    case EditorGizmoMode::Translate: {
      const auto current_hit = cursor_world_hit(
          camera_frame,
          interaction_rect,
          cursor,
          state.pivot,
          state.plane_normal
      );
      if (!current_hit.has_value()) {
        return false;
      }

      glm::vec3 delta_world = *current_hit - state.start_hit_point;
      if (state.constraint == decltype(state.constraint)::Axis) {
        float axis_delta = glm::dot(delta_world, state.axis);
        if (snap_active) {
          axis_delta = snap_scalar(axis_delta, k_translate_snap_increment);
        }
        transform.position =
            state.start_transform.position + state.axis * axis_delta;
        return true;
      }

      if (state.constraint == decltype(state.constraint)::Plane) {
        if (snap_active) {
          delta_world = snap_translation_on_locked_plane(
              delta_world,
              state.axis
          );
        }
        transform.position = state.start_transform.position + delta_world;
        return true;
      }

      if (snap_active) {
        delta_world =
            snap_translation_in_plane(delta_world, state.plane_normal);
      }
      transform.position = state.start_transform.position + delta_world;
      return true;
    }

    case EditorGizmoMode::Rotate: {
      const auto current_hit = cursor_world_hit(
          camera_frame,
          interaction_rect,
          cursor,
          state.pivot,
          state.plane_normal
      );
      if (!current_hit.has_value()) {
        return false;
      }

      const glm::vec3 ring_vector = *current_hit - state.pivot;
      if (glm::length2(ring_vector) <= gizmo::k_epsilon) {
        return false;
      }

      float angle = gizmo::signed_angle_on_axis(
          state.start_ring_vector,
          ring_vector,
          state.plane_normal
      );
      if (snap_active) {
        angle = snap_scalar(
            angle,
            glm::radians(k_rotation_snap_degrees)
        );
      }

      transform.rotation = glm::normalize(
          glm::angleAxis(angle, glm::normalize(state.plane_normal)) *
          state.start_transform.rotation
      );
      return true;
    }

    case EditorGizmoMode::Scale: {
      const float factor = modal_scale_factor(cursor - state.start_cursor, snap_active);
      glm::vec3 next_scale = state.start_transform.scale;

      if (state.constraint == decltype(state.constraint)::Axis) {
        const float scaled_component = std::max(
            gizmo::k_min_scale_component,
            axis_scale_component(state.start_transform.scale, state.axis) * factor
        );
        set_axis_scale_component(next_scale, state.axis, scaled_component);
        transform.scale = next_scale;
        return true;
      }

      if (state.constraint == decltype(state.constraint)::Plane) {
        if (state.axis.x > 0.5f) {
          next_scale.y = std::max(
              gizmo::k_min_scale_component,
              state.start_transform.scale.y * factor
          );
          next_scale.z = std::max(
              gizmo::k_min_scale_component,
              state.start_transform.scale.z * factor
          );
        } else if (state.axis.y > 0.5f) {
          next_scale.x = std::max(
              gizmo::k_min_scale_component,
              state.start_transform.scale.x * factor
          );
          next_scale.z = std::max(
              gizmo::k_min_scale_component,
              state.start_transform.scale.z * factor
          );
        } else {
          next_scale.x = std::max(
              gizmo::k_min_scale_component,
              state.start_transform.scale.x * factor
          );
          next_scale.y = std::max(
              gizmo::k_min_scale_component,
              state.start_transform.scale.y * factor
          );
        }

        transform.scale = next_scale;
        return true;
      }

      apply_uniform_scale(next_scale, factor);
      transform.scale = next_scale;
      return true;
    }
  }

  return false;
}

} // namespace

void WorkspaceShellSystem::push_transform_undo(
    EntityID entity_id,
    const scene::Transform &before,
    const scene::Transform &after
) {
  if (transforms_match(before, after)) {
    return;
  }

  m_undo_stack.push_back(EditorUndoEntry{
      .kind = EditorUndoEntry::Kind::Transform,
      .transform =
          TransformUndoEntry{
              .entity_id = entity_id,
              .before = before,
          },
  });
  if (m_undo_stack.size() > k_editor_undo_limit) {
    m_undo_stack.erase(m_undo_stack.begin());
  }
}

void WorkspaceShellSystem::push_restore_deleted_entity_undo(
    Scene &scene,
    serialization::EntitySnapshot snapshot
) {
  m_undo_stack.push_back(EditorUndoEntry{
      .kind = EditorUndoEntry::Kind::RestoreDeletedEntity,
      .restore_deleted_entity =
          RestoreDeletedEntityUndoEntry{
              .scene = &scene,
              .snapshot = std::move(snapshot),
          },
  });
  if (m_undo_stack.size() > k_editor_undo_limit) {
    m_undo_stack.erase(m_undo_stack.begin());
  }
}

bool WorkspaceShellSystem::try_undo_last_action() {
  auto scene_manager = SceneManager::get();
  Scene *scene =
      scene_manager != nullptr ? scene_manager->get_active_scene() : nullptr;
  if (scene == nullptr) {
    m_undo_stack.clear();
    return false;
  }

  while (!m_undo_stack.empty()) {
    const EditorUndoEntry entry = m_undo_stack.back();
    m_undo_stack.pop_back();

    switch (entry.kind) {
      case EditorUndoEntry::Kind::Transform: {
        if (!scene->world().contains(entry.transform.entity_id) ||
            !scene->world().has<scene::Transform>(entry.transform.entity_id)) {
          continue;
        }

        auto *transform =
            scene->world().get<scene::Transform>(entry.transform.entity_id);
        if (transform == nullptr) {
          continue;
        }

        apply_transform_snapshot(*scene, *transform, entry.transform.before);
        editor_selection_store()->set_selected_entity(entry.transform.entity_id);
        return true;
      }

      case EditorUndoEntry::Kind::RestoreDeletedEntity: {
        if (entry.restore_deleted_entity.scene != scene) {
          continue;
        }

        const auto &snapshot = entry.restore_deleted_entity.snapshot;
        auto entity =
            scene->world().ensure(snapshot.id, snapshot.name, snapshot.active);
        for (const auto &component : snapshot.components) {
          serialization::apply_component_snapshot(entity, component);
        }

        if (auto *transform = entity.get<scene::Transform>();
            transform != nullptr) {
          transform->dirty = true;
        }

        scene->world().touch();
        editor_selection_store()->set_selected_entity(snapshot.id);
        return true;
      }
    }
  }

  return false;
}

void WorkspaceShellSystem::clear_gizmo_drag_state() {
  m_gizmo_drag_state.reset();
  editor_gizmo_store()->set_active_handle(std::nullopt);
}

std::optional<EntityID> WorkspaceShellSystem::pick_entity_at_cursor(
    glm::vec2 cursor,
    const ui::UIRect &interaction_rect
) {
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

  m_pending_viewport_pick_pixel = glm::ivec2(pixel->x, pixel->y);
  render_system->request_entity_pick(*m_pending_viewport_pick_pixel);
  LOG_DEBUG(
      "[WorkspaceShellSystem] viewport pick requested",
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
      ")"
  );
  return std::nullopt;
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
  const bool modal_active = m_modal_transform_state.has_value();
  if (auto system_manager = SystemManager::get(); system_manager != nullptr) {
    if (auto *render_system = system_manager->get_system<RenderSystem>();
        render_system != nullptr) {
      if (auto result = render_system->consume_latest_entity_pick();
          result.has_value()) {
        if (modal_active) {
          m_pending_viewport_pick_pixel.reset();
        } else if (!m_pending_viewport_pick_pixel.has_value() ||
                   result->pixel == *m_pending_viewport_pick_pixel) {
          editor_selection_store()->set_selected_entity(result->entity_id);
          m_pending_viewport_pick_pixel.reset();
          clear_gizmo_drag_state();
          store->set_hovered_handle(std::nullopt);
          store->set_active_handle(std::nullopt);
        }
      }
    }
  }

  const auto interaction_rect = store->interaction_rect();
  if (!interaction_rect.has_value()) {
    m_modal_transform_state.reset();
    clear_gizmo_drag_state();
    store->clear_hover_and_active_handles();
    return;
  }

  auto scene_manager = SceneManager::get();
  Scene *scene =
      scene_manager != nullptr ? scene_manager->get_active_scene() : nullptr;
  std::optional<SelectedCamera> camera_selection;
  std::optional<gizmo::CameraFrame> camera_frame;
  if (scene != nullptr) {
    camera_selection = select_camera(*scene);
    if (camera_selection.has_value() && camera_selection->transform != nullptr &&
        camera_selection->camera != nullptr) {
      camera_frame = gizmo::make_camera_frame(
          *camera_selection->transform,
          *camera_selection->camera
      );
    }
  }

  const auto cursor_position = input::CURSOR_POSITION();
  const glm::vec2 cursor(
      static_cast<float>(cursor_position.x),
      static_cast<float>(cursor_position.y)
  );
  const bool interaction_hovered = store->point_in_interaction_region(cursor);
  bool keyboard_shortcuts_blocked = ConsoleManager::get().captures_input();
  if (auto system_manager = SystemManager::get(); system_manager != nullptr) {
    if (auto *ui_system = system_manager->get_system<UISystem>();
        ui_system != nullptr) {
      keyboard_shortcuts_blocked =
          keyboard_shortcuts_blocked ||
          ui_system->keyboard_focus_captures_editor_shortcuts();
    }
  }

  update_shortcut_modifier_latch(
      input::KeyCode::Z,
      m_undo_shortcut_latch
  );
  update_shortcut_modifier_latch(
      input::KeyCode::D,
      m_duplicate_shortcut_latch
  );
  update_shortcut_modifier_latch(
      input::KeyCode::H,
      m_visibility_shortcut_latch
  );
  update_shortcut_modifier_latch(
      input::KeyCode::A,
      m_add_shortcut_latch
  );

  auto restore_hidden_entity_visibility = [this]() {
    if (!m_hidden_entity_visibility_state.has_value()) {
      return false;
    }

    auto hidden_state = std::move(*m_hidden_entity_visibility_state);
    m_hidden_entity_visibility_state.reset();

    bool changed = false;
    if (hidden_state.scene == nullptr) {
      return false;
    }

    for (const auto &[entity_id, was_active] : hidden_state.active_states) {
      if (!hidden_state.scene->world().contains(entity_id)) {
        continue;
      }

      hidden_state.scene->world().set_active(entity_id, was_active);
      changed = true;
    }

    if (changed) {
      hidden_state.scene->world().touch();
    }

    return changed;
  };

  if (m_hidden_entity_visibility_state.has_value() &&
      scene != m_hidden_entity_visibility_state->scene) {
    restore_hidden_entity_visibility();
  }

  if (!m_modal_transform_state.has_value() && !m_gizmo_drag_state.has_value() &&
      !keyboard_shortcuts_blocked &&
      undo_shortcut_requested(m_undo_shortcut_latch)) {
    if (try_undo_last_action()) {
      store->clear_hover_and_active_handles();
    }
    return;
  }

  if (m_modal_transform_state.has_value()) {
    auto &modal_state = *m_modal_transform_state;
    scene::Transform *transform = nullptr;
    if (scene != nullptr && scene->world().contains(modal_state.entity_id)) {
      transform = scene->world().get<scene::Transform>(modal_state.entity_id);
    }

    if (scene == nullptr || transform == nullptr || !camera_frame.has_value()) {
      m_modal_transform_state.reset();
      store->clear_hover_and_active_handles();
      return;
    }

    store->set_hovered_handle(modal_state.visual_handle);
    store->set_active_handle(modal_state.visual_handle);

    if (cancel_modal_transform_requested()) {
      apply_transform_snapshot(*scene, *transform, modal_state.origin_transform);
      if (auto window = window_manager()->active_window();
          window != nullptr && window->cursor_captured()) {
        window->capture_cursor(false);
      }
      m_modal_transform_state.reset();
      store->clear_hover_and_active_handles();
      return;
    }

    if (confirm_modal_transform_requested()) {
      push_transform_undo(
          modal_state.entity_id,
          modal_state.origin_transform,
          *transform
      );
      m_modal_transform_state.reset();
      store->clear_hover_and_active_handles();
      return;
    }

    if (const auto constraint_axis = released_constraint_axis();
        constraint_axis.has_value()) {
      modal_state.axis = *constraint_axis;
      modal_state.constraint =
          shift_down()
              ? ModalTransformState::ConstraintKind::Plane
              : ModalTransformState::ConstraintKind::Axis;

      if (!rebase_modal_transform_state(
              modal_state,
              *transform,
              *camera_frame,
              *interaction_rect,
              cursor
          )) {
        modal_state.constraint = ModalTransformState::ConstraintKind::None;
        modal_state.visual_handle.reset();
        rebase_modal_transform_state(
            modal_state,
            *transform,
            *camera_frame,
            *interaction_rect,
            cursor
        );
      }

      auto constraint_message = [&modal_state]() {
        if (modal_state.mode == EditorGizmoMode::Rotate) {
          const char axis =
              modal_state.axis.x > 0.5f ? 'X'
              : (modal_state.axis.y > 0.5f ? 'Y' : 'Z');
          return std::string("Axis ") + axis + " rotation";
        }

        if (modal_state.constraint ==
            ModalTransformState::ConstraintKind::Axis) {
          const char axis =
              modal_state.axis.x > 0.5f ? 'X'
              : (modal_state.axis.y > 0.5f ? 'Y' : 'Z');
          return std::string("Axis ") + axis + " only";
        }

        const char locked_axis =
            modal_state.axis.x > 0.5f ? 'X'
            : (modal_state.axis.y > 0.5f ? 'Y' : 'Z');
        const char first_plane_axis = locked_axis == 'X' ? 'Y' : 'X';
        const char second_plane_axis = locked_axis == 'Z' ? 'Y' : 'Z';
        return std::string("Plane ") + first_plane_axis + second_plane_axis +
               ", " + locked_axis + " locked";
      }();
      editor_viewport_hud_store()->show_transient_message(
          std::move(constraint_message)
      );
    }

    if (update_modal_transform(
            *transform,
            modal_state,
            *camera_frame,
            *interaction_rect,
            cursor,
            control_down()
        )) {
      transform->dirty = true;
      scene->world().touch();
    }
    return;
  }

  if (input::IS_CURSOR_CAPTURED()) {
    clear_gizmo_drag_state();
    store->clear_hover_and_active_handles();
    return;
  }

  auto selected_entity = editor_selection_store()->selected_entity();
  scene::Transform *transform = nullptr;
  std::optional<glm::vec3> pivot;
  float gizmo_scale = 1.0f;
  bool selection_ready = false;

  if (scene != nullptr && selected_entity.has_value() &&
      scene->world().contains(*selected_entity)) {
    transform = scene->world().get<scene::Transform>(*selected_entity);
    if (transform != nullptr && camera_selection.has_value() &&
        camera_selection->transform != nullptr &&
        camera_selection->camera != nullptr && camera_frame.has_value()) {
      pivot = transform->position;
      gizmo_scale = gizmo::gizmo_scale_world(
          *camera_frame,
          *pivot,
          interaction_rect->height
      );
      selection_ready = true;
    }
  }

  auto begin_modal_transform = [this,
                                &cursor,
                                &camera_frame,
                                &gizmo_scale,
                                &interaction_rect,
                                &pivot,
                                &selection_ready,
                                &selected_entity,
                                &store,
                                &transform](EditorGizmoMode mode) {
    if (!selection_ready || !selected_entity.has_value() ||
        !camera_frame.has_value() || !pivot.has_value() || transform == nullptr) {
      return false;
    }

    store->set_mode(mode);
    clear_gizmo_drag_state();
    m_pending_viewport_pick_pixel.reset();

    ModalTransformState modal_state{
        .mode = mode,
        .entity_id = *selected_entity,
        .origin_transform = *transform,
        .start_transform = *transform,
        .pivot = *pivot,
        .gizmo_scale = gizmo_scale,
    };
    if (!rebase_modal_transform_state(
            modal_state,
            *transform,
            *camera_frame,
            *interaction_rect,
            cursor
        )) {
      return false;
    }

    m_modal_transform_state = std::move(modal_state);
    store->set_hovered_handle(m_modal_transform_state->visual_handle);
    store->set_active_handle(m_modal_transform_state->visual_handle);
    return true;
  };

  if (!m_gizmo_drag_state.has_value() && !keyboard_shortcuts_blocked) {
    auto selection_entity = [&]() -> std::optional<EntityID> {
      if (scene == nullptr || !selected_entity.has_value()) {
        return std::nullopt;
      }
      if (!scene->world().contains(*selected_entity)) {
        editor_selection_store()->clear_selected_entity();
        return std::nullopt;
      }
      return selected_entity;
    }();

    if (duplicate_shortcut_requested(m_duplicate_shortcut_latch)) {
      if (selection_entity.has_value()) {
        if (const auto duplicate_entity_id =
                duplicate_scene_entity(*scene, *selection_entity);
            duplicate_entity_id.has_value()) {
          editor_selection_store()->set_selected_entity(*duplicate_entity_id);
          selected_entity = *duplicate_entity_id;
          editor_viewport_hud_store()->show_transient_message(
              "Duplicate " + scene_entity_label(*scene, *duplicate_entity_id)
          );

          const auto duplicate_transform =
              scene->world().get<scene::Transform>(*duplicate_entity_id);
          if (interaction_hovered && duplicate_transform != nullptr &&
              camera_frame.has_value()) {
            transform = duplicate_transform;
            selection_ready = true;
            pivot = duplicate_transform->position;
            gizmo_scale = gizmo::gizmo_scale_world(
                *camera_frame,
                *pivot,
                interaction_rect->height
            );
            if (begin_modal_transform(EditorGizmoMode::Translate)) {
              return;
            }
          }
        }
      }
      return;
    }

    if (!control_down() && !alt_down() && !super_down() &&
        (input::IS_KEY_RELEASED(input::KeyCode::Delete) ||
         input::IS_KEY_RELEASED(input::KeyCode::X))) {
      if (selection_entity.has_value()) {
        const std::string label = scene_entity_label(*scene, *selection_entity);
        push_restore_deleted_entity_undo(
            *scene,
            serialization::collect_entity_snapshot(
                scene->world().entity(*selection_entity)
            )
        );
        if (m_hidden_entity_visibility_state.has_value()) {
          m_hidden_entity_visibility_state->active_states.erase(*selection_entity);
          if (m_hidden_entity_visibility_state->active_states.empty()) {
            m_hidden_entity_visibility_state.reset();
          }
        }
        scene->world().destroy(*selection_entity);
        scene->world().touch();
        editor_selection_store()->clear_selected_entity();
        clear_gizmo_drag_state();
        store->clear_hover_and_active_handles();
        editor_viewport_hud_store()->show_transient_message("Delete " + label);
      }
      return;
    }

    if (visibility_shortcut_requested(m_visibility_shortcut_latch)) {
      if (m_visibility_shortcut_latch.alt &&
          !m_visibility_shortcut_latch.shift) {
        if (restore_hidden_entity_visibility()) {
          editor_viewport_hud_store()->show_transient_message("Unhide All");
        }
        return;
      }

      if (!selection_entity.has_value()) {
        return;
      }

      if (!m_hidden_entity_visibility_state.has_value() ||
          m_hidden_entity_visibility_state->scene != scene) {
        restore_hidden_entity_visibility();
        m_hidden_entity_visibility_state = HiddenEntityVisibilityState{
            .scene = scene,
        };
      }

      auto &hidden_state = *m_hidden_entity_visibility_state;
      bool changed = false;

      if (m_visibility_shortcut_latch.shift &&
          !m_visibility_shortcut_latch.alt) {
        scene->world().each<scene::SceneEntity>([&](
            EntityID entity_id,
            const scene::SceneEntity &
        ) {
          if (static_cast<uint64_t>(entity_id) ==
              static_cast<uint64_t>(*selection_entity)) {
            return;
          }

          hidden_state.active_states.try_emplace(
              entity_id, scene->world().active(entity_id)
          );
          scene->world().set_active(entity_id, false);
          changed = true;
        });

        if (changed) {
          scene->world().touch();
          editor_viewport_hud_store()->show_transient_message(
              "Hide Unselected"
          );
        }
        return;
      }

      if (!m_visibility_shortcut_latch.alt &&
          !m_visibility_shortcut_latch.shift) {
        hidden_state.active_states.try_emplace(
            *selection_entity, scene->world().active(*selection_entity)
        );
        scene->world().set_active(*selection_entity, false);
        scene->world().touch();
        editor_selection_store()->clear_selected_entity();
        clear_gizmo_drag_state();
        store->clear_hover_and_active_handles();
        editor_viewport_hud_store()->show_transient_message(
            "Hide " + scene_entity_label(*scene, *selection_entity)
        );
      }
      return;
    }

    if (!shift_down() && !control_down() && !alt_down() && !super_down() &&
        input::IS_KEY_RELEASED(input::KeyCode::F2)) {
      if (selection_entity.has_value()) {
        editor_selection_store()->request_panel_focus("inspector");
        workspace_ui_store()->request_inspector_entity_name_focus();
        editor_viewport_hud_store()->show_transient_message("Rename Selected");
      }
      return;
    }

    if (add_entity_shortcut_requested(m_add_shortcut_latch)) {
      if (scene != nullptr) {
        editor_selection_store()->request_panel_focus("scene-hierarchy");
        workspace_ui_store()->request_scene_hierarchy_create_menu();
        editor_viewport_hud_store()->show_transient_message("Add Entity");
      }
      return;
    }
  }

  if (!m_gizmo_drag_state.has_value() && interaction_hovered &&
      !keyboard_shortcuts_blocked && !gizmo_shortcut_modifiers_active()) {

    if (input::IS_KEY_RELEASED(input::KeyCode::G)) {
      if (begin_modal_transform(EditorGizmoMode::Translate)) {
        return;
      }
    } else if (input::IS_KEY_RELEASED(input::KeyCode::R)) {
      if (begin_modal_transform(EditorGizmoMode::Rotate)) {
        return;
      }
    } else if (input::IS_KEY_RELEASED(input::KeyCode::S)) {
      if (begin_modal_transform(EditorGizmoMode::Scale)) {
        return;
      }
    } else if (input::IS_KEY_RELEASED(input::KeyCode::W)) {
      store->set_mode(EditorGizmoMode::Translate);
    } else if (input::IS_KEY_RELEASED(input::KeyCode::E)) {
      store->set_mode(EditorGizmoMode::Rotate);
    } else if (input::IS_KEY_RELEASED(input::KeyCode::T)) {
      store->set_mode(EditorGizmoMode::Scale);
    }
  }

  if (m_gizmo_drag_state.has_value()) {
    if (!selection_ready ||
        !selected_entity.has_value() ||
        static_cast<uint64_t>(m_gizmo_drag_state->entity_id) !=
            static_cast<uint64_t>(*selected_entity)) {
      clear_gizmo_drag_state();
      return;
    }

    if (!input::IS_MOUSE_BUTTON_DOWN(input::MouseButton::Left)) {
      push_transform_undo(
          m_gizmo_drag_state->entity_id,
          m_gizmo_drag_state->start_transform,
          *transform
      );
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

    const bool snap_active = control_down();

    switch (gizmo::mode_for_handle(m_gizmo_drag_state->handle)) {
      case EditorGizmoMode::Translate: {
        float delta = glm::dot(
            *current_hit - m_gizmo_drag_state->start_hit_point,
            m_gizmo_drag_state->axis
        );
        if (snap_active) {
          delta = snap_scalar(delta, k_translate_snap_increment);
        }
        transform->position = m_gizmo_drag_state->start_transform.position +
                              m_gizmo_drag_state->axis * delta;
        break;
      }

      case EditorGizmoMode::Rotate: {
        const glm::vec3 ring_vector = *current_hit - m_gizmo_drag_state->pivot;
        if (glm::length2(ring_vector) <= gizmo::k_epsilon) {
          return;
        }

        float angle = gizmo::signed_angle_on_axis(
            m_gizmo_drag_state->start_ring_vector,
            ring_vector,
            m_gizmo_drag_state->axis
        );
        if (snap_active) {
          angle = snap_scalar(
              angle,
              glm::radians(k_rotation_snap_degrees)
          );
        }
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
        float factor = gizmo::scale_factor_from_axis_delta(
            delta,
            m_gizmo_drag_state->gizmo_scale
        );
        if (snap_active) {
          factor = std::max(
              gizmo::k_min_scale_component,
              snap_scalar(factor, k_scale_snap_increment)
          );
        }
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
    if (picked_entity.has_value() || !m_pending_viewport_pick_pixel.has_value()) {
      LOG_DEBUG(
          "[WorkspaceShellSystem] applying viewport selection",
          picked_entity.has_value() ? static_cast<uint64_t>(*picked_entity)
                                    : 0ull
      );
      editor_selection_store()->set_selected_entity(picked_entity);
    }
    clear_gizmo_drag_state();
    store->set_hovered_handle(std::nullopt);
    return;
  }

  if (!selection_ready || !selected_entity.has_value()) {
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

} // namespace astralix::editor
