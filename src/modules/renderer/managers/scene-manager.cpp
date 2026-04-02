#include "managers/scene-manager.hpp"

#include "assert.hpp"
#include "console.hpp"
#include "entities/serializers/scene-serializer.hpp"
#include "managers/project-manager.hpp"

#include <unordered_set>
#include <utility>

namespace astralix {
namespace {

ConsoleCommandResult success_result(std::vector<std::string> lines = {}) {
  ConsoleCommandResult result;
  result.executed = true;
  result.success = true;
  result.lines = std::move(lines);
  return result;
}

ConsoleCommandResult error_result(std::string line) {
  ConsoleCommandResult result;
  result.executed = true;
  result.success = false;
  result.lines.push_back(std::move(line));
  return result;
}

} // namespace

SceneManager::SceneManager() { register_console_commands(); }

void SceneManager::register_scene_type(std::string type, SceneFactory factory) {
  ASTRA_ENSURE(type.empty(), "Scene type is required");
  ASTRA_ENSURE(!factory, "Scene factory is required for type ", type);

  m_scene_factories.insert_or_assign(std::move(type), std::move(factory));
}

void SceneManager::ensure_project_state() {
  auto project = active_project();
  if (project == nullptr) {
    m_loaded_project_id.reset();
    m_active_scene_id.reset();
    m_scene_instances.clear();
    return;
  }

  if (m_loaded_project_id.has_value() &&
      *m_loaded_project_id == static_cast<uint64_t>(project->get_project_id())) {
    return;
  }

  validate_project_scenes(project->get_config());

  m_loaded_project_id = static_cast<uint64_t>(project->get_project_id());
  m_active_scene_id.reset();
  m_scene_instances.clear();
}

void SceneManager::validate_project_scenes(const ProjectConfig &config) const {
  std::unordered_set<std::string> scene_ids;

  for (const auto &entry : config.scenes.entries) {
    ASTRA_ENSURE(entry.id.empty(), "Scene manifest entry id is required");
    ASTRA_ENSURE(entry.type.empty(), "Scene manifest entry type is required");
    ASTRA_ENSURE(entry.path.empty(), "Scene manifest entry path is required");
    ASTRA_ENSURE(!scene_ids.insert(entry.id).second,
                 "Duplicate scene manifest entry id: ", entry.id);
  }

  if (config.scenes.entries.empty()) {
    ASTRA_ENSURE(!config.scenes.startup.empty(),
                 "Startup scene cannot be set when no scenes are declared");
    return;
  }

  ASTRA_ENSURE(config.scenes.startup.empty(),
               "Startup scene is required when project scenes are declared");

  bool startup_exists = false;
  for (const auto &entry : config.scenes.entries) {
    if (entry.id == config.scenes.startup) {
      startup_exists = true;
      break;
    }
  }

  ASTRA_ENSURE(!startup_exists,
               "Startup scene not found in manifest: ",
               config.scenes.startup);
}

Ref<Scene>
SceneManager::instantiate_scene(const ProjectSceneEntryConfig &entry) {
  auto factory_it = m_scene_factories.find(entry.type);
  ASTRA_ENSURE(factory_it == m_scene_factories.end(),
               "No scene factory registered for type ", entry.type);

  Ref<Scene> scene = factory_it->second();
  ASTRA_ENSURE(scene == nullptr,
               "Scene factory returned null for type ", entry.type);

  scene->bind_to_manifest_entry(entry);
  scene->set_serializer(create_ref<SceneSerializer>(scene));
  scene->ensure_setup();

  if (!scene->load()) {
    scene->world() = ecs::World();
    scene->build_default_world();
    scene->mark_world_ready(true);
  }

  scene->after_world_ready();
  return scene;
}

Scene *SceneManager::activate(std::string scene_id) {
  ensure_project_state();

  auto *entry = find_scene_entry(scene_id);
  ASTRA_ENSURE(entry == nullptr, "Unknown scene id: ", scene_id);

  auto instance_it = m_scene_instances.find(scene_id);
  if (instance_it == m_scene_instances.end()) {
    instance_it =
        m_scene_instances.emplace(scene_id, instantiate_scene(*entry)).first;
  }

  m_active_scene_id = scene_id;
  return instance_it->second.get();
}

Scene *SceneManager::get_active_scene() {
  ensure_project_state();

  if (!m_active_scene_id.has_value()) {
    auto project = active_project();
    if (project == nullptr || project->get_config().scenes.startup.empty()) {
      return nullptr;
    }

    return activate(project->get_config().scenes.startup);
  }

  auto it = m_scene_instances.find(*m_active_scene_id);
  return it != m_scene_instances.end() ? it->second.get() : nullptr;
}

std::optional<std::string> SceneManager::get_active_scene_id() {
  ensure_project_state();

  if (!m_active_scene_id.has_value()) {
    (void)get_active_scene();
  }

  return m_active_scene_id;
}

std::vector<ProjectSceneEntryConfig> SceneManager::get_scene_entries() {
  ensure_project_state();

  auto project = active_project();
  if (project == nullptr) {
    return {};
  }

  return project->get_config().scenes.entries;
}

const ProjectSceneEntryConfig *
SceneManager::find_scene_entry(std::string_view scene_id) {
  ensure_project_state();

  auto project = active_project();
  if (project == nullptr) {
    return nullptr;
  }

  return project->find_scene_entry(scene_id);
}

void SceneManager::register_console_commands() {
  auto &console = ConsoleManager::get();
  console.register_command(
      "scene_activate",
      "Activate a scene by manifest id. Run without arguments to list scenes.",
      [this](const ConsoleCommandInvocation &invocation) {
        const auto scene_entries = get_scene_entries();
        if (scene_entries.empty()) {
          return error_result("no scenes declared in the active project");
        }

        if (invocation.arguments.empty()) {
          std::vector<std::string> lines;
          const auto active_scene_id = get_active_scene_id();

          for (const auto &entry : scene_entries) {
            const bool is_active =
                active_scene_id.has_value() && *active_scene_id == entry.id;
            lines.push_back(
                std::string(is_active ? "* " : "  ") + entry.id + " (" +
                entry.type + ")"
            );
          }

          return success_result(std::move(lines));
        }

        Scene *scene = activate(invocation.arguments.front());
        return success_result(
            {std::string("activated scene ") + scene->get_scene_id()}
        );
      },
      {"scene"}
  );
}

} // namespace astralix
