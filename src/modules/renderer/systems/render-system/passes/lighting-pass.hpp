#pragma once

#include "framebuffer.hpp"
#include "log.hpp"
#include "render-pass.hpp"
#include "systems/render-system/brdf-lut.hpp"
#include "systems/render-system/core/compiled-frame.hpp"
#include "systems/render-system/core/pass-recorder.hpp"
#include "systems/render-system/core/shader-param-recorder.hpp"
#include "systems/render-system/light-frame.hpp"
#include "systems/render-system/passes/render-graph-resource.hpp"
#include "systems/render-system/pbr-default-textures.hpp"
#include "systems/render-system/render-frame.hpp"
#include "targets/render-target.hpp"
#include "trace.hpp"

#include ASTRALIX_ENGINE_BINDINGS_HEADER

#include "glad/glad.h"
#include "renderer-api.hpp"
#include "resources/equirectangular-converter.hpp"

#include <cmath>

namespace astralix {

class LightingPass : public FramePass {
public:
  explicit LightingPass(rendering::ResolvedMeshDraw fullscreen_quad = {})
      : m_fullscreen_quad(std::move(fullscreen_quad)) {}
  ~LightingPass() override = default;

  void setup(PassSetupContext &ctx) override {
    ASTRA_PROFILE_N("LightingPass::setup");
    m_shader = ctx.require_shader("lighting_shader");

    if (m_shader == nullptr) {
      LOG_WARN("[LightingPass] Missing graph dependency: lighting_shader");
      set_enabled(false);
    }
  }

