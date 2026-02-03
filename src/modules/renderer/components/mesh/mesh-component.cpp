#include "mesh-component.hpp"
#include "components/mesh/serialzers/mesh-component-serializer.hpp"
#include "targets/render-target.hpp"
#include "vector"
#include "vertex-array.hpp"
#include "vertex-buffer.hpp"

namespace astralix {

MeshComponent::MeshComponent(COMPONENT_INIT_PARAMS)
    : COMPONENT_INIT(MeshComponent, "mesh", true,
                     create_ref<MeshComponentSerializer>(this)),
      m_draw_type(VertexBuffer::DrawType::Static) {

      };

void MeshComponent::start(Ref<RenderTarget> render_target) {
  for (auto &mesh : m_meshes) {
    auto api = render_target->renderer_api()->get_backend();
    mesh.vertex_array = VertexArray::create(api);

    Ref<VertexBuffer> vertex_buffer = VertexBuffer::create(
        api, &mesh.vertices[0], mesh.vertices.size() * sizeof(Vertex),
        m_draw_type);

    BufferLayout layout(
        {BufferElement(ShaderDataType::Float3, "position"),
         BufferElement(ShaderDataType::Float3, "normal"),
         BufferElement(ShaderDataType::Float2, "texture_coordinates"),
         BufferElement(ShaderDataType::Float3, "tangent")}

    );

    vertex_buffer->set_layout(layout);

    mesh.vertex_array->add_vertex_buffer(vertex_buffer);

    mesh.vertex_array->set_index_buffer(
        IndexBuffer::create(api, &mesh.indices[0], mesh.indices.size()));

    mesh.vertex_array->unbind();
  }
}

void MeshComponent::update(Ref<RenderTarget> render_target) {
  for (auto mesh : m_meshes) {
    render_target->renderer_api()->draw_indexed(mesh.vertex_array,
                                                mesh.draw_type);
  }
};
} // namespace astralix
