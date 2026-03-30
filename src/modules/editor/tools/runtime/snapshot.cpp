#include "tools/runtime/runtime-panel-controller.hpp"

#include "components/light.hpp"
#include "components/rigidbody.hpp"
#include "components/tags.hpp"
#include "components/ui.hpp"
#include "managers/scene-manager.hpp"

namespace astralix::editor {

RuntimePanelController::RuntimeSnapshot
RuntimePanelController::collect_snapshot() const {
  RuntimeSnapshot snapshot;

  auto scene_manager = SceneManager::get();
  Scene *scene =
      scene_manager != nullptr ? scene_manager->get_active_scene() : nullptr;
  if (scene == nullptr) {
    return snapshot;
  }

  snapshot.has_scene = true;
  snapshot.scene_name = scene->get_name();

  const auto &scene_world = scene->world();
  snapshot.entity_count = scene_world.count<scene::SceneEntity>();
  snapshot.renderable_count = scene_world.count<rendering::Renderable>();
  snapshot.light_count = scene_world.count<rendering::Light>();
  snapshot.camera_count = scene_world.count<rendering::MainCamera>();
  snapshot.ui_root_count = scene_world.count<rendering::UIRoot>();

  scene_world.each<physics::RigidBody>(
      [&](EntityID, const physics::RigidBody &body) {
        snapshot.rigid_body_count++;
        if (body.mode == physics::RigidBodyMode::Static) {
          snapshot.static_body_count++;
        } else {
          snapshot.dynamic_body_count++;
        }
      }
  );

  return snapshot;
}

} // namespace astralix::editor
