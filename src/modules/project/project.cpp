#include "project.hpp"

#include "adapters/file/file-stream-reader.hpp"
#include "arena.hpp"
#include "assets/asset_registry.hpp"
#include "assert.hpp"
#include "guid.hpp"
#include "log.hpp"
#include "managers/path-manager.hpp"
#include "managers/resource-manager.hpp"
#include "serialization-context.hpp"
#include "trace.hpp"
#include <filesystem>

namespace astralix {

Project::Project(ProjectConfig config)
    : m_config(config), m_project_id(ProjectID()) {}

std::filesystem::path Project::manifest_path() const {
  return resolve_path(m_config.manifest);
}

std::filesystem::path
Project::resolve_path(const std::filesystem::path &relative_path) const {
  return std::filesystem::path(m_config.directory) / relative_path;
}

const ProjectSceneEntryConfig *
Project::find_scene_entry(std::string_view id) const {
  for (const auto &entry : m_config.scenes.entries) {
    if (entry.id == id) {
      return &entry;
    }
  }

  return nullptr;
}

Ref<Project> Project::create(ProjectConfig config) {
  ASTRA_PROFILE_N("Project::create");

  auto project = create_ref<Project>(config);

  ASTRA_ENSURE(config.directory.empty(), "Project path must be defined");

  {
    ASTRA_PROFILE_N("Project::create_serializer");
    project->m_serializer = create_scope<ProjectSerializer>(
        project, SerializationContext::create(config.serialization.format)
    );
  }

  {
    ASTRA_PROFILE_N("ResourceManager::init");
    ResourceManager::init();
  }
  {
    ASTRA_PROFILE_N("PathManager::init");
    PathManager::init();
  }

  const auto manifest_path = project->manifest_path();

  if (std::filesystem::exists(manifest_path)) {
    ASTRA_PROFILE_N("Project::deserialize_manifest");
    auto stream = FileStreamReader(manifest_path);
    stream.read();

    project->m_serializer->get_ctx()->from_buffer(stream.get_buffer());
    project->m_serializer->deserialize();
  } else {
    ASTRA_PROFILE_N("Project::serialize_manifest");
    project->m_serializer->serialize();
  }

  {
    ASTRA_PROFILE_N("AssetRegistry::load_root_assets");
    project->m_asset_registry = create_scope<AssetRegistry>(project);
    project->m_asset_registry->load_root_assets(
        project->m_config.resources.asset_bindings
    );
  }

  return project;
}

void Project::reload_manifest() {
  const auto path = manifest_path();

  if (!std::filesystem::exists(path)) {
    return;
  }

  auto stream = FileStreamReader(path);
  stream.read();

  m_serializer->get_ctx()->from_buffer(stream.get_buffer());
  m_serializer->deserialize();

  for (const auto &callback : m_manifest_reload_callbacks) {
    callback(m_config);
  }

  if (m_asset_registry) {
    m_asset_registry->reload_all(m_config.resources.asset_bindings);
  }
}

void Project::on_manifest_reload(ManifestReloadCallback callback) {
  m_manifest_reload_callbacks.push_back(std::move(callback));
}

void Project::save(ElasticArena &arena) {
  m_serializer->serialize();

  auto ctx = m_serializer->get_ctx();

  auto buffer = ctx->to_buffer(arena);

  auto path = manifest_path();
  (void)buffer;
  (void)path;

  // FileStreamWriter writer(path, std::move(buffer));

  // writer.write();
}

void Project::load(ElasticArena &arena) {
  (void)arena;
  // auto ctx = m_serializer->get_ctx();

  // auto path = std::filesystem::path(m_config.directory)
  //                 .append("project" + ctx->extension());
  //
  // FileStreamReader reader(path);
  //
  // reader.read();
  //
  // auto buffer = reader.get_buffer();
  // ctx->from_buffer(std::move(buffer));
  //
  // m_serializer->deserialize();
}

} // namespace astralix
