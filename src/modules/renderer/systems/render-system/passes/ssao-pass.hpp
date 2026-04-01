#pragma once

#include "framebuffer.hpp"
#include "glm/vec2.hpp"
#include "glm/vec3.hpp"
#include "guid.hpp"
#include "log.hpp"
#include "managers/resource-manager.hpp"
#include "managers/scene-manager.hpp"
#include "render-pass.hpp"
#include "resources/descriptors/shader-descriptor.hpp"
#include "resources/mesh.hpp"
#include "resources/texture.hpp"
#include "shaders/engine_shaders_ssao_axsl.hpp"
#include "systems/render-system/mesh-resolution.hpp"
#include "systems/render-system/passes/render-graph-resource.hpp"
#include "systems/render-system/scene-selection.hpp"
#include <array>
#include <random>

#if __has_include(ASTRALIX_ENGINE_BINDINGS_HEADER)
#include ASTRALIX_ENGINE_BINDINGS_HEADER
#define ASTRALIX_HAS_ENGINE_BINDINGS
#endif

namespace astralix {

class SSAOPass : public RenderPass {
public:
  SSAOPass() = default;
  ~SSAOPass() override = default;

  void
  setup(Ref<RenderTarget> render_target, const std::vector<const RenderGraphResource *> &resources) override {
    m_render_target = render_target;

    for (auto resource : resources) {
      switch (resource->desc.type) {
        case RenderGraphResourceType::Framebuffer: {
          if (resource->desc.name == "g_buffer") {
            m_g_buffer = resource->get_framebuffer();
          }

          if (resource->desc.name == "ssao") {
            m_ssao = resource->get_framebuffer();
          }
          break;
        }

        default:
          break;
      }
    }

    if (m_g_buffer == nullptr || m_ssao == nullptr) {
      set_enabled(false);
    }

    m_ssao_kernel.clear();
    std::uniform_real_distribution<float> random_floats(0.0, 1.0);
    std::default_random_engine generator;

    for (unsigned int i = 0; i < 64; ++i) {
      glm::vec3 sample(
          random_floats(generator) * 2.0 - 1.0,
          random_floats(generator) * 2.0 - 1.0,
          random_floats(generator)
      );

      sample = glm::normalize(sample);
      sample *= random_floats(generator);

      float scale = (float)i / 64.0;
      scale = lerp(0.1f, 1.0f, scale * scale);
      sample *= scale;
      m_ssao_kernel.push_back(sample);
    }

    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    for (int i = 0; i < 16; i++) {
      m_ssao_noise.emplace_back(
          dist(generator) * 2.0f - 1.0f,
          dist(generator) * 2.0f - 1.0f,
          0.0f
      );
    }

    auto renderer_api = m_render_target->renderer_api();

    TextureConfig texture_config;
    texture_config.width = 4;
    texture_config.height = 4;
    texture_config.format = TextureFormat::RGB;
    texture_config.buffer = (unsigned char *)m_ssao_noise.data();
    texture_config.parameters = {
        {TextureParameter::WrapS, TextureValue::Linear},
        {TextureParameter::WrapT, TextureValue::Linear},
        {TextureParameter::MagFilter, TextureValue::Nearest},
        {TextureParameter::MinFilter, TextureValue::Nearest},
    };

    Texture2D::create("noise_texture", texture_config);

    resource_manager()->load_from_descriptor<Texture2DDescriptor>(renderer_api->get_backend());

    m_noise_texture = resource_manager()->get_by_descriptor_id<Texture2D>("noise_texture");
  }

  void
  begin(double dt) override {
  }

  void execute(double dt) override {
    ASTRA_PROFILE_N("SSAOPass Update");

#ifdef ASTRALIX_HAS_ENGINE_BINDINGS
    auto scene = SceneManager::get()->get_active_scene();
    if (scene == nullptr) {
      LOG_WARN("[SSAOPass] Skipping execute: no active scene");
      return;
    }

    if (m_g_buffer == nullptr || m_ssao == nullptr) {
      LOG_WARN("[SSAOPass] Skipping execute: required framebuffers are not available");
      return;
    }

    auto &world = scene->world();
    auto camera = rendering::select_main_camera(world);
    if (!camera.has_value()) {
      LOG_WARN("[SSAOPass] Skipping execute: no main camera selected");
      return;
    }

    auto renderer_api = m_render_target->renderer_api();
    const auto backend = renderer_api->get_backend();
    resource_manager()->load_from_descriptors_by_ids<ShaderDescriptor>(
        backend, {"shaders::ssao"}
    );

    auto shader =
        resource_manager()->get_by_descriptor_id<Shader>("shaders::ssao");
    if (shader == nullptr) {
      LOG_WARN(
          "[SSAOPass] Skipping execute: failed to load shaders::ssao"
      );
      return;
    }

    rendering::ensure_mesh_uploaded(m_fullscreen_quad, m_render_target);
    shader->bind();

    using namespace shader_bindings::engine_shaders_ssao_axsl;

    int32_t texture_unit = 0;
    auto color_attachments = m_g_buffer->get_color_attachments();
    for (uint32_t i = 0; i < color_attachments.size(); i++) {
      renderer_api->bind_texture_2d(color_attachments[i], texture_unit + i);
    }

    const auto &ssao_spec = m_ssao->get_specification();
    std::array<glm::vec3, kernel_size> kernel{};
    for (size_t i = 0; i < kernel.size() && i < m_ssao_kernel.size(); ++i) {
      kernel[i] = m_ssao_kernel[i];
    }

    const int32_t noise_slot =
        texture_unit + static_cast<int32_t>(color_attachments.size());
    renderer_api->bind_texture_2d(m_noise_texture->renderer_id(), noise_slot);

    shader->set_all(GBufferParams{
        .g_position = texture_unit,
        .g_normal = texture_unit + 1,
        .g_albedo = texture_unit + 2,
    });
    shader->set_all(CameraParams{
        .view = camera->camera->view_matrix,
        .projection = camera->camera->projection_matrix,
        .position = camera->transform->position,
    });
    shader->set_all(SSAOParams{
        .samples = kernel,
        .kernel_size = kernel_size,
        .radius = k_ssao_radius,
        .bias = k_ssao_bias,
        .noise_scale = glm::vec2(static_cast<float>(ssao_spec.width) / 4.0f, static_cast<float>(ssao_spec.height) / 4.0f),
        .noise_texture = noise_slot,
    });

    m_ssao->bind();
    renderer_api->disable_depth_test();
    renderer_api->draw_indexed(m_fullscreen_quad.vertex_array, m_fullscreen_quad.draw_type);
    renderer_api->enable_depth_test();
    shader->unbind();
    m_ssao->unbind();
#endif
  }

  void end(double dt) override {}

  void cleanup() override {}

  std::string name() const noexcept override { return "SSAOPass"; }

  float lerp(float a, float b, float f) {
    return a + f * (b - a);
  }

private:
  Framebuffer *m_g_buffer = nullptr;
  Framebuffer *m_ssao = nullptr;
  Mesh m_fullscreen_quad = Mesh::quad(1.0f);
  static constexpr float k_ssao_radius = 1.25f;
  static constexpr float k_ssao_bias = 0.025f;
  static constexpr uint32_t kernel_size = 64;

  std::vector<glm::vec3> m_ssao_kernel;
  std::vector<glm::vec3> m_ssao_noise;

  Ref<Texture2D> m_noise_texture;
};

} // namespace astralix
