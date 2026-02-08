#pragma once

#include "components/mesh/mesh-component.hpp"
#include "components/resource/resource-component.hpp"
#include "components/transform/transform-component.hpp"
#include "entities/object.hpp"
#include "guid.hpp"
#include "renderer-api.hpp"
#include "resources/mesh.hpp"
#include "storage-buffer.hpp"
#include <unordered_map>
#include <vector>

namespace astralix {

struct Batch {
  std::vector<Mesh> meshes;
  std::unordered_map<EntityID, IEntity *> sources;
  std::unordered_map<EntityID, glm::mat4> transforms;
  bool is_dirty = true;
};

struct MeshContext {
  std::unordered_map<MeshGroupID, Batch> batches = {};
};

class MeshCollector {
public:
  template <typename T>
  void draw(std::vector<T *> entities, RendererAPI *renderer_api,
            StorageBuffer *storage_buffer, MeshContext *mesh_ctx) {
    ASTRA_PROFILE_N("MeshCollector Update");

    for (auto entity_ptr : entities) {
      auto entity = static_cast<IEntity *>(entity_ptr);

      ASTRA_PROFILE_N("MeshCollector Object Loop");

      auto mesh_component = entity->get_component<MeshComponent>();
      auto resource = entity->get_component<ResourceComponent>();
      auto transform = entity->get_component<TransformComponent>();

      if (!has_components(mesh_component, resource))
        continue;

      if (!resource->has_shader()) {
        continue;
      }

      auto meshes = mesh_component->get_meshes();

      auto batch_id = compute_group_id(meshes, resource->shader()->id().index);

      auto batch_exists = mesh_ctx->batches.find(batch_id);

      auto entity_id = entity->get_entity_id();

      if (batch_exists == mesh_ctx->batches.end()) {
        std::unordered_map<EntityID, IEntity *> sources;
        std::unordered_map<EntityID, glm::mat4> transforms;

        sources[entity_id] = entity;
        transforms[entity_id] = transform->matrix;

        mesh_ctx->batches[batch_id] = {
            .meshes = meshes, .sources = sources, .transforms = transforms};

        continue;
      }

      auto source = batch_exists->second.sources.find(entity_id);

      if (source == batch_exists->second.sources.end()) {
        batch_exists->second.sources[entity_id] = entity;
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

    for (auto it = mesh_ctx->batches.begin(); it != mesh_ctx->batches.end();) {
      auto &[id, batch] = *it;

      if (batch.sources.size() == 1) {
        for (auto &[_, source] : batch.sources) {
          auto shader = source->get_component<ResourceComponent>()->shader();
          auto transform = source->get_component<TransformComponent>();

          shader->bind();

          shader->set_bool("use_instancing", false);
          shader->set_matrix("g_model", transform->matrix);

          for (auto mesh : batch.meshes) {
            renderer_api->draw_indexed(mesh.vertex_array, mesh.draw_type);
          }

          shader->unbind();
        }

        it = mesh_ctx->batches.erase(it);
        continue;
      }

      if (!batch.is_dirty) {
        it++;
        continue;
      }

      storage_buffer->bind();

      std::vector<glm::mat4> models;

      for (const auto &[_, transform] : batch.transforms) {
        models.push_back(transform);
      }

      storage_buffer->set_data(models.data(),
                               models.size() * sizeof(glm::mat4));

      storage_buffer->unbind();

      it++;
    }

    for (auto [_, batch] : mesh_ctx->batches) {
      auto shader = batch.sources.begin()
                        ->second->get_component<ResourceComponent>()
                        ->shader();

      shader->bind();
      shader->set_bool("use_instacing", true);

      for (auto mesh : batch.meshes) {

        auto vertex_array = mesh.vertex_array;
        vertex_array->bind();

        storage_buffer->bind();

        uint32_t count = vertex_array->get_index_buffer()->get_count();

        renderer_api->draw_instanced_indexed(mesh.draw_type, count,
                                             batch.transforms.size());

        vertex_array->unbind();
      }

      shader->unbind();
    }
  };

private:
  MeshGroupID compute_group_id(std::vector<Mesh> &mesh, uint32_t shader_id);
};

} // namespace astralix