  void record(PassRecordContext &ctx, PassRecorder &recorder) override {
    ASTRA_PROFILE_N("LightingPass::record");
    const auto *scene_frame = ctx.scene();
    if (scene_frame == nullptr || !scene_frame->main_camera.has_value()) {
      return;
    }

    const auto *shadow_map = ctx.find_graph_image("shadow_map");
    const auto *scene_color_resource = ctx.find_graph_image("scene_color");
    const auto *bloom_extract = ctx.find_graph_image("bloom_extract");
    const auto *entity_pick = ctx.find_graph_image("entity_pick");
    const auto *g_position_resource = ctx.find_graph_image("g_position");
    const auto *g_normal_resource = ctx.find_graph_image("g_normal");
    const auto *g_geometric_normal_resource = ctx.find_graph_image("g_geometric_normal");
    const auto *g_albedo_resource = ctx.find_graph_image("g_albedo");
    const auto *g_emissive_resource = ctx.find_graph_image("g_emissive");
    const auto *g_entity_id_resource = ctx.find_graph_image("g_entity_id");
    const auto *ssao_resource = ctx.find_graph_image("ssao_blur");

#define LIGHTING_PASS_REQUIRED_RESOURCES(ENTRY) \
  ENTRY(shadow_map)                             \
  ENTRY(scene_color_resource)                   \
  ENTRY(bloom_extract)                          \
  ENTRY(entity_pick)                            \
  ENTRY(g_position_resource)                    \
  ENTRY(g_normal_resource)                      \
  ENTRY(g_geometric_normal_resource)            \
  ENTRY(g_albedo_resource)                      \
  ENTRY(g_emissive_resource)                    \
  ENTRY(g_entity_id_resource)                   \
  ENTRY(ssao_resource)

#define CHECK_MISSING(var)                     \
  if (!(var)) {                                \
    LOG_WARN("[LightingPass] Missing: " #var); \
    missing = true;                            \
  }

    bool missing = false;
    LIGHTING_PASS_REQUIRED_RESOURCES(CHECK_MISSING)
    if (!m_shader) {
      LOG_WARN("[LightingPass] Missing: lighting_shader");
      missing = true;
    }
    if (!m_fullscreen_quad.vertex_array) {
      LOG_WARN("[LightingPass] Missing: fullscreen_quad.vertex_array");
      missing = true;
    }
    if (!m_fullscreen_quad.index_count) {
      LOG_WARN("[LightingPass] Missing: fullscreen_quad.index_count");
      missing = true;
    }
    if (missing)
      return;

#undef CHECK_MISSING
#undef LIGHTING_PASS_REQUIRED_RESOURCES

    using namespace shader_bindings::engine_shaders_light_axsl;

    auto &frame = ctx.frame();
    const auto extent = ctx.graph_image_extent(*scene_color_resource);

    auto shadow_depth = ctx.register_graph_image(
        "lighting.shadow-map", *shadow_map, ImageAspect::Depth
    );
    auto g_position = ctx.register_graph_image(
        "lighting.g-position", *g_position_resource
    );

    auto g_normal =
        ctx.register_graph_image("lighting.g-normal", *g_normal_resource);

    auto g_geometric_normal =
        ctx.register_graph_image("lighting.g-geometric-normal", *g_geometric_normal_resource);

    auto g_albedo =
        ctx.register_graph_image("lighting.g-albedo", *g_albedo_resource);
    auto g_emissive = ctx.register_graph_image(
        "lighting.g-emissive", *g_emissive_resource
    );
    auto g_entity_id = ctx.register_graph_image(
        "lighting.g-entity-id", *g_entity_id_resource
    );
    auto ssao_blur =
        ctx.register_graph_image("lighting.ssao-blur", *ssao_resource);
    auto scene_color = ctx.register_graph_image(
        "lighting.scene-color", *scene_color_resource
    );
    auto bright_color = ctx.register_graph_image(
        "lighting.scene-bright", *bloom_extract
    );
    auto entity_id = ctx.register_graph_image(
        "lighting.scene-entity-id", *entity_pick
    );

    const auto bindings = frame.register_binding_group(
        make_binding_group_desc(
            "lighting-pass",
            "lighting-pass",
            m_shader,
            0,
            "lighting-pass",
            RenderBindingScope::Pass,
            RenderBindingCachePolicy::Reuse,
            RenderBindingSharing::LocalOnly,
            0,
            RenderBindingStability::FrameLocal
        )
    );

    {
      ASTRA_PROFILE_N("LightingPass::bind_g_buffer");
      frame.add_sampled_image_binding(
          bindings, LightResources::shadow_map.binding_id, ImageViewRef{.image = shadow_depth, .aspect = ImageAspect::Depth}
      );
      frame.add_sampled_image_binding(
          bindings, LightResources::g_position.binding_id, ImageViewRef{.image = g_position}
      );
      frame.add_sampled_image_binding(
          bindings, LightResources::g_normal.binding_id, ImageViewRef{.image = g_normal}

      );
      frame.add_sampled_image_binding(
          bindings, LightResources::g_geometric_normal.binding_id, ImageViewRef{.image = g_geometric_normal}
      );
      frame.add_sampled_image_binding(
          bindings, LightResources::g_albedo.binding_id, ImageViewRef{.image = g_albedo}
      );
      frame.add_sampled_image_binding(
          bindings, LightResources::g_emissive.binding_id, ImageViewRef{.image = g_emissive}
      );
      frame.add_sampled_image_binding(
          bindings, LightResources::g_entity_id.binding_id, ImageViewRef{.image = g_entity_id}
      );
      frame.add_sampled_image_binding(
          bindings, LightResources::g_ssao.binding_id, ImageViewRef{.image = ssao_blur}
      );
    }

    bool ibl_available = false;
    float prefilter_max_lod = 0.0f;

    {
      ASTRA_PROFILE_N("LightingPass::resolve_ibl");
      auto fallback_cubemap = resource_manager()->get_by_descriptor_id<Texture3D>(
          rendering::default_ibl_black_cubemap_id()
      );
      auto real_brdf_lut_texture = resource_manager()->get_by_descriptor_id<Texture2D>(
          rendering::brdf_lut_texture_id()
      );
      auto fallback_brdf_lut_texture =
          resource_manager()->get_by_descriptor_id<Texture2D>(
              rendering::default_brdf_lut_fallback_texture_id()
          );

      if (fallback_cubemap == nullptr || fallback_brdf_lut_texture == nullptr) {
        return;
      }

      auto brdf_lut_texture = real_brdf_lut_texture != nullptr
                                  ? real_brdf_lut_texture
                                  : fallback_brdf_lut_texture;

      Ref<Texture3D> env_source = fallback_cubemap;
      Ref<Texture3D> irradiance_source = fallback_cubemap;
      if (scene_frame->skybox.has_value() &&
          scene_frame->skybox->cubemap != nullptr &&
          real_brdf_lut_texture != nullptr) {
        env_source = scene_frame->skybox->cubemap;
        ibl_available = true;

        irradiance_source = resolve_irradiance_cubemap(env_source);
      }

      const float cubemap_extent = static_cast<float>(
          std::max(env_source->width(), env_source->height())
      );
      if (cubemap_extent > 1.0f) {
        prefilter_max_lod = std::floor(std::log2(cubemap_extent));
      }

      auto env_cubemap = frame.register_texture_cube(
          "lighting.env-cubemap", env_source
      );
      auto irradiance_cubemap = frame.register_texture_cube(
          "lighting.irradiance-cubemap", irradiance_source
      );
      auto brdf_lut_handle = frame.register_texture_2d(
          "lighting.brdf-lut", brdf_lut_texture
      );

      frame.add_sampled_image_binding(
          bindings, LightResources::irradiance_map.binding_id, ImageViewRef{.image = irradiance_cubemap}, CompiledSampledImageTarget::TextureCube
      );
      frame.add_sampled_image_binding(
          bindings, LightResources::prefilter_map.binding_id, ImageViewRef{.image = env_cubemap}, CompiledSampledImageTarget::TextureCube
      );
      frame.add_sampled_image_binding(
          bindings, LightResources::brdf_lut.binding_id, ImageViewRef{.image = brdf_lut_handle}
      );
    }

    {
      ASTRA_PROFILE_N("LightingPass::record_shader_params");
      rendering::record_shader_params(
          frame,
          bindings,
          rendering::build_deferred_light_params(
              scene_frame->light_frame,
              ibl_available,
              prefilter_max_lod
          )
      );
      rendering::record_shader_params(frame, bindings, CameraParams{
                                                           .position = scene_frame->main_camera->position,
                                                           .view_matrix = scene_frame->main_camera->view,
                                                       });
    }

    {
      ASTRA_PROFILE_N("LightingPass::draw");
      RenderPipelineDesc pipeline_desc;
      pipeline_desc.debug_name = "lighting-pass";
      pipeline_desc.depth_stencil.depth_test = false;
      pipeline_desc.depth_stencil.depth_write = false;

      const auto pipeline = frame.register_pipeline(pipeline_desc, m_shader);
      const auto quad_buffer = frame.register_vertex_array(
          "lighting-pass.fullscreen-quad", m_fullscreen_quad.vertex_array
      );

      RenderingInfo info;
      info.debug_name = "lighting-pass";
      info.extent = extent;
      info.color_attachments.push_back(ColorAttachmentRef{
          .view = ImageViewRef{.image = scene_color},
          .load_op = AttachmentLoadOp::Clear,
          .store_op = AttachmentStoreOp::Store,
          .clear_color = {0.0f, 0.0f, 0.0f, 1.0f},
      });
      info.color_attachments.push_back(ColorAttachmentRef{
          .view = ImageViewRef{.image = bright_color},
          .load_op = AttachmentLoadOp::Clear,
          .store_op = AttachmentStoreOp::Store,
          .clear_color = {0.0f, 0.0f, 0.0f, 1.0f},
      });
      info.color_attachments.push_back(ColorAttachmentRef{
          .view = ImageViewRef{.image = entity_id},
          .load_op = AttachmentLoadOp::Clear,
          .store_op = AttachmentStoreOp::Store,
          .clear_color = {0.0f, 0.0f, 0.0f, 0.0f},
      });

      recorder.begin_rendering(info);
      recorder.bind_pipeline(pipeline);
      recorder.bind_binding_group(bindings);
      recorder.bind_vertex_buffer(quad_buffer);
      recorder.bind_index_buffer(quad_buffer, IndexType::Uint32);
      recorder.draw_indexed(DrawIndexedArgs{
          .index_count = m_fullscreen_quad.index_count,
      });
      recorder.end_rendering();
    }
  }

  std::string name() const override { return "LightingPass"; }

private:
  Ref<Texture3D> resolve_irradiance_cubemap(Ref<Texture3D> env_source) {
    uint32_t source_id = env_source->renderer_id();
    if (m_cached_irradiance_source_id == source_id && m_cached_irradiance != nullptr) {
      return m_cached_irradiance;
    }

    uint32_t face_size = env_source->width();
    uint32_t irradiance_resolution = 32;

    CubemapConversionResult source_faces;
    glBindTexture(GL_TEXTURE_CUBE_MAP, source_id);
    for (uint32_t face = 0; face < 6; ++face) {
      source_faces.faces[face].width = face_size;
      source_faces.faces[face].height = face_size;
      source_faces.faces[face].pixels.resize(face_size * face_size * 4);
      glGetTexImage(
          GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0,
          GL_RGBA, GL_FLOAT,
          source_faces.faces[face].pixels.data()
      );
    }
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

    auto convolved = convolve_irradiance_cubemap(source_faces, irradiance_resolution);

    std::vector<std::vector<float>> face_data(6);
    for (uint32_t face = 0; face < 6; ++face) {
      face_data[face] = std::move(convolved.faces[face].pixels);
    }

    static uint32_t irradiance_generation = 0;
    ResourceDescriptorID descriptor_id = "irradiance_cubemap_" + std::to_string(++irradiance_generation);
    Texture3D::create_from_float_buffer(descriptor_id, irradiance_resolution, irradiance_resolution, std::move(face_data));
    resource_manager()->load_from_descriptors_by_ids<Texture3DDescriptor>(
        RendererBackend::OpenGL, {descriptor_id}
    );
    m_cached_irradiance = resource_manager()->get_by_descriptor_id<Texture3D>(descriptor_id);
    m_cached_irradiance_source_id = source_id;

    return m_cached_irradiance;
  }

  Ref<Shader> m_shader = nullptr;
  rendering::ResolvedMeshDraw m_fullscreen_quad{};
  uint32_t m_cached_irradiance_source_id = 0;
  Ref<Texture3D> m_cached_irradiance = nullptr;
};

} // namespace astralix
