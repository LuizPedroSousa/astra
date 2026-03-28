#include "systems/render-system/passes/ui-pass.hpp"

#include "components/ui.hpp"
#include "framebuffer.hpp"
#include "managers/resource-manager.hpp"
#include "managers/scene-manager.hpp"
#include "path.hpp"
#include "renderer-api.hpp"
#include "resources/descriptors/shader-descriptor.hpp"
#include "resources/font.hpp"
#include "resources/texture.hpp"
#include "shaders/engine_shaders_ui_quad_axsl.hpp"
#include "shaders/engine_shaders_ui_solid_axsl.hpp"
#include "systems/render-system/mesh-resolution.hpp"
#include <algorithm>
#include <cmath>

#if __has_include(ASTRALIX_ENGINE_BINDINGS_HEADER)
#include ASTRALIX_ENGINE_BINDINGS_HEADER
#define ASTRALIX_HAS_ENGINE_BINDINGS
using namespace astralix::shader_bindings;
#endif

namespace astralix {

void UIPass::setup(Ref<RenderTarget> render_target, const std::vector<const RenderGraphResource *> &) {
  m_render_target = render_target;
  rendering::ensure_mesh_uploaded(m_quad, m_render_target);
  ensure_shaders_loaded();
}

void UIPass::ensure_shaders_loaded() {
  const auto backend = m_render_target->renderer_api()->get_backend();
  resource_manager()->load_from_descriptors_by_ids<ShaderDescriptor>(
      backend, {"shaders::ui_solid", "shaders::ui_image", "shaders::ui_text"}
  );

  m_solid_shader =
      resource_manager()->get_by_descriptor_id<Shader>("shaders::ui_solid");
  m_image_shader =
      resource_manager()->get_by_descriptor_id<Shader>("shaders::ui_image");
  m_text_shader =
      resource_manager()->get_by_descriptor_id<Shader>("shaders::ui_text");
}

void UIPass::begin(double) {
  m_render_target->framebuffer()->bind(FramebufferBindType::Default, 0);
}

void UIPass::apply_clip(const ui::UIDrawCommand &command, uint32_t framebuffer_height) {
  auto renderer_api = m_render_target->renderer_api();
  if (!command.has_clip || command.clip_rect.width <= 0.0f ||
      command.clip_rect.height <= 0.0f) {
    renderer_api->disable_scissor();
    return;
  }

  renderer_api->enable_scissor();

  const uint32_t clip_x =
      static_cast<uint32_t>(std::max(0.0f, command.clip_rect.x));
  const uint32_t clip_height =
      static_cast<uint32_t>(std::max(0.0f, command.clip_rect.height));
  const uint32_t clip_width =
      static_cast<uint32_t>(std::max(0.0f, command.clip_rect.width));

  const float bottom = static_cast<float>(framebuffer_height) -
                       (command.clip_rect.y + command.clip_rect.height);
  const uint32_t clip_y = static_cast<uint32_t>(std::max(0.0f, bottom));

  renderer_api->set_scissor_rect(clip_x, clip_y, clip_width, clip_height);
}

void UIPass::draw_rect_command(const ui::UIDrawCommand &command, glm::mat4 projection) {
  if (m_solid_shader == nullptr || command.rect.width <= 0.0f ||
      command.rect.height <= 0.0f) {
    return;
  }
#ifdef ASTRALIX_HAS_ENGINE_BINDINGS
  m_solid_shader->bind();
  m_solid_shader->set(engine_shaders_ui_quad_axsl::CameraUniform::projection, projection);
  m_solid_shader->set(engine_shaders_ui_quad_axsl::QuadUniform::rect, glm::vec4(command.rect.x, command.rect.y, command.rect.width, command.rect.height));
  m_solid_shader->set(engine_shaders_ui_solid_axsl::SolidUniform::fill_color, command.color);
  m_solid_shader->set(engine_shaders_ui_solid_axsl::SolidBorderUniform::color, command.border_color);
  m_solid_shader->set(engine_shaders_ui_solid_axsl::SolidBorderUniform::width, command.border_width);
  m_solid_shader->set(engine_shaders_ui_solid_axsl::SolidBorderUniform::radius, command.border_radius);

  m_render_target->renderer_api()->draw_indexed(m_quad.vertex_array, m_quad.draw_type);
  m_solid_shader->unbind();
#endif
}

void UIPass::draw_image_command(const ui::UIDrawCommand &command, glm::mat4 projection) {
  if (m_image_shader == nullptr || command.rect.width <= 0.0f ||
      command.rect.height <= 0.0f) {
    return;
  }

  auto texture =
      resource_manager()->get_by_descriptor_id<Texture2D>(command.texture_id);
  if (texture == nullptr) {
    return;
  }

#ifdef ASTRALIX_HAS_ENGINE_BINDINGS
  m_image_shader->bind();
  m_image_shader->set(engine_shaders_ui_quad_axsl::CameraUniform::projection, projection);
  m_image_shader->set(engine_shaders_ui_quad_axsl::QuadUniform::rect, glm::vec4(command.rect.x, command.rect.y, command.rect.width, command.rect.height));
  m_image_shader->set(engine_shaders_ui_image_axsl::ImageUniform::tint, command.tint);
  m_image_shader->set(engine_shaders_ui_image_axsl::ImageUniform::texture, 0);

  m_render_target->renderer_api()->bind_texture_2d(texture->renderer_id(), 0);
  m_render_target->renderer_api()->draw_indexed(m_quad.vertex_array, m_quad.draw_type);
  m_image_shader->unbind();
#endif
}

void UIPass::draw_text_command(const ui::UIDrawCommand &command, glm::mat4 projection) {
  if (m_text_shader == nullptr || command.text.empty() ||
      command.font_id.empty()) {
    return;
  }

  auto font = resource_manager()->get_by_descriptor_id<Font>(command.font_id);
  if (font == nullptr) {
    return;
  }

  const uint32_t font_size =
      static_cast<uint32_t>(std::max(1.0f, std::round(command.font_size)));
  const auto &glyphs = font->characters(font_size);
  const float baseline_y = command.text_origin.y + font->ascent(font_size);

  float current_x = command.text_origin.x;
#ifdef ASTRALIX_HAS_ENGINE_BINDINGS
  m_text_shader->bind();
  m_text_shader->set(engine_shaders_ui_quad_axsl::CameraUniform::projection, projection);
  m_text_shader->set(engine_shaders_ui_text_axsl::TextUniform::glyph, 0);
  m_text_shader->set(engine_shaders_ui_text_axsl::TextUniform::color, command.color);

  for (const char character : command.text) {
    auto glyph_it = glyphs.find(character);
    if (glyph_it == glyphs.end()) {
      continue;
    }

    const auto &glyph = glyph_it->second;
    auto texture =
        resource_manager()->get_by_descriptor_id<Texture2D>(glyph.texture_id);
    if (texture == nullptr) {
      continue;
    }

    const float xpos = current_x + static_cast<float>(glyph.bearing.x);
    const float ypos = baseline_y - static_cast<float>(glyph.bearing.y);
    const float width = static_cast<float>(glyph.size.x);
    const float height = static_cast<float>(glyph.size.y);

    m_text_shader->set(engine_shaders_ui_quad_axsl::QuadUniform::rect, glm::vec4(xpos, ypos, width, height));
    m_render_target->renderer_api()->bind_texture_2d(texture->renderer_id(), 0);
    m_render_target->renderer_api()->draw_indexed(m_quad.vertex_array, m_quad.draw_type);

    current_x += static_cast<float>(glyph.advance >> 6);
  }
#endif

  m_text_shader->unbind();
}

void UIPass::execute(double) {
  auto *scene = SceneManager::get()->get_active_scene();
  if (scene == nullptr) {
    return;
  }

  auto &world = scene->world();
  auto renderer_api = m_render_target->renderer_api();
  const auto &spec = m_render_target->framebuffer()->get_specification();
  const glm::mat4 projection =
      glm::ortho(0.0f, static_cast<float>(spec.width), static_cast<float>(spec.height), 0.0f);

  renderer_api->disable_depth_test();
  renderer_api->disable_depth_write();
  renderer_api->enable_blend();
  renderer_api->set_blend_func(RendererAPI::BlendFactor::SrcAlpha, RendererAPI::BlendFactor::OneMinusSrcAlpha);

  std::vector<rendering::UIRoot *> roots;
  world.each<rendering::UIRoot>([&](EntityID entity_id,
                                    rendering::UIRoot &root) {
    if (!world.active(entity_id) || !root.visible || root.document == nullptr) {
      return;
    }

    roots.push_back(&root);
  });

  std::sort(roots.begin(), roots.end(), [](const rendering::UIRoot *lhs, const rendering::UIRoot *rhs) {
    return lhs->sort_order < rhs->sort_order;
  });

  for (const rendering::UIRoot *root : roots) {
    for (const auto &command : root->document->draw_list().commands) {
      apply_clip(command, spec.height);

      switch (command.type) {
        case ui::DrawCommandType::Rect:
          draw_rect_command(command, projection);
          break;
        case ui::DrawCommandType::Image:
          draw_image_command(command, projection);
          break;
        case ui::DrawCommandType::Text:
          draw_text_command(command, projection);
          break;
      }
    }
  }

  renderer_api->disable_scissor();
  renderer_api->disable_blend();
  renderer_api->enable_depth_write();
  renderer_api->enable_depth_test();
}

void UIPass::end(double) {}

void UIPass::cleanup() {
  m_solid_shader.reset();
  m_image_shader.reset();
  m_text_shader.reset();
  m_quad.vertex_array.reset();
}

} // namespace astralix
