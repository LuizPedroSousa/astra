#include "mesh-system.hpp"

#include "base.hpp"
#include "components/mesh/mesh-component.hpp"
#include "components/resource/resource-component.hpp"
#include "components/transform/transform-component.hpp"
#include "engine.hpp"
#include "entities/object.hpp"
#include "glad//glad.h"
#include "glm/gtc/type_ptr.hpp"
#include "guid.hpp"
#include "managers/entity-manager.hpp"
#include "managers/resource-manager.hpp"
#include "renderer-api.hpp"
#include "storage-buffer.hpp"
#include "trace.hpp"
#include <cstdint>

#include <unordered_map>

namespace astralix {

MeshSystem::MeshSystem(Ref<RenderTarget> render_target)
    : m_render_target(render_target) {}

void MeshSystem::pre_update(double dt) {}

void MeshSystem::fixed_update(double fixed_dt) {}

void MeshSystem::start() {
  auto resource_manager = ResourceManager::get();

  m_storage_buffer = StorageBuffer::create(
      m_render_target->renderer_api()->get_backend(), 1024 * sizeof(glm::mat4));
}

void MeshSystem::update(double dt) {
  CHECK_ACTIVE(this);
  ASTRA_PROFILE_N("MeshSystem Update");

  auto entity_manager = EntityManager::get();

  auto objects = entity_manager->get_entities<Object>();

  for (auto object : objects) {
    ASTRA_PROFILE_N("MeshSystem Object Loop");

    auto mesh_component = object->get_component<MeshComponent>();

    if (mesh_component == nullptr) {
      continue;
    }

    auto resource = object->get_component<ResourceComponent>();

    if (!resource->has_shader()) {
      continue;
    }

    auto meshes = mesh_component->get_meshes();

    auto batch_id = compute_group_id(meshes, resource->shader()->id().index);

    auto batch_exists = m_batches.find(batch_id);

    auto transform = object->get_component<TransformComponent>();

    auto entity_id = object->get_entity_id();

    if (batch_exists == m_batches.end()) {
      std::unordered_map<EntityID, Object *> sources;
      std::unordered_map<EntityID, glm::mat4> transforms;

      sources[entity_id] = object;
      transforms[entity_id] = transform->matrix;

      m_batches[batch_id] = {
          .meshes = meshes, .sources = sources, .transforms = transforms};

      continue;
    }

    auto source = batch_exists->second.sources.find(entity_id);

    if (source == batch_exists->second.sources.end()) {
      batch_exists->second.sources[entity_id] = object;
      batch_exists->second.transforms[entity_id] = transform->matrix;
      batch_exists->second.is_dirty = true;
      continue;
    } else {

      if (transform->matrix != batch_exists->second.transforms[entity_id]) {
        batch_exists->second.is_dirty = true;
        continue;
      }
    }

    batch_exists->second.is_dirty = false;
  }

  auto engine = Engine::get();

  for (auto it = m_batches.begin(); it != m_batches.end();) {
    auto &[id, batch] = *it;

    if (batch.sources.size() == 1) {
      for (auto &[_, source] : batch.sources) {
        auto shader = source->get_component<ResourceComponent>()->shader();
        auto transform = source->get_component<TransformComponent>();

        shader->bind();

        shader->set_bool("use_instancing", false);
        shader->set_matrix("g_model", transform->matrix);

        for (auto mesh : batch.meshes) {
          m_render_target->renderer_api()->draw_indexed(mesh.vertex_array,
                                                        mesh.draw_type);
        }

        shader->unbind();
      }

      it = m_batches.erase(it);
      continue;
    }

    if (!batch.is_dirty) {
      it++;
      continue;
    }

    m_storage_buffer->bind();

    std::vector<glm::mat4> models;

    for (const auto &[_, transform] : batch.transforms) {
      models.push_back(transform);
    }

    m_storage_buffer->set_data(models.data(),
                               models.size() * sizeof(glm::mat4));

    m_storage_buffer->unbind();

    it++;
  }

  for (auto [_, batch] : m_batches) {
    auto shader = batch.sources.begin()
                      ->second->get_component<ResourceComponent>()
                      ->shader();

    shader->bind();
    shader->set_bool("use_instacing", true);

    for (auto mesh : batch.meshes) {

      auto vertex_array = mesh.vertex_array;
      vertex_array->bind();

      m_storage_buffer->bind();

      uint32_t count = vertex_array->get_index_buffer()->get_count();

      m_render_target->renderer_api()->draw_instanced_indexed(
          mesh.draw_type, count, batch.transforms.size());

      vertex_array->unbind();
    }

    shader->unbind();
  }
}

MeshGroupID MeshSystem::compute_group_id(std::vector<Mesh> &meshes,
                                         uint32_t shader_id) {
  ASTRA_PROFILE_N("MeshSystem Compute Group ID");

  MeshGroupID group_id = shader_id;

  for (auto mesh : meshes) {
    group_id ^= mesh.id;
  }

  return group_id;
}

MeshSystem::~MeshSystem() {}

} // namespace astralix
