#include "systems/render-system/passes/ui-pass.hpp"

#include "components/ui.hpp"
#include "framebuffer.hpp"
#include "managers/resource-manager.hpp"
#include "managers/scene-manager.hpp"
#include "managers/system-manager.hpp"
#include "path.hpp"
#include "renderer-api.hpp"
#include "resources/descriptors/shader-descriptor.hpp"
#include "resources/font.hpp"
#include "resources/svg.hpp"
#include "resources/texture.hpp"
#include "systems/render-system/mesh-resolution.hpp"
#include "systems/render-system/render-system.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>

#if __has_include(ASTRALIX_ENGINE_BINDINGS_HEADER)
#include ASTRALIX_ENGINE_BINDINGS_HEADER
#define ASTRALIX_HAS_ENGINE_BINDINGS
using namespace astralix::shader_bindings;
#endif

namespace astralix {
namespace {

ui::UIRect clamp_clip_rect_to_framebuffer(
    const ui::UIRect &clip_rect,
    uint32_t framebuffer_width,
    uint32_t framebuffer_height
) {
  return ui::intersect_rect(
      clip_rect,
      ui::UIRect{
          .x = 0.0f,
          .y = 0.0f,
          .width = static_cast<float>(framebuffer_width),
          .height = static_cast<float>(framebuffer_height),
      }
  );
}

} // namespace

void UIPass::setup(Ref<RenderTarget> render_target, const std::vector<const RenderGraphResource *> &) {
  m_render_target = render_target;
  rendering::ensure_mesh_uploaded(m_quad, m_render_target);
  ensure_shaders_loaded();

  Shader::create(
      "shaders::ui_polyline",
      "shaders/ui/polyline.axsl"_engine,
      "shaders/ui/polyline.axsl"_engine
  );
  resource_manager()->load_from_descriptors_by_ids<ShaderDescriptor>(
      m_render_target->renderer_api()->get_backend(),
      {"shaders::ui_polyline"}
  );
  m_polyline_shader =
      resource_manager()->get_by_descriptor_id<Shader>("shaders::ui_polyline");
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
  if (!command.has_clip) {
    renderer_api->disable_scissor();
    return;
  }

  if (command.clip_rect.width <= 0.0f || command.clip_rect.height <= 0.0f) {
    renderer_api->enable_scissor();
    renderer_api->set_scissor_rect(0, 0, 0, 0);
    return;
  }

  const auto &spec = m_render_target->framebuffer()->get_specification();
  const ui::UIRect clipped =
      clamp_clip_rect_to_framebuffer(command.clip_rect, spec.width, framebuffer_height);
  if (clipped.width <= 0.0f || clipped.height <= 0.0f) {
    renderer_api->disable_scissor();
    return;
  }

  renderer_api->enable_scissor();

  const uint32_t clip_x = static_cast<uint32_t>(std::max(0.0f, clipped.x));
  const uint32_t clip_height =
      static_cast<uint32_t>(std::max(0.0f, clipped.height));
  const uint32_t clip_width =
      static_cast<uint32_t>(std::max(0.0f, clipped.width));

  const float bottom = static_cast<float>(framebuffer_height) -
                       (clipped.y + clipped.height);
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
  m_image_shader->set(engine_shaders_ui_image_axsl::ImageUniform::sample_flip_y, 0.0f);

  m_render_target->renderer_api()->bind_texture_2d(texture->renderer_id(), 0);
  m_render_target->renderer_api()->draw_indexed(m_quad.vertex_array, m_quad.draw_type);
  m_image_shader->unbind();
#endif
}

void UIPass::draw_svg_image_command(
    const ui::UIDrawCommand &command,
    glm::mat4 projection
) {
  if (command.rect.width <= 0.0f || command.rect.height <= 0.0f) {
    return;
  }

  auto svg = resource_manager()->get_by_descriptor_id<Svg>(command.texture_id);
  if (svg == nullptr || svg->width() <= 0.0f || svg->height() <= 0.0f) {
    return;
  }

  std::vector<ui::UIPolylineVertex> triangle_vertices;
  for (const auto &batch : svg->batches()) {
    triangle_vertices.reserve(triangle_vertices.size() + batch.vertices.size());
    for (const SvgColorVertex &vertex : batch.vertices) {
      const glm::vec2 normalized(
          vertex.position.x / svg->width(),
          vertex.position.y / svg->height()
      );
      const glm::vec2 transformed(
          command.rect.x + normalized.x * command.rect.width,
          command.rect.y + normalized.y * command.rect.height
      );
      triangle_vertices.push_back(
          {transformed, vertex.color * command.tint}
      );
    }
  }

  draw_color_triangles(triangle_vertices, projection);
}

void UIPass::draw_render_image_view_command(
    const ui::UIDrawCommand &command, glm::mat4 projection
) {
  if (m_image_shader == nullptr || command.rect.width <= 0.0f ||
      command.rect.height <= 0.0f || !command.render_image_key.has_value()) {
    return;
  }

  auto *render_system = SystemManager::get()->get_system<RenderSystem>();
  if (render_system == nullptr) {
    return;
  }

  auto resolved =
      render_system->resolve_render_image(*command.render_image_key);
  if (!resolved.has_value() || !resolved->available) {
    return;
  }

  switch (resolved->target) {
    case RenderImageTarget::Texture2D:
#ifdef ASTRALIX_HAS_ENGINE_BINDINGS
      m_image_shader->bind();
      m_image_shader->set(engine_shaders_ui_quad_axsl::CameraUniform::projection, projection);
      m_image_shader->set(engine_shaders_ui_quad_axsl::QuadUniform::rect, glm::vec4(command.rect.x, command.rect.y, command.rect.width, command.rect.height));
      m_image_shader->set(engine_shaders_ui_image_axsl::ImageUniform::tint, command.tint);
      m_image_shader->set(engine_shaders_ui_image_axsl::ImageUniform::texture, 0);
      m_image_shader->set(engine_shaders_ui_image_axsl::ImageUniform::sample_flip_y, 1.0f);

      m_render_target->renderer_api()->bind_texture_2d(
          resolved->renderer_texture_id, 0
      );
      m_render_target->renderer_api()->draw_indexed(
          m_quad.vertex_array, m_quad.draw_type
      );
      m_image_shader->unbind();
#endif
      break;
  }
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
    const ui::UIRect glyph_rect{
        .x = xpos,
        .y = ypos,
        .width = width,
        .height = height,
    };

    if (command.has_clip && !ui::intersects(glyph_rect, command.clip_rect)) {
      current_x += static_cast<float>(glyph.advance >> 6);
      continue;
    }

    m_text_shader->set(
        engine_shaders_ui_quad_axsl::QuadUniform::rect,
        glm::vec4(glyph_rect.x, glyph_rect.y, glyph_rect.width, glyph_rect.height)
    );
    m_render_target->renderer_api()->bind_texture_2d(texture->renderer_id(), 0);
    m_render_target->renderer_api()->draw_indexed(m_quad.vertex_array, m_quad.draw_type);

    current_x += static_cast<float>(glyph.advance >> 6);
  }
#endif

