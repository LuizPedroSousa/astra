#pragma once

#include "components/mesh.hpp"
#include "components/model.hpp"
#include "components/tags.hpp"
#include "components/transform.hpp"
#include "managers/resource-manager.hpp"
#include "resources/descriptors/model-descriptor.hpp"
#include "resources/model.hpp"
#include "targets/render-target.hpp"
#include "vertex-array.hpp"
#include "vertex-buffer.hpp"
#include "world.hpp"

namespace astralix::rendering {

inline void ensure_mesh_uploaded(Mesh &mesh, Ref<RenderTarget> render_target) {
  if (mesh.vertex_array != nullptr) {
    return;
  }

  const auto backend = render_target->renderer_api()->get_backend();
  mesh.vertex_array = VertexArray::create(backend);

  Ref<VertexBuffer> vertex_buffer = VertexBuffer::create(
      backend, mesh.vertices.data(), mesh.vertices.size() * sizeof(Vertex),
      VertexBuffer::DrawType::Static);

  BufferLayout layout(
      {BufferElement(ShaderDataType::Float3, "position"),
       BufferElement(ShaderDataType::Float3, "normal"),
       BufferElement(ShaderDataType::Float2, "texture_coordinates"),
       BufferElement(ShaderDataType::Float3, "tangent")});

  vertex_buffer->set_layout(layout);
  mesh.vertex_array->add_vertex_buffer(vertex_buffer);
  mesh.vertex_array->set_index_buffer(
      IndexBuffer::create(backend, mesh.indices.data(), mesh.indices.size()));
  mesh.vertex_array->unbind();
}

template <typename Fn>
inline void for_each_resolved_mesh(const ModelRef &model_ref,
                                   Ref<RenderTarget> render_target, Fn &&fn) {
  const auto backend = render_target->renderer_api()->get_backend();

  for (const auto &resource_id : model_ref.resource_ids) {
    resource_manager()->load_from_descriptors_by_ids<ModelDescriptor>(
        backend, {resource_id});

    auto model = resource_manager()->get_by_descriptor_id<Model>(resource_id);
    if (model == nullptr) {
      continue;
    }

    for (auto &mesh : model->meshes) {
      ensure_mesh_uploaded(mesh, render_target);
      fn(mesh);
    }
  }
}

template <typename Fn>
inline void for_each_render_mesh(ModelRef *model_ref, MeshSet *mesh_set,
                                 Ref<RenderTarget> render_target, Fn &&fn) {
  if (model_ref != nullptr) {
    for_each_resolved_mesh(*model_ref, render_target, std::forward<Fn>(fn));
    return;
  }

  if (mesh_set == nullptr) {
    return;
  }

  for (auto &mesh : mesh_set->meshes) {
    ensure_mesh_uploaded(mesh, render_target);
    fn(mesh);
  }
}

inline bool has_renderables(ecs::World &world) {
  bool found = false;

  world.each<Renderable, scene::Transform>([&](EntityID entity_id, Renderable &,
                                        scene::Transform &) {
    if (found || !world.active(entity_id)) {
      return;
    }

    auto entity = world.entity(entity_id);
    found =
        entity.get<ModelRef>() != nullptr || entity.get<MeshSet>() != nullptr;
  });

  return found;
}

} // namespace astralix::rendering
