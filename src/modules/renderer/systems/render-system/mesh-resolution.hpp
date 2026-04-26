#pragma once

#include "components/mesh.hpp"
#include "components/model.hpp"
#include "components/tags.hpp"
#include "components/transform.hpp"
#include "managers/resource-manager.hpp"
#include "render-frame.hpp"
#include "resources/model.hpp"
#include "targets/render-target.hpp"
#include "vertex-array.hpp"
#include "vertex-buffer.hpp"
#include "world.hpp"
#include <vector>

namespace astralix::rendering {

inline void ensure_mesh_uploaded(Mesh &mesh, Ref<RenderTarget> render_target) {
  if (mesh.vertex_array != nullptr) {
    return;
  }

  const auto backend = render_target->backend();
  mesh.vertex_array = VertexArray::create(backend);

  Ref<VertexBuffer> vertex_buffer = VertexBuffer::create(
      backend, mesh.vertices.data(), mesh.vertices.size() * sizeof(Vertex), VertexBuffer::DrawType::Static
  );

  BufferLayout layout(
      {BufferElement(ShaderDataType::Float3, "position").at_location(0),
       BufferElement(ShaderDataType::Float3, "normal").at_location(1),
       BufferElement(ShaderDataType::Float2, "texture_coordinates").at_location(2),
       BufferElement(ShaderDataType::Float3, "tangent").at_location(3)}
  );

  vertex_buffer->set_layout(layout);
  mesh.vertex_array->add_vertex_buffer(vertex_buffer);
  mesh.vertex_array->set_index_buffer(
      IndexBuffer::create(backend, mesh.indices.data(), mesh.indices.size())
  );
  mesh.vertex_array->unbind();
}

inline ResolvedMeshDraw
prepare_mesh_draw(Mesh &mesh, const ResourceDescriptorID &source_model_id,
                  uint32_t submesh_index, Ref<RenderTarget> render_target) {
  ensure_mesh_uploaded(mesh, render_target);

  return ResolvedMeshDraw{
      .mesh_id = mesh.id,
      .source_model_id = source_model_id,
      .submesh_index = submesh_index,
      .vertex_array = mesh.vertex_array,
      .draw_type = mesh.draw_type,
      .index_count = static_cast<uint32_t>(mesh.indices.size()),
  };
}

inline std::vector<ResolvedMeshDraw>
prepare_render_meshes(ModelRef *model_ref, MeshSet *mesh_set,
                      Ref<RenderTarget> render_target) {
  std::vector<ResolvedMeshDraw> resolved_meshes;

  if (model_ref != nullptr) {
    for (const auto &resource_id : model_ref->resource_ids) {
      auto model = resource_manager()->get_by_descriptor_id<Model>(resource_id);
      if (model == nullptr) {
        continue;
      }

      resolved_meshes.reserve(resolved_meshes.size() + model->meshes.size());
      for (uint32_t submesh_index = 0;
           submesh_index < static_cast<uint32_t>(model->meshes.size());
           ++submesh_index) {
        auto &mesh = model->meshes[submesh_index];
        resolved_meshes.push_back(
            prepare_mesh_draw(mesh, resource_id, submesh_index, render_target));
      }
    }

    return resolved_meshes;
  }

  if (mesh_set == nullptr) {
    return resolved_meshes;
  }

  resolved_meshes.reserve(mesh_set->meshes.size());
  for (uint32_t submesh_index = 0;
       submesh_index < static_cast<uint32_t>(mesh_set->meshes.size());
       ++submesh_index) {
    auto &mesh = mesh_set->meshes[submesh_index];
    resolved_meshes.push_back(
        prepare_mesh_draw(mesh, {}, submesh_index, render_target));
  }

  return resolved_meshes;
}

inline bool has_renderables(ecs::World &world) {
  bool found = false;

  world.each<Renderable, scene::Transform>([&](EntityID entity_id, Renderable &, scene::Transform &) {
    if (found || !world.active(entity_id)) {
      return;
    }

    auto entity = world.entity(entity_id);
    found =
        entity.get<ModelRef>() != nullptr || entity.get<MeshSet>() != nullptr;
  });

  return found;
}

inline ResolvedMeshDraw create_skybox_cube_mesh(RendererBackend backend) {
  // clang-format off
  static constexpr float positions[] = {
    -1.0f,  1.0f, -1.0f,  -1.0f, -1.0f, -1.0f,   1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,   1.0f,  1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,  -1.0f, -1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,  -1.0f,  1.0f,  1.0f,  -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f, -1.0f,   1.0f, -1.0f,  1.0f,   1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,   1.0f,  1.0f, -1.0f,   1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,   1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,   1.0f, -1.0f,  1.0f,  -1.0f, -1.0f,  1.0f,
    -1.0f,  1.0f, -1.0f,   1.0f,  1.0f, -1.0f,   1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,  -1.0f,  1.0f, -1.0f,
    -1.0f, -1.0f, -1.0f,  -1.0f, -1.0f,  1.0f,   1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,  -1.0f, -1.0f,  1.0f,   1.0f, -1.0f,  1.0f,
  };
  // clang-format on

  static constexpr uint32_t vertex_count = 36;

  auto vertex_array = VertexArray::create(backend);

  auto vertex_buffer = VertexBuffer::create(
      backend, positions, sizeof(positions), VertexBuffer::DrawType::Static
  );
  vertex_buffer->set_layout(BufferLayout({
      BufferElement(ShaderDataType::Float3, "position").at_location(0),
  }));
  vertex_array->add_vertex_buffer(vertex_buffer);

  std::vector<uint32_t> indices(vertex_count);
  for (uint32_t i = 0; i < vertex_count; ++i) {
    indices[i] = i;
  }
  vertex_array->set_index_buffer(
      IndexBuffer::create(backend, indices.data(), vertex_count)
  );
  vertex_array->unbind();

  return ResolvedMeshDraw{
      .vertex_array = vertex_array,
      .draw_type = RendererAPI::DrawPrimitive::TRIANGLES,
      .index_count = vertex_count,
  };
}

inline ResolvedMeshDraw create_fullscreen_quad_mesh(RendererBackend backend) {
  struct QuadVertex {
    glm::vec2 position;
    glm::vec3 normal;
    glm::vec2 texture_coordinates;
  };

  const bool is_vulkan = backend == RendererBackend::Vulkan;
  const float top_v = is_vulkan ? 0.0f : 1.0f;
  const float bottom_v = is_vulkan ? 1.0f : 0.0f;

  const QuadVertex vertices[] = {
      {{-1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, top_v}},
      {{-1.0f, -1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, bottom_v}},
      {{1.0f, -1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, bottom_v}},
      {{1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, top_v}},
  };
  uint32_t indices[] = {0, 1, 2, 0, 2, 3};

  auto vertex_array = VertexArray::create(backend);

  auto vertex_buffer = VertexBuffer::create(
      backend, vertices, sizeof(vertices), VertexBuffer::DrawType::Static
  );
  vertex_buffer->set_layout(BufferLayout({
      BufferElement(ShaderDataType::Float2, "position").at_location(0),
      BufferElement(ShaderDataType::Float3, "normal").at_location(1),
      BufferElement(ShaderDataType::Float2, "texture_coordinates").at_location(2),
  }));
  vertex_array->add_vertex_buffer(vertex_buffer);
  vertex_array->set_index_buffer(
      IndexBuffer::create(backend, indices, 6)
  );
  vertex_array->unbind();

  return ResolvedMeshDraw{
      .vertex_array = vertex_array,
      .draw_type = RendererAPI::DrawPrimitive::TRIANGLES,
      .index_count = 6,
  };
}

} // namespace astralix::rendering
