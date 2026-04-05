#pragma once

#include "framebuffer.hpp"
#include "glm/vec2.hpp"
#include "log.hpp"
#include "managers/resource-manager.hpp"
#include "render-pass.hpp"
#include "resources/descriptors/shader-descriptor.hpp"
#include "resources/mesh.hpp"
#include "systems/render-system/mesh-resolution.hpp"
#include "systems/render-system/passes/render-graph-resource.hpp"
#include "trace.hpp"

namespace astralix {

class SSAOBlurPass : public RenderPass {
public:
  SSAOBlurPass() = default;
  ~SSAOBlurPass() override = default;

  void setup(
      Ref<RenderTarget> render_target,
      const std::vector<const RenderGraphResource *> &resources
  ) override {
    m_render_target = render_target;
    m_g_buffer = nullptr;
    m_ssao = nullptr;
    m_ssao_blur = nullptr;

    for (auto resource : resources) {
      switch (resource->desc.type) {
        case RenderGraphResourceType::Framebuffer: {
          if (resource->desc.name == "g_buffer") {
            m_g_buffer = resource->get_framebuffer();
          }

          if (resource->desc.name == "ssao") {
            m_ssao = resource->get_framebuffer();
          }

          if (resource->desc.name == "ssao_blur") {
            m_ssao_blur = resource->get_framebuffer();
          }
          break;
        }

        default:
          break;
      }
    }

    if (m_g_buffer == nullptr || m_ssao == nullptr || m_ssao_blur == nullptr) {
      set_enabled(false);
      return;
    }

    rendering::ensure_mesh_uploaded(m_fullscreen_quad, m_render_target);
  }

  void begin(double) override {}

  void execute(double) override {
    ASTRA_PROFILE_N("SSAOBlurPass");
    if (m_g_buffer == nullptr || m_ssao == nullptr || m_ssao_blur == nullptr) {
      LOG_WARN("[SSAOBlurPass] Skipping execute: required framebuffers are not available");
      return;
    }

    auto renderer_api = m_render_target->renderer_api();
    const auto backend = renderer_api->get_backend();
    resource_manager()->load_from_descriptors_by_ids<ShaderDescriptor>(
        backend, {"shaders::ssao_blur"}
    );

    auto shader =
        resource_manager()->get_by_descriptor_id<Shader>("shaders::ssao_blur");
    if (shader == nullptr) {
      LOG_WARN("[SSAOBlurPass] Skipping execute: failed to load shaders::ssao_blur");
      return;
    }

    shader->bind();

    int32_t texture_unit = 0;
    renderer_api->bind_texture_2d(m_ssao->get_color_attachment_id(), texture_unit);
    ++texture_unit;

    const auto &color_attachments = m_g_buffer->get_color_attachments();
    if (color_attachments.size() < 2u) {
      LOG_WARN("[SSAOBlurPass] Skipping execute: g_buffer is missing required attachments");
      shader->unbind();
      return;
    }

    renderer_api->bind_texture_2d(color_attachments[0], texture_unit);
    ++texture_unit;
    renderer_api->bind_texture_2d(color_attachments[1], texture_unit);

    const auto &spec = m_ssao_blur->get_specification();
    shader->set_int("blur.ssao_input", 0);
    shader->set_int("blur.g_position", 1);
    shader->set_int("blur.g_normal", 2);
    shader->set_vec2(
        "blur.texel_size",
        glm::vec2(
            1.0f / static_cast<float>(spec.width),
            1.0f / static_cast<float>(spec.height)
        )
    );

    m_ssao_blur->bind();
    renderer_api->disable_depth_test();
    renderer_api->draw_indexed(
        m_fullscreen_quad.vertex_array, m_fullscreen_quad.draw_type
    );
    renderer_api->enable_depth_test();
    m_ssao_blur->unbind();

    shader->unbind();
  }

  void end(double) override {}
  void cleanup() override {}

  std::string name() const noexcept override { return "SSAOBlurPass"; }

private:
  Ref<RenderTarget> m_render_target = nullptr;
  Framebuffer *m_g_buffer = nullptr;
  Framebuffer *m_ssao = nullptr;
  Framebuffer *m_ssao_blur = nullptr;
  Mesh m_fullscreen_quad = Mesh::quad(1.0f);
};

} // namespace astralix
