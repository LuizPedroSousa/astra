#include "scene.hpp"

#include "adapters/file/file-stream-reader.hpp"
#include "adapters/file/file-stream-writer.hpp"
#include "arena.hpp"
#include "assert.hpp"
#include "managers/project-manager.hpp"
#include "project.hpp"
#include "serializers/scene-serializer.hpp"
#include "stream-buffer.hpp"
#include <cstring>
#include <filesystem>

namespace astralix {
namespace {

std::filesystem::path scene_path(const Scene &scene, const SerializationContext &ctx) {
  (void)ctx;
  auto project = active_project();
  ASTRA_ENSURE(project == nullptr, "Cannot resolve scene path without an active project");
  ASTRA_ENSURE(scene.get_scene_path().empty(), "Scene path is not configured for scene ", scene.get_type());

  return project->resolve_path(scene.get_scene_path());
}

Scope<StreamBuffer> copy_to_stream_buffer(ElasticArena::Block *block) {
  auto buffer = create_scope<StreamBuffer>(block->size);
  std::memcpy(buffer->data(), block->data, block->size);
  return buffer;
}

} // namespace

Scene::Scene(std::string type) : m_scene_type(std::move(type)), m_serializer() {}

void Scene::bind_to_manifest_entry(const ProjectSceneEntryConfig &entry) {
  ASTRA_ENSURE(entry.id.empty(), "Scene manifest id is required");
  ASTRA_ENSURE(entry.type.empty(), "Scene manifest type is required");
  ASTRA_ENSURE(entry.path.empty(), "Scene manifest path is required");
  ASTRA_ENSURE(m_scene_type != entry.type, "Scene type mismatch. Runtime type: ", m_scene_type, ", manifest type: ", entry.type);

  m_scene_id = entry.id;
  m_scene_path = entry.path;
}

void Scene::ensure_setup() {
  if (m_setup_complete) {
    return;
  }

  setup();
  m_setup_complete = true;
}

void Scene::serialize() { return m_serializer->serialize(); }

void Scene::save() {
  if (m_serializer == nullptr) {
    return;
  }

  m_serializer->serialize();

  auto ctx = m_serializer->get_ctx();
  if (ctx == nullptr) {
    return;
  }

  const auto path = scene_path(*this, *ctx);
  std::filesystem::create_directories(path.parent_path());

  ElasticArena arena(KB(64));
  auto *block = ctx->to_buffer(arena);
  auto writer = FileStreamWriter(path, copy_to_stream_buffer(block));
  writer.write();
}

bool Scene::load() {
  if (m_serializer == nullptr) {
    return false;
  }

  auto ctx = m_serializer->get_ctx();
  if (ctx == nullptr) {
    return false;
  }

  const auto path = scene_path(*this, *ctx);
  if (!std::filesystem::exists(path)) {
    return false;
  }

  auto reader = FileStreamReader(path);
  reader.read();
  ctx->from_buffer(reader.get_buffer());
  m_serializer->deserialize();
  mark_world_ready(true);
  return true;
}
} // namespace astralix
