#pragma once

#include "framebuffer.hpp"
#include "managers/resource-manager.hpp"
#include "render-pass.hpp"
#include "resources/descriptors/shader-descriptor.hpp"
#include "resources/mesh.hpp"
#include "systems/render-system/mesh-resolution.hpp"
#include "systems/render-system/passes/render-graph-resource.hpp"

namespace astralix {

class PostProcessPass : public RenderPass {
public:
  PostProcessPass() = default;
  ~PostProcessPass() override = default;

  void
  setup(Ref<RenderTarget> render_target, const std::vector<const RenderGraphResource *> &resources) override {
    m_render_target = render_target;
    m_bloom = nullptr;

    for (auto resource : resources) {
      if (resource->desc.type != RenderGraphResourceType::Framebuffer) {
        continue;
      }

      if (resource->desc.name == "bloom") {
        m_bloom = resource->get_framebuffer();
      }
    }

    const auto backend = m_render_target->renderer_api()->get_backend();
    resource_manager()->load_from_descriptors_by_ids<ShaderDescriptor>(
        backend, {"shaders::hdr"}
    );
    m_shader = resource_manager()->get_by_descriptor_id<Shader>("shaders::hdr");

    rendering::ensure_mesh_uploaded(m_fullscreen_quad, m_render_target);

    if (render_target->has_msaa_enabled()) {
      auto framebuffer = render_target->framebuffer();
      FramebufferSpecification framebuffer_spec =
          framebuffer->get_specification();
      framebuffer_spec.samples = render_target->msaa().samples;

      m_resolved_framebuffer = Framebuffer::create(backend, framebuffer_spec);
    }

    if (m_shader != nullptr) {
      m_shader->bind();
      m_shader->set_int("screen_texture", 0);
      m_shader->set_int("bloom_texture", 1);
      m_shader->set_float("bloom_strength", 0.12f);
      m_shader->unbind();
    }
  }

  void begin(double dt) override { m_render_target->unbind(); }

  void execute(double dt) override {
    if (m_shader == nullptr || m_bloom == nullptr) {
      return;
    }

    m_render_target->framebuffer()->bind(FramebufferBindType::Default, 0);
    m_render_target->renderer_api()->disable_buffer_testing();
    m_render_target->renderer_api()->clear_color();
    m_render_target->renderer_api()->clear_buffers();

    m_shader->bind();
    bind_screen_texture();
    m_render_target->renderer_api()->bind_texture_2d(
        m_bloom->get_color_attachment_id(), 1
    );
    m_render_target->renderer_api()->draw_indexed(
        m_fullscreen_quad.vertex_array, m_fullscreen_quad.draw_type
    );
    m_shader->unbind();
  }

  void end(double dt) override {}

  void cleanup() override {}

  std::string name() const override { return "PostProcessPass"; }

private:
  void bind_screen_texture() {
    const bool msaa_enabled = m_render_target->has_msaa_enabled();
    if (msaa_enabled && m_resolved_framebuffer != nullptr) {
      resolve_screen_texture();
    }

    const uint32_t screen_texture =
        msaa_enabled && m_resolved_framebuffer != nullptr
            ? m_resolved_framebuffer->get_color_attachment_id()
            : m_render_target->framebuffer()->get_color_attachment_id();

    m_render_target->renderer_api()->bind_texture_2d(screen_texture, 0);
  }

  void resolve_screen_texture() {
    auto framebuffer = m_render_target->framebuffer();

    framebuffer->bind(FramebufferBindType::Read);
    m_resolved_framebuffer->bind(FramebufferBindType::Draw);

    const auto &spec = framebuffer->get_specification();
    framebuffer->blit(spec.width, spec.height);
    framebuffer->unbind();
  }

  Ref<Shader> m_shader;
  Ref<Framebuffer> m_resolved_framebuffer;
  Framebuffer *m_bloom = nullptr;
  Mesh m_fullscreen_quad = Mesh::quad(1.0f);
};

} // namespace astralix
