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

Ref<Project> Project::create(ProjectConfig config) {

  auto project = create_ref<Project>(config);

  ASTRA_ENSURE(config.directory.empty(), "Project path must be defined");

  project->m_serializer = create_scope<ProjectSerializer>(
      project, SerializationContext::create(config.serialization.format));

  ResourceManager::init();
  PathManager::init();

  auto manifest_path =
      std::filesystem::path(config.directory) / config.manifest;

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

  auto path = std::filesystem::path(m_config.directory)
                  .append("project" + ctx->extension());

  // FileStreamWriter writer(path, std::move(buffer));

  // writer.write();
}

void Project::load(ElasticArena &arena) {
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
