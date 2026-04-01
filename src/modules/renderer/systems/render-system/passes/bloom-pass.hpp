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

namespace astralix {

class BloomPass : public RenderPass {
public:
  BloomPass() = default;
  ~BloomPass() override = default;

  void setup(
      Ref<RenderTarget> render_target,
      const std::vector<const RenderGraphResource *> &resources
  ) override {
    m_render_target = render_target;
    m_scene_color = nullptr;
    m_bloom = nullptr;

    for (auto resource : resources) {
      if (resource->desc.type != RenderGraphResourceType::Framebuffer) {
        continue;
      }

      if (resource->desc.name == "scene_color") {
        m_scene_color = resource->get_framebuffer();
      }

      if (resource->desc.name == "bloom") {
        m_bloom = resource->get_framebuffer();
      }
    }

    if (m_scene_color == nullptr || m_bloom == nullptr) {
      set_enabled(false);
      return;
    }

    rendering::ensure_mesh_uploaded(m_fullscreen_quad, m_render_target);
  }

  void begin(double) override {}

  void execute(double) override {
    ASTRA_PROFILE_N("BloomPass Update");

    if (m_scene_color == nullptr || m_bloom == nullptr) {
      LOG_WARN("[BloomPass] Skipping execute: required framebuffers are not available");
      return;
    }

    auto renderer_api = m_render_target->renderer_api();
    const auto backend = renderer_api->get_backend();
    resource_manager()->load_from_descriptors_by_ids<ShaderDescriptor>(
        backend, {"shaders::bloom"}
    );

    auto shader =
        resource_manager()->get_by_descriptor_id<Shader>("shaders::bloom");
    if (shader == nullptr) {
      LOG_WARN("[BloomPass] Skipping execute: failed to load shaders::bloom");
      return;
    }

    const auto &scene_color_attachments = m_scene_color->get_color_attachments();
    if (scene_color_attachments.size() < 2u) {
      LOG_WARN("[BloomPass] Skipping execute: scene_color is missing bright attachment");
      return;
    }

    shader->bind();
    renderer_api->bind_texture_2d(scene_color_attachments[1], 0);

    const auto &spec = m_bloom->get_specification();
    shader->set_int("bloom.bright_texture", 0);
    shader->set_vec2(
        "bloom.texel_size",
        glm::vec2(
            1.0f / static_cast<float>(spec.width),
            1.0f / static_cast<float>(spec.height)
        )
    );

    m_bloom->bind();
    renderer_api->disable_depth_test();
    renderer_api->draw_indexed(
        m_fullscreen_quad.vertex_array, m_fullscreen_quad.draw_type
    );
    renderer_api->enable_depth_test();
    m_bloom->unbind();
    shader->unbind();
  }

  void end(double) override {}

  void cleanup() override {}

  std::string name() const noexcept override { return "BloomPass"; }

private:
  Ref<RenderTarget> m_render_target = nullptr;
  Framebuffer *m_scene_color = nullptr;
  Framebuffer *m_bloom = nullptr;
  Mesh m_fullscreen_quad = Mesh::quad(1.0f);
};

} // namespace astralix
