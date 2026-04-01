#include "tools/scene-hierachy/scene-hierarchy-panel-controller.hpp"

#include "components/light.hpp"
#include "components/tags.hpp"
#include "components/ui.hpp"
#include "managers/scene-manager.hpp"
#include "tools/scene-hierachy/helpers.hpp"

#include <algorithm>
#include <utility>

namespace astralix::editor {

SceneHierarchyPanelController::Snapshot
SceneHierarchyPanelController::collect_snapshot() const {
  Snapshot snapshot;

  auto scene_manager = SceneManager::get();
  Scene *scene =
      scene_manager != nullptr ? scene_manager->get_active_scene() : nullptr;
  if (scene == nullptr) {
    return snapshot;
  }

  snapshot.has_scene = true;
  snapshot.scene_name = scene->get_name();

  const auto &world = scene->world();
  world.each<>([&](EntityID entity_id) {
    EntityEntry entry;
    entry.id = entity_id;
    entry.name = std::string(world.name(entity_id));
    entry.active = world.active(entity_id);
    entry.scene_backed = world.has<scene::SceneEntity>(entity_id);
    entry.scope_bucket = scene_hierarchy_panel::scope_bucket(entry.scene_backed);
    entry.scope_label = entry.scene_backed ? "scene-backed" : "world-only";

    const bool has_camera = world.has<rendering::MainCamera>(entity_id);
    const bool has_light = world.has<rendering::Light>(entity_id);
    const bool has_renderable = world.has<rendering::Renderable>(entity_id);
    const bool has_ui_root = world.has<rendering::UIRoot>(entity_id);

    entry.type_bucket = scene_hierarchy_panel::type_bucket(
        has_camera,
        has_light,
        has_renderable,
        has_ui_root
    );

    if (const auto *light = world.get<rendering::Light>(entity_id);
        light != nullptr) {
      entry.kind_label = scene_hierarchy_panel::light_type_label(light->type);
    } else if (has_ui_root) {
      entry.kind_label = "UI Root";
    } else if (has_camera) {
      entry.kind_label = "Main Camera";
    } else if (has_renderable) {
      entry.kind_label =
          entry.scene_backed ? "Renderable Entity" : "Renderable";
    } else if (entry.scene_backed) {
      entry.kind_label = "Scene Entity";
    } else {
      entry.kind_label = "World Entity";
    }

    if (entry.name.empty()) {
      entry.name = "Unnamed Entity";
    }

    const std::string entity_id_text = static_cast<std::string>(entity_id);
    entry.search_blob =
        entry.name + "\n" +
        entry.kind_label + "\n" +
        entry.scope_label + "\n" +
        scene_hierarchy_panel::type_bucket_label(entry.type_bucket) + "\n" +
        entity_id_text + "\n#" + entity_id_text + "\n" +
        (entry.active ? "active" : "inactive");

    snapshot.entities.push_back(std::move(entry));
  });

  std::sort(
      snapshot.entities.begin(),
      snapshot.entities.end(),
      [](const EntityEntry &lhs, const EntityEntry &rhs) {
        if (lhs.scope_bucket != rhs.scope_bucket) {
          return static_cast<uint8_t>(lhs.scope_bucket) <
                 static_cast<uint8_t>(rhs.scope_bucket);
        }

        if (lhs.type_bucket != rhs.type_bucket) {
          return static_cast<uint8_t>(lhs.type_bucket) <
                 static_cast<uint8_t>(rhs.type_bucket);
        }

        const std::string lhs_name =
            scene_hierarchy_panel::lowercase_ascii(lhs.name);
        const std::string rhs_name =
            scene_hierarchy_panel::lowercase_ascii(rhs.name);
        if (lhs_name != rhs_name) {
          return lhs_name < rhs_name;
        }

        return static_cast<uint64_t>(lhs.id) < static_cast<uint64_t>(rhs.id);
      }
  );

  return snapshot;
}

} // namespace astralix::editor
