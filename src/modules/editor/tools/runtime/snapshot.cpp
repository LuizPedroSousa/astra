#include "tools/runtime/runtime-panel-controller.hpp"

#include "components/light.hpp"
#include "components/mesh.hpp"
#include "components/rigidbody.hpp"
#include "components/tags.hpp"
#include "components/ui.hpp"
#include "managers/resource-manager.hpp"
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
  snapshot.shadow_caster_count = scene_world.count<rendering::ShadowCaster>();

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

  scene_world.each<rendering::MeshSet>(
      [&](EntityID, const rendering::MeshSet &mesh_set) {
        for (const auto &mesh : mesh_set.meshes) {
          snapshot.vertex_count += mesh.vertices.size();
          snapshot.triangle_count += mesh.indices.size() / 3u;
        }
      }
  );

  auto resource = resource_manager();
  if (resource != nullptr) {
    snapshot.texture_count = resource->texture_count();
    snapshot.shader_count = resource->shader_count();
    snapshot.material_count = resource->material_count();
    snapshot.model_count = resource->model_count();
  }

  return snapshot;
}

} // namespace astralix::editor