  m_text_shader->unbind();
}

void UIPass::ensure_polyline_resources(size_t required_vertices) {
  if (required_vertices == 0u) {
    return;
  }

  if (m_polyline_vertex_array != nullptr && m_polyline_vertex_buffer != nullptr &&
      required_vertices <= m_polyline_vertex_capacity) {
    return;
  }

  const auto backend = m_render_target->renderer_api()->get_backend();
  m_polyline_vertex_capacity = std::max(required_vertices, size_t(4096u));

  m_polyline_vertex_array = VertexArray::create(backend);
  m_polyline_vertex_buffer = VertexBuffer::create(
      backend,
      static_cast<uint32_t>(m_polyline_vertex_capacity * sizeof(ui::UIPolylineVertex))
  );

  BufferLayout layout(
      {BufferElement(ShaderDataType::Float2, "a_position"),
       BufferElement(ShaderDataType::Float4, "a_color")}
  );
  m_polyline_vertex_buffer->set_layout(layout);
  m_polyline_vertex_array->add_vertex_buffer(m_polyline_vertex_buffer);
  m_polyline_vertex_array->unbind();
}

void UIPass::draw_polyline_command(
    const ui::UIDrawCommand &command, glm::mat4 projection
) {
  if (m_polyline_shader == nullptr || command.polyline_series.empty()) {
    return;
  }

  std::vector<ui::UIPolylineVertex> triangle_vertices;

  for (const auto &series : command.polyline_series) {
    if (series.vertices.size() < 2u) {
      continue;
    }

    const float half_thickness = series.thickness * 0.5f;
    const size_t segment_count = series.vertices.size() - 1u;
    triangle_vertices.reserve(triangle_vertices.size() + segment_count * 6u);

    std::vector<glm::vec2> perpendiculars(series.vertices.size());
    for (size_t i = 0u; i < segment_count; ++i) {
      const glm::vec2 direction = glm::normalize(
          series.vertices[i + 1u].position - series.vertices[i].position
      );
      perpendiculars[i] = glm::vec2(-direction.y, direction.x);
    }
    perpendiculars[segment_count] = perpendiculars[segment_count - 1u];

    std::vector<glm::vec2> miter_normals(series.vertices.size());
    miter_normals[0u] = perpendiculars[0u];
    miter_normals[segment_count] = perpendiculars[segment_count - 1u];

    for (size_t i = 1u; i < segment_count; ++i) {
      glm::vec2 averaged = glm::normalize(perpendiculars[i - 1u] + perpendiculars[i]);
      float dot_product = glm::dot(averaged, perpendiculars[i]);
      if (dot_product < 0.5f) {
        averaged = perpendiculars[i];
        dot_product = 1.0f;
      }
      miter_normals[i] = averaged / dot_product;
    }

    for (size_t i = 0u; i < segment_count; ++i) {
      const auto &point_a = series.vertices[i];
      const auto &point_b = series.vertices[i + 1u];

      const glm::vec2 offset_a = miter_normals[i] * half_thickness;
      const glm::vec2 offset_b = miter_normals[i + 1u] * half_thickness;

      const glm::vec2 top_left = point_a.position + offset_a;
      const glm::vec2 bottom_left = point_a.position - offset_a;
      const glm::vec2 top_right = point_b.position + offset_b;
      const glm::vec2 bottom_right = point_b.position - offset_b;

      triangle_vertices.push_back({top_left, point_a.color});
      triangle_vertices.push_back({bottom_left, point_a.color});
      triangle_vertices.push_back({top_right, point_b.color});

      triangle_vertices.push_back({top_right, point_b.color});
      triangle_vertices.push_back({bottom_left, point_a.color});
      triangle_vertices.push_back({bottom_right, point_b.color});
    }
  }

  if (triangle_vertices.empty()) {
    return;
  }

  draw_color_triangles(triangle_vertices, projection);
}

