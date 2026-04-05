#include "scene-system.hpp"
#include "console.hpp"
#include "log.hpp"
#include "managers/scene-manager.hpp"
#include "trace.hpp"
#include "managers/window-manager.hpp"
#include "systems/camera-system/camera-controller-system.hpp"
#include "systems/render-resource-expansion.hpp"
#include "systems/transform-system/transform-system.hpp"

namespace astralix {

void SceneSystem::start() {
  (void)SceneManager::get()->get_active_scene();
}

void SceneSystem::fixed_update(double fixed_dt) {

};

void SceneSystem::pre_update(double dt) {

};

void SceneSystem::update(double dt) {
  ASTRA_PROFILE_N("SceneSystem::update");
  (void)SceneManager::get()->flush_pending_active_scene_state();

  auto scene = SceneManager::get()->get_active_scene();

  if (scene == nullptr) {
    return;
  }

  scene->update();

  auto &world = scene->world();
  if (world.empty()) {
    return;
  }

  {
    ASTRA_PROFILE_N("SceneSystem::expand_render_resources");
    rendering::expand_render_resource_requests(world);
  }

  auto window = window_manager()->active_window();
  const bool console_captures_input = ConsoleManager::get().captures_input();
  const bool camera_input_enabled =
      scene_camera_input_enabled(
          console_captures_input,
          window != nullptr && window->cursor_captured()
      );
  const float aspect_ratio = (window != nullptr && window->height() != 0.0)
                                 ? static_cast<float>(window->width()) /
                                       static_cast<float>(window->height())
                                 : 1.0f;

  if (world.count<scene::Transform, rendering::Camera, scene::CameraController>() > 0u) {
    using namespace input;

    const auto mouse_delta =
        camera_input_enabled
            ? MOUSE_DELTA()
            : input::Mouse::Position{.x = 0.0, .y = 0.0};

    ASTRA_PROFILE_N("SceneSystem::camera_controllers");
    scene::update_camera_controllers(
        world, scene::CameraControllerInput{
                   .forward = camera_input_enabled && IS_KEY_DOWN(KeyCode::W),
                   .backward = camera_input_enabled && IS_KEY_DOWN(KeyCode::S),
                   .left = camera_input_enabled && IS_KEY_DOWN(KeyCode::A),
                   .right = camera_input_enabled && IS_KEY_DOWN(KeyCode::D),
                   .up = camera_input_enabled && IS_KEY_DOWN(KeyCode::Space),
                   .down = camera_input_enabled && IS_KEY_DOWN(KeyCode::LeftControl),
                   .mouse_delta = glm::vec2(static_cast<float>(mouse_delta.x), static_cast<float>(mouse_delta.y)),
                   .dt = static_cast<float>(dt),
                   .aspect_ratio = aspect_ratio,
               }
    );
  }

  {
    ASTRA_PROFILE_N("SceneSystem::update_transforms");
    scene::update_transforms(world);
  }

  {
    ASTRA_PROFILE_N("SceneSystem::recalculate_cameras");
    world.each<scene::Transform, rendering::Camera>(
      [&](EntityID, scene::Transform &transform, rendering::Camera &camera) {
        scene::recalculate_camera_view_matrix(camera, transform, aspect_ratio);

        if (camera.orthographic) {
          scene::recalculate_camera_orthographic_matrix(camera, transform, aspect_ratio);
        } else {
          scene::recalculate_camera_projection_matrix(camera, transform, aspect_ratio);
        }
      }
  );
  }
};

} // namespace astralix
