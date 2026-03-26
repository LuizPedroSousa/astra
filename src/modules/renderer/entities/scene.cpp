#include "scene.hpp"

#include "adapters/file/file-stream-reader.hpp"
#include "adapters/file/file-stream-writer.hpp"
#include "arena.hpp"
#include "managers/project-manager.hpp"
#include "serializers/scene-serializer.hpp"
#include "stream-buffer.hpp"
#include <cstring>
#include <filesystem>

namespace astralix {
namespace {

std::filesystem::path scene_path(const Scene &scene,
                                 const SerializationContext &ctx) {
  auto project = active_project();
  const std::filesystem::path base_directory =
      project != nullptr ? std::filesystem::path(project->get_config().directory)
                         : std::filesystem::current_path();

  return base_directory / "scenes" / (scene.get_name() + ctx.extension());
}

Scope<StreamBuffer> copy_to_stream_buffer(ElasticArena::Block *block) {
  auto buffer = create_scope<StreamBuffer>(block->size);
  std::memcpy(buffer->data(), block->data, block->size);
  return buffer;
}

} // namespace

Scene::Scene(std::string name) : m_name(name), m_serializer() {}

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
  return true;
}
} // namespace astralix
