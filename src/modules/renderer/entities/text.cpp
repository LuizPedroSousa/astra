
#include "text.hpp"
#include "base.hpp"
#include "components/mesh/mesh-component.hpp"
#include "components/resource/resource-component.hpp"
#include "entities/ientity.hpp"

#include "glad/glad.h"
#include "guid.hpp"
#include "managers/resource-manager.hpp"
#include "resources/font.hpp"
#include "vertex-buffer.hpp"

namespace astralix {

Text::Text(ENTITY_INIT_PARAMS, std::string text, ResourceDescriptorID font_id,
           glm::vec2 position, float scale, glm::vec3 color)
    : ENTITY_INIT(), m_text(text), m_font_id(font_id), m_position(position),
      m_scale(scale), m_color(color), m_mesh(Mesh::quad(1.0f)) {
  add_component<ResourceComponent>();
  add_component<MeshComponent>();
}

void Text::start(Ref<RenderTarget> render_target) {
  auto resource = get_component<ResourceComponent>();
  auto mesh_comp = get_component<MeshComponent>();

  resource->set_shader("shaders::glyph");
  resource->start();

  mesh_comp->set_draw_type(VertexBuffer::DrawType::Dynamic);

  m_mesh.vertex_array =
      VertexArray::create(render_target->renderer_api()->get_backend());

  Ref<VertexBuffer> vertex_buffer = VertexBuffer::create(
      render_target->renderer_api()->get_backend(), &m_mesh.vertices[0],
      m_mesh.vertices.size() * sizeof(Vertex), VertexBuffer::DrawType::Dynamic);

  BufferLayout layout({
      BufferElement(ShaderDataType::Float3, "position"),
      BufferElement(ShaderDataType::Float3, "normal"),
      BufferElement(ShaderDataType::Float2, "texture_coordinates"),
  });

  vertex_buffer->set_layout(layout);

  m_mesh.vertex_array->add_vertex_buffer(vertex_buffer);
  m_mesh.vertex_array->unbind();
}

void Text::pre_update() {}

void Text::update() {
  CHECK_ACTIVE(this);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  auto resource = get_component<ResourceComponent>();
  // auto mesh_comp = get_component<MeshComponent>();

  auto component_manager = ComponentManager::get();

  glm::mat4 projection = glm::ortho(0.0f, 1920.0f, 0.0f, 1080.0f);

  resource->update();
  std::string::const_iterator c;

  auto shader = resource->shader();

  shader->set_vec3("textColor", m_color);
  shader->set_matrix("projection", projection);

  auto resource_manager = ResourceManager::get();

  auto font = resource_manager->get_by_descriptor_id<Font>(m_font_id);
  
  ASTRA_ENSURE(font == nullptr, "Font not found for Text entity: ", m_font_id);

  auto characters = font->characters();

  shader->set_int("text", 0);
  glActiveTexture(GL_TEXTURE0);

  m_mesh.vertex_array->bind();

  float current_x = m_position.x;
  for (c = m_text.begin(); c != m_text.end(); c++) {
    CharacterGlyph cg = characters[*c];

    auto texture =
        resource_manager->get_by_descriptor_id<Texture2D>(cg.texture_id);

    float xpos = current_x + cg.bearing.x * m_scale;
    float ypos = m_position.y - (cg.size.y - cg.bearing.y) * m_scale;

    float w = cg.size.x * m_scale;
    float h = cg.size.y * m_scale;

    glm::vec2 vertices[6] = {
        glm::vec2(xpos, ypos + h),    // Vertex 0
        glm::vec2(xpos, ypos),        // Vertex 1
        glm::vec2(xpos + w, ypos),    // Vertex 2
        glm::vec2(xpos, ypos + h),    // Vertex 3
        glm::vec2(xpos + w, ypos),    // Vertex 4
        glm::vec2(xpos + w, ypos + h) // Vertex 5
    };

    glm::vec2 texCoords[6] = {
        glm::vec2(0.0f, 0.0f), // Vertex 0
        glm::vec2(0.0f, 1.0f), // Vertex 1
        glm::vec2(1.0f, 1.0f), // Vertex 2
        glm::vec2(0.0f, 0.0f), // Vertex 3
        glm::vec2(1.0f, 1.0f), // Vertex 4
        glm::vec2(1.0f, 0.0f)  // Vertex 5
    };

    for (int i = 0; i < 4; i++) {
      m_mesh.vertices[i].position =
          glm::vec3(vertices[i].x, vertices[i].y, 0.0f);
      m_mesh.vertices[i].texture_coordinates = texCoords[i];
    }

    glBindTexture(GL_TEXTURE_2D, texture->renderer_id());

    auto vertex_buffer = m_mesh.vertex_array->get_vertex_buffers()[0];

    vertex_buffer->set_data(m_mesh.vertices.data(),
                            m_mesh.vertices.size() * sizeof(Vertex));

    vertex_buffer->unbind();
    glDrawArrays(GL_TRIANGLES, 0, 6);

    current_x += (cg.advance >> 6) * m_scale;
  }

  m_mesh.vertex_array->unbind();

  glBindTexture(GL_TEXTURE_2D, 0);

  // mesh->update();
}

void Text::post_update() {}

} // namespace astralix
