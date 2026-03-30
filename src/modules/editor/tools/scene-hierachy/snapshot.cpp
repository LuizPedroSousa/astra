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

    if (const auto *light = world.get<rendering::Light>(entity_id);
        light != nullptr) {
      entry.kind_label = scene_hierarchy_panel::light_type_label(light->type);
    } else if (world.has<rendering::UIRoot>(entity_id)) {
      entry.kind_label = "UI Root";
    } else if (world.has<rendering::MainCamera>(entity_id)) {
      entry.kind_label = "Main Camera";
    } else if (world.has<rendering::Renderable>(entity_id)) {
      entry.kind_label =
          entry.scene_backed ? "Renderable Entity" : "Renderable";
    } else if (entry.scene_backed) {
      entry.kind_label = "Scene Entity";
    } else {
      entry.kind_label = "World Entity";
    }

    entry.meta_label =
        "ID " + static_cast<std::string>(entity_id) + " | " +
        (entry.active ? "active" : "inactive") + " | " +
        (entry.scene_backed ? "scene-backed" : "world-only");

    if (entry.name.empty()) {
      entry.name = "Unnamed Entity";
    }

    snapshot.entities.push_back(std::move(entry));
  });

  std::sort(
      snapshot.entities.begin(),
      snapshot.entities.end(),
      [](const EntityEntry &lhs, const EntityEntry &rhs) {
        if (lhs.scene_backed != rhs.scene_backed) {
          return lhs.scene_backed && !rhs.scene_backed;
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
