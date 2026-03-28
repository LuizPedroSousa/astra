#pragma once

#include "framebuffer.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "log.hpp"
#include "managers/resource-manager.hpp"
#include "managers/scene-manager.hpp"
#include "render-pass.hpp"
#include "resources/descriptors/font-descriptor.hpp"
#include "resources/descriptors/shader-descriptor.hpp"
#include "resources/font.hpp"
#include "resources/mesh.hpp"
#include "resources/texture.hpp"
#include "shaders/engine_shaders_glyph_axsl.hpp"
#include "systems/render-system/passes/render-graph-resource.hpp"
#include "systems/render-system/scene-selection.hpp"
#include "targets/render-target.hpp"
#include "vertex-array.hpp"
#include "vertex-buffer.hpp"
#include <array>

#if __has_include(ASTRALIX_ENGINE_BINDINGS_HEADER)
#include ASTRALIX_ENGINE_BINDINGS_HEADER
#define ASTRALIX_HAS_ENGINE_BINDINGS
#endif

namespace astralix {

class TextPass : public RenderPass {
public:
  TextPass() = default;
  ~TextPass() override = default;

  void
  setup(Ref<RenderTarget> render_target,
        const std::vector<const RenderGraphResource *> &resources) override {
    m_render_target = render_target;

    for (auto resource : resources) {
      if (resource->desc.type == RenderGraphResourceType::Framebuffer &&
          resource->desc.name == "scene_color") {
        m_scene_color = resource->get_framebuffer();
      }
    }

    if (m_scene_color == nullptr) {
      set_enabled(false);
      LOG_WARN("[TextPass] Skipping setup: scene_color framebuffer is not "
               "available");
      return;
    }

    const auto backend = m_render_target->renderer_api()->get_backend();
    resource_manager()->load_from_descriptors_by_ids<ShaderDescriptor>(
        backend, {"shaders::glyph"});
    m_shader =
        resource_manager()->get_by_descriptor_id<Shader>("shaders::glyph");

    ensure_glyph_quad_uploaded();
  }

  void begin(double dt) override {}

  void execute(double dt) override {
    if (m_scene_color == nullptr) {
      LOG_WARN("[TextPass] Skipping execute: scene_color framebuffer is not "
               "available");
      return;
    }

    if (m_shader == nullptr) {
      LOG_WARN("[TextPass] Skipping execute: shaders::glyph is not available");
      return;
    }

#ifdef ASTRALIX_HAS_ENGINE_BINDINGS
    using namespace shader_bindings::engine_shaders_glyph_axsl;
    auto scene = SceneManager::get()->get_active_scene();
    if (scene == nullptr) {
      LOG_WARN("[TextPass] Skipping execute: no active scene");
      return;
    }

    auto &world = scene->world();
    auto sprites = rendering::collect_text_sprites(world);
    if (sprites.empty()) {
      return;
    }

    const auto backend = m_render_target->renderer_api()->get_backend();
    const auto &spec = m_scene_color->get_specification();
    const float projection_width =
        spec.width > 0 ? static_cast<float>(spec.width) : 1920.0f;
    const float projection_height =
        spec.height > 0 ? static_cast<float>(spec.height) : 1080.0f;
    const glm::mat4 projection =
        glm::ortho(0.0f, projection_width, 0.0f, projection_height);

    auto renderer_api = m_render_target->renderer_api();
    m_scene_color->bind();
    renderer_api->disable_depth_test();
    renderer_api->disable_depth_write();
    renderer_api->enable_blend();
    renderer_api->set_blend_func(RendererAPI::BlendFactor::SrcAlpha,
                                 RendererAPI::BlendFactor::OneMinusSrcAlpha);

    m_shader->bind();
    m_shader->set(TextUniform::glyph, 0);
    m_shader->set(CameraUniform::projection, projection);

    for (const auto &selection : sprites) {
      if (selection.sprite == nullptr) {
        continue;
      }

      resource_manager()->load_from_descriptors_by_ids<FontDescriptor>(
          backend, {selection.sprite->font_id});

      auto font = resource_manager()->get_by_descriptor_id<Font>(
          selection.sprite->font_id);
      if (font == nullptr) {
        continue;
      }

      m_shader->set(TextUniform::color, selection.sprite->color);
      draw_text_sprite(*selection.sprite, *font);
    }

    m_shader->unbind();
    renderer_api->disable_blend();
    renderer_api->enable_depth_write();
    renderer_api->enable_depth_test();
    renderer_api->bind_texture_2d(0, 0);
    m_scene_color->unbind();
#endif
  }

