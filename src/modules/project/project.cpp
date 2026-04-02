#include "project.hpp"

#include "adapters/file/file-stream-reader.hpp"
#include "arena.hpp"
#include "assert.hpp"
#include "guid.hpp"
#include "log.hpp"
#include "managers/path-manager.hpp"
#include "managers/resource-manager.hpp"
#include "serialization-context.hpp"
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

  auto project = create_ref<Project>(config);

  ASTRA_ENSURE(config.directory.empty(), "Project path must be defined");

  project->m_serializer = create_scope<ProjectSerializer>(
      project, SerializationContext::create(config.serialization.format)
  );

  ResourceManager::init();
  PathManager::init();

  const auto manifest_path = project->manifest_path();

  if (std::filesystem::exists(manifest_path)) {
    auto stream = FileStreamReader(manifest_path);
    stream.read();

    project->m_serializer->get_ctx()->from_buffer(stream.get_buffer());
    project->m_serializer->deserialize();
  } else {
    project->m_serializer->serialize();
  }

  return project;
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