void UIPass::draw_color_triangles(
    const std::vector<ui::UIPolylineVertex> &triangle_vertices,
    glm::mat4 projection
) {
  if (m_polyline_shader == nullptr || triangle_vertices.empty()) {
    return;
  }

  ensure_polyline_resources(triangle_vertices.size());
  if (m_polyline_vertex_array == nullptr || m_polyline_vertex_buffer == nullptr) {
    return;
  }

  m_polyline_shader->bind();
  m_polyline_shader->set_matrix("camera.projection", projection);
  m_polyline_vertex_buffer->set_data(
      triangle_vertices.data(),
      static_cast<uint32_t>(triangle_vertices.size() * sizeof(ui::UIPolylineVertex))
  );
  m_render_target->renderer_api()->draw_triangles(
      m_polyline_vertex_array,
      static_cast<uint32_t>(triangle_vertices.size())
  );
  m_polyline_shader->unbind();
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
        case ui::DrawCommandType::SvgImage:
          draw_svg_image_command(command, projection);
          break;
        case ui::DrawCommandType::RenderImageView:
          draw_render_image_view_command(command, projection);
          break;
        case ui::DrawCommandType::Text:
          draw_text_command(command, projection);
          break;
        case ui::DrawCommandType::Polyline:
          draw_polyline_command(command, projection);
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
  m_polyline_shader.reset();
  m_polyline_vertex_array.reset();
  m_polyline_vertex_buffer.reset();
  m_polyline_vertex_capacity = 0u;
}

} // namespace astralix
