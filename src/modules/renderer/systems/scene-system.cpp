#include "console.hpp"
#include "scene-system.hpp"
#include "systems/render-resource-expansion.hpp"
#include "systems/camera-system/camera-controller-system.hpp"
#include "systems/transform-system/transform-system.hpp"
#include "managers/scene-manager.hpp"
#include "managers/window-manager.hpp"

namespace astralix {

void SceneSystem::start() {
  auto scene = SceneManager::get()->get_active_scene();

  if (scene != nullptr) {
    if (!scene->load() || scene->world().empty()) {
      scene->start();
    }
  }
}

void SceneSystem::fixed_update(double fixed_dt) {

};

void SceneSystem::pre_update(double dt) {

};

void SceneSystem::update(double dt) {
  auto scene = SceneManager::get()->get_active_scene();

  if (scene == nullptr) {
    return;
  }

  scene->update();

  auto &world = scene->world();
  if (world.empty()) {
    return;
  }

  rendering::expand_render_resource_requests(world);

  auto window = window_manager()->active_window();
  const bool console_captures_input = ConsoleManager::get().captures_input();
  const float aspect_ratio = (window != nullptr && window->height() != 0.0)
                                 ? static_cast<float>(window->width()) /
                                       static_cast<float>(window->height())
                                 : 1.0f;

  if (world.count<scene::Transform, rendering::Camera,
                  scene::CameraController>() > 0u) {
    using namespace input;

    const auto mouse_delta =
        (window != nullptr && window->cursor_captured() &&
         !console_captures_input)
            ? MOUSE_DELTA()
            : input::Mouse::Position{.x = 0.0, .y = 0.0};

    scene::update_camera_controllers(
        world, scene::CameraControllerInput{
                   .forward = !console_captures_input && IS_KEY_DOWN(KeyCode::W),
                   .backward = !console_captures_input && IS_KEY_DOWN(KeyCode::S),
                   .left = !console_captures_input && IS_KEY_DOWN(KeyCode::A),
                   .right = !console_captures_input && IS_KEY_DOWN(KeyCode::D),
                   .up = !console_captures_input && IS_KEY_DOWN(KeyCode::Space),
                   .down = !console_captures_input &&
                           IS_KEY_DOWN(KeyCode::LeftControl),
                   .mouse_delta = glm::vec2(static_cast<float>(mouse_delta.x),
                                            static_cast<float>(mouse_delta.y)),
                   .dt = static_cast<float>(dt),
                   .aspect_ratio = aspect_ratio,
               });
  }

  scene::update_transforms(world);

  world.each<scene::Transform, rendering::Camera>(
      [&](EntityID, scene::Transform &transform,
          rendering::Camera &camera) {
        scene::recalculate_camera_view_matrix(camera, transform,
                                                  aspect_ratio);

        if (camera.orthographic) {
          scene::recalculate_camera_orthographic_matrix(camera, transform,
                                                            aspect_ratio);
        } else {
          scene::recalculate_camera_projection_matrix(camera, transform,
                                                          aspect_ratio);
        }
      });
};

} // namespace astralix