  void end(double dt) override {}

  void cleanup() override {}

  std::string name() const override { return "TextPass"; }

private:
  void ensure_glyph_quad_uploaded() {
    if (m_glyph_quad.vertex_array != nullptr) {
      return;
    }

    const auto backend = m_render_target->renderer_api()->get_backend();

    m_glyph_quad.vertex_array = VertexArray::create(backend);
    m_vertex_buffer = VertexBuffer::create(
        backend, m_glyph_quad.vertices.data(),
        static_cast<uint32_t>(m_glyph_quad.vertices.size() * sizeof(Vertex)),
        VertexBuffer::DrawType::Dynamic);

    BufferLayout layout(
        {BufferElement(ShaderDataType::Float3, "position"),
         BufferElement(ShaderDataType::Float3, "normal"),
         BufferElement(ShaderDataType::Float2, "texture_coordinates"),
         BufferElement(ShaderDataType::Float3, "tangent")});

    m_vertex_buffer->set_layout(layout);
    m_glyph_quad.vertex_array->add_vertex_buffer(m_vertex_buffer);
    m_glyph_quad.vertex_array->set_index_buffer(IndexBuffer::create(
        backend, m_glyph_quad.indices.data(), m_glyph_quad.indices.size()));
    m_glyph_quad.vertex_array->unbind();
  }

  void draw_text_sprite(const rendering::TextSprite &sprite, const Font &font) {
    const auto characters = font.characters();

    float current_x = sprite.position.x;
    const float base_y = sprite.position.y;
    auto renderer_api = m_render_target->renderer_api();

    for (const char character : sprite.text) {
      auto glyph_it = characters.find(character);
      if (glyph_it == characters.end()) {
        continue;
      }

      const auto &glyph = glyph_it->second;
      auto texture =
          resource_manager()->get_by_descriptor_id<Texture2D>(glyph.texture_id);
      if (texture == nullptr) {
        continue;
      }

      const float xpos = current_x + glyph.bearing.x * sprite.scale;
      const float ypos =
          base_y - (glyph.size.y - glyph.bearing.y) * sprite.scale;
      const float width = glyph.size.x * sprite.scale;
      const float height = glyph.size.y * sprite.scale;

      m_glyph_quad.vertices[0].position = glm::vec3(xpos, ypos + height, 0.0f);
      m_glyph_quad.vertices[1].position = glm::vec3(xpos, ypos, 0.0f);
      m_glyph_quad.vertices[2].position = glm::vec3(xpos + width, ypos, 0.0f);
      m_glyph_quad.vertices[3].position =
          glm::vec3(xpos + width, ypos + height, 0.0f);

      m_glyph_quad.vertices[0].texture_coordinates = glm::vec2(0.0f, 0.0f);
      m_glyph_quad.vertices[1].texture_coordinates = glm::vec2(0.0f, 1.0f);
      m_glyph_quad.vertices[2].texture_coordinates = glm::vec2(1.0f, 1.0f);
      m_glyph_quad.vertices[3].texture_coordinates = glm::vec2(1.0f, 0.0f);

      renderer_api->bind_texture_2d(texture->renderer_id(), 0);
      m_vertex_buffer->set_data(
          m_glyph_quad.vertices.data(),
          static_cast<uint32_t>(m_glyph_quad.vertices.size() * sizeof(Vertex)));

      m_render_target->renderer_api()->draw_indexed(m_glyph_quad.vertex_array,
                                                    m_glyph_quad.draw_type);

      current_x += static_cast<float>(glyph.advance >> 6) * sprite.scale;
    }
  }

  Framebuffer *m_scene_color = nullptr;
  Ref<Shader> m_shader;
  Ref<VertexBuffer> m_vertex_buffer;
  Mesh m_glyph_quad = Mesh::quad(1.0f);
};

} // namespace astralix
