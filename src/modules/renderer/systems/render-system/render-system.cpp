#include "render-system.hpp"
#include "assert.hpp"
#include "components/lens-flare.hpp"
#include "events/event-scheduler.hpp"
#include "framebuffer.hpp"
#include "log.hpp"
#include "managers/debug-draw-store.hpp"
#include "managers/path-manager.hpp"
#include "managers/resource-manager.hpp"
#include "managers/scene-manager.hpp"
#include "managers/window-manager.hpp"
#include "module-api.h"
#include "path.hpp"
#include "resources/descriptors/font-descriptor.hpp"
#include "resources/descriptors/material-descriptor.hpp"
#include "resources/descriptors/model-descriptor.hpp"
#include "resources/descriptors/shader-descriptor.hpp"
#include "resources/descriptors/svg-descriptor.hpp"
#include "resources/descriptors/texture-descriptor.hpp"
#include "resources/shader.hpp"
#include "resources/texture.hpp"
#include "systems/job-system/job-system.hpp"
#include "systems/render-system/brdf-lut.hpp"
#include "systems/render-system/builtin-pass-type.hpp"
#include "systems/render-system/mesh-resolution.hpp"
#include "systems/render-system/passes/bloom-pass.hpp"
#include "systems/render-system/passes/chromatic-aberration-composite-pass.hpp"
#include "systems/render-system/passes/chromatic-aberration-pass.hpp"
#include "systems/render-system/passes/debug-draw-pass.hpp"
#include "systems/render-system/passes/debug-pass.hpp"
#include "systems/render-system/passes/editor-gizmo-pass.hpp"
#include "systems/render-system/passes/entity-pick-readback-pass.hpp"
#include "systems/render-system/passes/eye-adaptation-average-pass.hpp"
#include "systems/render-system/passes/eye-adaptation-histogram-pass.hpp"
#include "systems/render-system/passes/depth-of-field-composite-pass.hpp"
#include "systems/render-system/passes/depth-of-field-pass.hpp"
#include "systems/render-system/passes/film-grain-composite-pass.hpp"
#include "systems/render-system/passes/film-grain-pass.hpp"
#include "systems/render-system/passes/forward-pass.hpp"
#include "systems/render-system/passes/gbuffer-blend-pass.hpp"
#include "systems/render-system/passes/geometry-pass.hpp"
#include "systems/render-system/passes/god-rays-pass.hpp"
#include "systems/render-system/passes/god-rays-composite-pass.hpp"
#include "systems/render-system/passes/cas-composite-pass.hpp"
#include "systems/render-system/passes/cas-pass.hpp"
#include "systems/render-system/passes/taa-composite-pass.hpp"
#include "systems/render-system/passes/taa-resolve-pass.hpp"
#include "systems/render-system/passes/taa-history-store-pass.hpp"
#include "systems/render-system/passes/grid-pass.hpp"
#include "systems/render-system/passes/lens-flare-composite-pass.hpp"
#include "systems/render-system/passes/lens-flare-pass.hpp"
#include "systems/render-system/passes/lighting-pass.hpp"
#include "systems/render-system/passes/motion-blur-composite-pass.hpp"
#include "systems/render-system/passes/motion-blur-pass.hpp"
#include "systems/render-system/passes/navigation-gizmo-pass.hpp"
#include "systems/render-system/passes/post-process-pass.hpp"
#include "systems/render-system/passes/render-graph-builder.hpp"
#include "systems/render-system/passes/shadow-pass.hpp"
#include "systems/render-system/passes/skybox-pass.hpp"
#include "systems/render-system/passes/ssao-blur-pass.hpp"
#include "systems/render-system/passes/ssao-pass.hpp"
#include "systems/render-system/passes/ssgi-blur-pass.hpp"
#include "systems/render-system/passes/ssgi-composite-pass.hpp"
#include "systems/render-system/passes/ssgi-history-store-pass.hpp"
#include "systems/render-system/passes/ssgi-pass.hpp"
#include "systems/render-system/passes/ssgi-temporal-pass.hpp"
#include "systems/render-system/passes/ssr-blur-pass.hpp"
#include "systems/render-system/passes/ssr-composite-pass.hpp"
#include "systems/render-system/passes/ssr-pass.hpp"
#include "systems/render-system/passes/terrain-pass.hpp"
#include "systems/render-system/passes/ui-pass.hpp"
#include "systems/render-system/passes/vignette-composite-pass.hpp"
#include "systems/render-system/passes/vignette-pass.hpp"
#include "systems/render-system/passes/volumetric-blur-pass.hpp"
#include "systems/render-system/passes/volumetric-composite-pass.hpp"
#include "systems/render-system/passes/volumetric-fog-pass.hpp"
#include "systems/render-system/passes/volumetric-history-store-pass.hpp"
#include "systems/render-system/passes/volumetric-temporal-pass.hpp"
#include "systems/render-system/pbr-default-textures.hpp"
#include "systems/render-system/scene-extraction.hpp"
#include "targets/render-target.hpp"
#include "trace.hpp"
#include <algorithm>
#include <array>
#include <limits>
#include <random>
#include <string_view>
#include <unordered_map>

namespace astralix {

namespace {

using enum BuiltinPassType;

constexpr uint32_t k_invalid_resource_index =
    std::numeric_limits<uint32_t>::max();

const std::array<unsigned char, 4 * 4 * 3> &ssao_noise_seed() {
  static const auto k_seed = [] {
    std::array<unsigned char, 4 * 4 * 3> seed{};
    std::mt19937 generator(1337u);
    std::uniform_int_distribution<int> distribution(0, 255);

    for (auto &value : seed) {
      value = static_cast<unsigned char>(distribution(generator));
    }

    return seed;
  }();

  return k_seed;
}

uint32_t resolve_window_relative_extent(uint32_t extent, float scale) {
  return std::max(1u, static_cast<uint32_t>(extent * scale));
}

ImageFormat image_format_from_manifest(std::string_view format) {
  if (format == "rgba8") {
    return ImageFormat::RGBA8;
  }

  if (format == "rgba16f") {
    return ImageFormat::RGBA16F;
  }

  if (format == "rgba32f") {
    return ImageFormat::RGBA32F;
  }

  if (format == "r32i" || format == "r32ui") {
    return ImageFormat::R32I;
  }

  if (format == "depth24stencil8") {
    return ImageFormat::Depth24Stencil8;
  }

  if (format == "depth32f") {
    return ImageFormat::Depth32F;
  }

  ASTRA_EXCEPTION("Unknown render graph image format: ", format);
}

ImageUsage image_usage_from_manifest(std::string_view usage) {
  if (usage == "sampled") {
    return ImageUsage::Sampled;
  }

  if (usage == "color_attachment") {
    return ImageUsage::ColorAttachment;
  }

  if (usage == "depth_stencil_attachment") {
    return ImageUsage::DepthStencilAttachment;
  }

  if (usage == "transfer_src") {
    return ImageUsage::TransferSrc;
  }

  if (usage == "transfer_dst") {
    return ImageUsage::TransferDst;
  }

  if (usage == "readback") {
    return ImageUsage::Readback;
  }

  ASTRA_EXCEPTION("Unknown render graph image usage: ", usage);
}

ImageUsage image_usage_mask_from_manifest(
    const std::vector<std::string> &usages
) {
  ImageUsage mask = ImageUsage::None;
  for (const auto &usage : usages) {
    mask |= image_usage_from_manifest(usage);
  }
  return mask;
}

RenderUsage render_usage_from_manifest(std::string_view usage) {
  if (usage == "color_attachment_write") {
    return RenderUsage::ColorAttachmentWrite;
  }

  if (usage == "depth_attachment_write" ||
      usage == "depth_stencil_attachment_write") {
    return RenderUsage::DepthAttachmentWrite;
  }

  if (usage == "depth_attachment_read" ||
      usage == "depth_stencil_attachment_read") {
    return RenderUsage::DepthAttachmentRead;
  }

  if (usage == "sampled_read") {
    return RenderUsage::SampledRead;
  }

  if (usage == "storage_read") {
    return RenderUsage::StorageRead;
  }

  if (usage == "storage_write") {
    return RenderUsage::StorageWrite;
  }

  if (usage == "resolve_src") {
    return RenderUsage::ResolveSrc;
  }

  if (usage == "resolve_dst") {
    return RenderUsage::ResolveDst;
  }

  if (usage == "transfer_src") {
    return RenderUsage::TransferSrc;
  }

  if (usage == "transfer_dst") {
    return RenderUsage::TransferDst;
  }

  if (usage == "present") {
    return RenderUsage::Present;
  }

  ASTRA_EXCEPTION("Unknown render graph pass usage: ", usage);
}

ImageAspect image_aspect_from_manifest(std::string_view aspect) {
  if (aspect == "color0") {
    return ImageAspect::Color0;
  }

  if (aspect == "color1") {
    return ImageAspect::Color1;
  }

  if (aspect == "color2") {
    return ImageAspect::Color2;
  }

  if (aspect == "color3") {
    return ImageAspect::Color3;
  }

  if (aspect == "depth") {
    return ImageAspect::Depth;
  }

  if (aspect == "stencil") {
    return ImageAspect::Stencil;
  }

  ASTRA_EXCEPTION("Unknown render graph image aspect: ", aspect);
}

RenderGraphResourceLifetime lifetime_from_manifest(
    std::string_view lifetime
) {
  if (lifetime.empty() || lifetime == "transient") {
    return RenderGraphResourceLifetime::Transient;
  }

  if (lifetime == "persistent") {
    return RenderGraphResourceLifetime::Persistent;
  }

  ASTRA_EXCEPTION("Unknown render graph lifetime: ", lifetime);
}

uint32_t resource_index_or_invalid(
    const std::unordered_map<std::string, uint32_t> &resource_indices,
    std::string_view resource_name
) {
  auto it = resource_indices.find(std::string(resource_name));
  if (it == resource_indices.end()) {
    return k_invalid_resource_index;
  }
  return it->second;
}

uint32_t require_resource_index(
    const std::unordered_map<std::string, uint32_t> &resource_indices,
    std::string_view resource_name,
    std::string_view pass_id
) {
  auto it = resource_indices.find(std::string(resource_name));
  ASTRA_ENSURE(
      it == resource_indices.end(),
      "Render graph pass '",
      pass_id,
      "' references unknown resource '",
      resource_name,
      "'"
  );
  return it->second;
}

bool is_ssao_resolution_overridable_resource(std::string_view resource_name) {
  return resource_name == "ssao" || resource_name == "ssao_blur";
}

bool is_ssgi_resolution_overridable_resource(std::string_view resource_name) {
  return resource_name == "ssgi" || resource_name == "ssgi_blur" ||
         resource_name == "ssgi_temporal" ||
         resource_name == "ssgi_history";
}

RenderGraphSizeConfig effective_resource_size(
    const RenderGraphResourceConfig &resource_config,
    const RenderSystemConfig &system_config
) {
  RenderGraphSizeConfig size = resource_config.size;
  if (size.mode != RenderGraphSizeMode::WindowRelative) {
    return size;
  }

  if (system_config.ssao.full_resolution &&
      is_ssao_resolution_overridable_resource(resource_config.name)) {
    size.scale_x = 1.0f;
    size.scale_y = 1.0f;
  }

  if (system_config.ssgi.full_resolution &&
      is_ssgi_resolution_overridable_resource(resource_config.name)) {
    size.scale_x = 1.0f;
    size.scale_y = 1.0f;
  }
  return size;
}

Scope<FramePass> create_manifest_frame_pass(
    BuiltinPassType pass_type,
    const rendering::ResolvedMeshDraw &skybox_cube,
    const rendering::ResolvedMeshDraw &fullscreen_quad,
    EyeAdaptationState *eye_adaptation_state,
    rendering::EntityPickReadbackRequest *entity_pick_request
) {
  switch (pass_type) {
    case Shadow:
      return create_scope<ShadowPass>();
    case Forward:
      return create_scope<ForwardPass>();
    case Geometry:
      return create_scope<GeometryPass>();
    case GBufferBlend:
      return create_scope<GBufferBlendPass>();
    case SSAO:
      return create_scope<SSAOPass>(fullscreen_quad);
    case SSAOBlur:
      return create_scope<SSAOBlurPass>(fullscreen_quad);
    case Lighting:
      return create_scope<LightingPass>(fullscreen_quad);
    case SSR:
      return create_scope<SSRPass>(fullscreen_quad);
    case SSRBlur:
      return create_scope<SSRBlurPass>(fullscreen_quad);
    case SSRComposite:
      return create_scope<SSRCompositePass>(fullscreen_quad);
    case SSGI:
      return create_scope<SSGIPass>(fullscreen_quad);
    case SSGIBlur:
      return create_scope<SSGIBlurPass>(fullscreen_quad);
    case SSGITemporal:
      return create_scope<SSGITemporalPass>(fullscreen_quad);
    case SSGIComposite:
      return create_scope<SSGICompositePass>(fullscreen_quad);
    case SSGIHistoryStore:
      return create_scope<SSGIHistoryStorePass>(fullscreen_quad);
    case DebugGBuffer:
      return create_scope<DebugGBufferPass>(fullscreen_quad);
    case Terrain:
      return create_scope<TerrainRenderPass>();
    case Skybox:
      return create_scope<SkyboxPass>(skybox_cube);
    case Grid:
      return create_scope<GridPass>(fullscreen_quad);
    case DebugOverlay:
      return create_scope<DebugOverlayPass>(fullscreen_quad);
    case EntityPickReadback:
      return create_scope<EntityPickReadbackPass>(entity_pick_request);
    case Bloom:
      return create_scope<BloomPass>(fullscreen_quad);
    case EyeAdaptationHistogram:
      return create_scope<EyeAdaptationHistogramPass>();
    case EyeAdaptationAverage:
      return create_scope<EyeAdaptationAveragePass>(eye_adaptation_state);
    case DebugDraw:
      return create_scope<DebugDrawPass>();
    case EditorGizmo:
      return create_scope<EditorGizmoPass>();
    case NavigationGizmo:
      return create_scope<NavigationGizmoPass>();
    case PostProcess:
      return create_scope<PostProcessPass>(fullscreen_quad, eye_adaptation_state);
    case UI:
      return create_scope<UIPass>(fullscreen_quad);
    case VolumetricFog:
      return create_scope<VolumetricFogPass>(fullscreen_quad);
    case VolumetricBlurH:
      return create_scope<VolumetricBlurPass>(VolumetricBlurPass::Direction::Horizontal, fullscreen_quad);
    case VolumetricBlurV:
      return create_scope<VolumetricBlurPass>(VolumetricBlurPass::Direction::Vertical, fullscreen_quad);
    case VolumetricTemporal:
      return create_scope<VolumetricTemporalPass>(fullscreen_quad);
    case VolumetricHistoryStore:
      return create_scope<VolumetricHistoryStorePass>(fullscreen_quad);
    case VolumetricComposite:
      return create_scope<VolumetricCompositePass>(fullscreen_quad);
    case LensFlare:
      return create_scope<LensFlarePass>(fullscreen_quad);
    case LensFlareComposite:
      return create_scope<LensFlareCompositePass>(fullscreen_quad);
    case MotionBlur:
      return create_scope<MotionBlurPass>(fullscreen_quad);
    case MotionBlurComposite:
      return create_scope<MotionBlurCompositePass>(fullscreen_quad);
    case ChromaticAberration:
      return create_scope<ChromaticAberrationPass>(fullscreen_quad);
    case ChromaticAberrationComposite:
      return create_scope<ChromaticAberrationCompositePass>(fullscreen_quad);
    case Vignette:
      return create_scope<VignettePass>(fullscreen_quad);
    case VignetteComposite:
      return create_scope<VignetteCompositePass>(fullscreen_quad);
    case FilmGrain:
      return create_scope<FilmGrainPass>(fullscreen_quad);
    case FilmGrainComposite:
      return create_scope<FilmGrainCompositePass>(fullscreen_quad);
    case DepthOfField:
      return create_scope<DepthOfFieldPass>(fullscreen_quad);
    case DepthOfFieldComposite:
      return create_scope<DepthOfFieldCompositePass>(fullscreen_quad);
    case GodRays:
      return create_scope<GodRaysPass>(fullscreen_quad);
    case GodRaysComposite:
      return create_scope<GodRaysCompositePass>(fullscreen_quad);
    case TAAResolve:
      return create_scope<TAAResolvePass>(fullscreen_quad);
    case TAAComposite:
      return create_scope<TAACompositePass>(fullscreen_quad);
    case TAAHistoryStore:
      return create_scope<TAAHistoryStorePass>(fullscreen_quad);
    case CAS:
      return create_scope<CASPass>(fullscreen_quad);
    case CASComposite:
      return create_scope<CASCompositePass>(fullscreen_quad);
  }
  ASTRA_EXCEPTION("Unsupported render graph pass type");
}

void apply_manifest_dependency(
    PassBuilder &builder,
    const RenderGraphPassDependencyConfig &dependency
) {
  switch (dependency.kind) {
    case RenderGraphPassDependencyKind::Shader:
      builder.use_shader(dependency.slot, dependency.descriptor_id);
      return;
    case RenderGraphPassDependencyKind::Texture2D:
      builder.use_texture_2d(dependency.slot, dependency.descriptor_id);
      return;
    case RenderGraphPassDependencyKind::Texture3D:
      builder.use_texture_3d(dependency.slot, dependency.descriptor_id);
      return;
    case RenderGraphPassDependencyKind::Material:
      builder.use_material(dependency.slot, dependency.descriptor_id);
      return;
    case RenderGraphPassDependencyKind::Model:
      builder.use_model(dependency.slot, dependency.descriptor_id);
      return;
    case RenderGraphPassDependencyKind::Font:
      builder.use_font(dependency.slot, dependency.descriptor_id);
      return;
    case RenderGraphPassDependencyKind::Svg:
      builder.use_svg(dependency.slot, dependency.descriptor_id);
      return;
    case RenderGraphPassDependencyKind::AudioClip:
      builder.use_audio_clip(dependency.slot, dependency.descriptor_id);
      return;
    case RenderGraphPassDependencyKind::TerrainRecipe:
      builder.use_terrain_recipe(dependency.slot, dependency.descriptor_id);
      return;
  }
}

void apply_builtin_exports(
    PassBuilder &builder,
    BuiltinPassType pass_type,
    const std::unordered_map<std::string, uint32_t> &resource_indices
) {
  switch (pass_type) {
    case Shadow: {
      const auto shadow_map =
          resource_index_or_invalid(resource_indices, "shadow_map");
      if (shadow_map != k_invalid_resource_index) {
        builder.export_image(make_shadow_map_render_image_export(shadow_map));
      }

      return;
    }

    case Geometry: {
      const auto g_position =
          resource_index_or_invalid(resource_indices, "g_position");
      const auto g_normal =
          resource_index_or_invalid(resource_indices, "g_normal");
      const auto g_geometric_normal =
          resource_index_or_invalid(resource_indices, "g_geometric_normal");
      const auto g_albedo =
          resource_index_or_invalid(resource_indices, "g_albedo");
      const auto g_emissive =
          resource_index_or_invalid(resource_indices, "g_emissive");
      const auto g_velocity =
          resource_index_or_invalid(resource_indices, "g_velocity");

      if (g_position != k_invalid_resource_index) {
        builder.export_image(make_g_buffer_render_image_export(
            GBufferAspect::Position, g_position
        ));
      }
      if (g_normal != k_invalid_resource_index) {
        builder.export_image(make_g_buffer_render_image_export(
            GBufferAspect::Normal, g_normal
        ));
      }
      if (g_geometric_normal != k_invalid_resource_index) {
        builder.export_image(make_g_buffer_render_image_export(
            GBufferAspect::GeometricNormal, g_geometric_normal
        ));
      }
      if (g_albedo != k_invalid_resource_index) {
        builder.export_image(make_g_buffer_render_image_export(
            GBufferAspect::Albedo, g_albedo
        ));
      }
      if (g_emissive != k_invalid_resource_index) {
        builder.export_image(make_g_buffer_render_image_export(
            GBufferAspect::Emissive, g_emissive
        ));
      }
      if (g_velocity != k_invalid_resource_index) {
        builder.export_image(make_velocity_render_image_export(g_velocity));
      }
      return;
    }

    case SSAO: {
      const auto ssao = resource_index_or_invalid(resource_indices, "ssao");
      if (ssao != k_invalid_resource_index) {
        builder.export_image(make_ssao_render_image_export(ssao));
      }
      return;
    }

    case SSAOBlur: {
      const auto ssao_blur =
          resource_index_or_invalid(resource_indices, "ssao_blur");
      if (ssao_blur != k_invalid_resource_index) {
        builder.export_image(make_ssao_blur_render_image_export(ssao_blur));
      }
      return;
    }

    case SSR: {
      const auto ssr = resource_index_or_invalid(resource_indices, "ssr");
      if (ssr != k_invalid_resource_index) {
        builder.export_image(make_ssr_render_image_export(ssr));
      }
      return;
    }

    case SSRBlur: {
      const auto ssr_blur =
          resource_index_or_invalid(resource_indices, "ssr_blur");
      if (ssr_blur != k_invalid_resource_index) {
        builder.export_image(make_ssr_blur_render_image_export(ssr_blur));
      }
      return;
    }

    case SSGI: {
      const auto ssgi = resource_index_or_invalid(resource_indices, "ssgi");
      if (ssgi != k_invalid_resource_index) {
        builder.export_image(make_ssgi_render_image_export(ssgi));
      }
      return;
    }

    case SSGIBlur: {
      const auto ssgi_blur =
          resource_index_or_invalid(resource_indices, "ssgi_blur");
      if (ssgi_blur != k_invalid_resource_index) {
        builder.export_image(make_ssgi_blur_render_image_export(ssgi_blur));
      }
      return;
    }

    case SSGITemporal: {
      const auto ssgi_temporal =
          resource_index_or_invalid(resource_indices, "ssgi_temporal");
      if (ssgi_temporal != k_invalid_resource_index) {
        builder.export_image(
            make_ssgi_temporal_render_image_export(ssgi_temporal)
        );
      }
      return;
    }

    case SSGIHistoryStore: {
      const auto ssgi_history =
          resource_index_or_invalid(resource_indices, "ssgi_history");
      if (ssgi_history != k_invalid_resource_index) {
        builder.export_image(
            make_ssgi_history_render_image_export(ssgi_history)
        );
      }
      return;
    }

    case GodRays: {
      const auto god_rays =
          resource_index_or_invalid(resource_indices, "god_rays");
      if (god_rays != k_invalid_resource_index) {
        builder.export_image(make_god_rays_render_image_export(god_rays));
      }
      return;
    }

    case TAAResolve: {
      const auto taa_output =
          resource_index_or_invalid(resource_indices, "taa_output");
      if (taa_output != k_invalid_resource_index) {
        builder.export_image(make_taa_output_render_image_export(taa_output));
      }
      return;
    }

    case Forward:
    case Lighting:
    case SSGIComposite:
    case SSRComposite:
    case VolumetricComposite:
    case LensFlareComposite:
    case GodRaysComposite:
    case MotionBlurComposite:
    case DepthOfFieldComposite:
    case TAAComposite:
    case DebugDraw:
    case EditorGizmo:
    case NavigationGizmo: {
      const auto scene_color =
          resource_index_or_invalid(resource_indices, "scene_color");
      if (scene_color != k_invalid_resource_index) {
        builder.export_image(make_scene_color_render_image_export(scene_color));
      }
      return;
    }

    case TAAHistoryStore: {
      return;
    }

    case MotionBlur: {
      const auto motion_blur =
          resource_index_or_invalid(resource_indices, "motion_blur");
      if (motion_blur != k_invalid_resource_index) {
        builder.export_image(make_motion_blur_render_image_export(motion_blur));
      }
      return;
    }

    case DepthOfField: {
      const auto depth_of_field =
          resource_index_or_invalid(resource_indices, "depth_of_field");
      if (depth_of_field != k_invalid_resource_index) {
        builder.export_image(
            make_depth_of_field_render_image_export(depth_of_field)
        );
      }
      return;
    }

    case CAS: {
      const auto cas =
          resource_index_or_invalid(resource_indices, "cas");
      if (cas != k_invalid_resource_index) {
        builder.export_image(make_cas_render_image_export(cas));
      }
      return;
    }

    case ChromaticAberration: {
      const auto chromatic_aberration =
          resource_index_or_invalid(resource_indices, "chromatic_aberration");
      if (chromatic_aberration != k_invalid_resource_index) {
        builder.export_image(
            make_chromatic_aberration_render_image_export(chromatic_aberration)
        );
      }
      return;
    }

    case Vignette: {
      const auto vignette =
          resource_index_or_invalid(resource_indices, "vignette");
      if (vignette != k_invalid_resource_index) {
        builder.export_image(make_vignette_render_image_export(vignette));
      }
      return;
    }

    case FilmGrain: {
      const auto film_grain =
          resource_index_or_invalid(resource_indices, "film_grain");
      if (film_grain != k_invalid_resource_index) {
        builder.export_image(make_film_grain_render_image_export(film_grain));
      }
      return;
    }

    case CASComposite:
    case ChromaticAberrationComposite:
    case VignetteComposite:
    case FilmGrainComposite: {
      const auto present =
          resource_index_or_invalid(resource_indices, "present");
      if (present != k_invalid_resource_index) {
        builder.export_image(make_final_output_render_image_export(present));
      }
      return;
    }

    case Bloom: {
      const auto bloom = resource_index_or_invalid(resource_indices, "bloom");
      if (bloom != k_invalid_resource_index) {
        builder.export_image(make_bloom_render_image_export(bloom));
      }
      return;
    }

    case PostProcess: {
      const auto present =
          resource_index_or_invalid(resource_indices, "present");
      if (present != k_invalid_resource_index) {
        builder.export_image(make_final_output_render_image_export(present));
      }
      return;
    }

    default:
      return;
  }
}

void apply_eye_adaptation_resource_accesses(
    PassBuilder &builder,
    BuiltinPassType pass_type,
    const std::unordered_map<std::string, uint32_t> &resource_indices
) {
  const auto histogram_resource = resource_index_or_invalid(
      resource_indices, k_eye_adaptation_histogram_resource
  );
  const auto exposure_resource = resource_index_or_invalid(
      resource_indices, k_eye_adaptation_exposure_resource
  );

  switch (pass_type) {
    case EyeAdaptationHistogram:
      ASTRA_ENSURE(
          histogram_resource == k_invalid_resource_index,
          "Missing internal eye adaptation histogram resource"
      );
      builder.write(histogram_resource);
      return;

    case EyeAdaptationAverage:
      ASTRA_ENSURE(
          histogram_resource == k_invalid_resource_index,
          "Missing internal eye adaptation histogram resource"
      );
      ASTRA_ENSURE(
          exposure_resource == k_invalid_resource_index,
          "Missing internal eye adaptation exposure resource"
      );
      builder.read(histogram_resource);
      builder.read_write(exposure_resource);
      return;

    case PostProcess:
      ASTRA_ENSURE(
          exposure_resource == k_invalid_resource_index,
          "Missing internal eye adaptation exposure resource"
      );
      builder.read(exposure_resource);
      return;

    default:
      return;
  }
}

} // namespace

RenderSystem::RenderSystem(RenderSystemConfig &config) : m_config(config) {};

void RenderSystem::reset_render_graph_state() {
  m_shadow_map_resource_index = k_invalid_resource_index;
  m_scene_color_resource_index = k_invalid_resource_index;
  m_scene_depth_resource_index = k_invalid_resource_index;
  m_bloom_extract_resource_index = k_invalid_resource_index;
  m_present_resource_index = k_invalid_resource_index;
  m_ssao_resource_index = k_invalid_resource_index;
  m_ssao_blur_resource_index = k_invalid_resource_index;
  m_ssgi_resource_index = k_invalid_resource_index;
  m_ssgi_blur_resource_index = k_invalid_resource_index;
  m_ssgi_temporal_resource_index = k_invalid_resource_index;
  m_ssgi_history_resource_index = k_invalid_resource_index;
  m_volumetric_fog_resource_index = k_invalid_resource_index;
  m_volumetric_blur_h_resource_index = k_invalid_resource_index;
  m_volumetric_blur_resource_index = k_invalid_resource_index;
  m_volumetric_temporal_resource_index = k_invalid_resource_index;
  m_volumetric_history_resource_index = k_invalid_resource_index;
  m_lens_flare_resource_index = k_invalid_resource_index;
  m_god_rays_resource_index = k_invalid_resource_index;
  m_eye_adaptation_histogram_resource_index = k_invalid_resource_index;
  m_eye_adaptation_exposure_resource_index = k_invalid_resource_index;
  m_bloom_resource_index = k_invalid_resource_index;
  m_g_position_resource_index = k_invalid_resource_index;
  m_g_normal_resource_index = k_invalid_resource_index;
  m_g_geometric_normal_resource_index = k_invalid_resource_index;
  m_g_albedo_resource_index = k_invalid_resource_index;
  m_g_emissive_resource_index = k_invalid_resource_index;
  m_g_entity_id_resource_index = k_invalid_resource_index;
  m_g_velocity_resource_index = k_invalid_resource_index;
  m_entity_pick_resource_index = k_invalid_resource_index;
  m_pending_entity_pick_request.reset();
  m_pending_entity_pick_submissions.clear();
  m_latest_entity_pick_result.reset();
  m_entity_pick_readback_request = {};
  m_render_frame_serial = 0;
  m_eye_adaptation_state = {};
  invalidate_ssgi_history();
}

void RenderSystem::build_graph_resources(RenderGraphBuilder &builder) {
  auto window =
      window_manager()->get_window_by_id(m_render_target->window_id());

  auto window_width = static_cast<uint32_t>(window->width());
  auto window_height = static_cast<uint32_t>(window->height());

  m_resource_indices.clear();
  m_resource_indices.reserve(m_config.render_graph.resources.size() + 2u);

  for (const auto &resource_config : m_config.render_graph.resources) {
    ASTRA_ENSURE(
        resource_config.name.empty(),
        "Render graph resources must be named"
    );

    ImageDesc desc;
    desc.debug_name = resource_config.name;
    desc.depth = 1;
    desc.samples = 1;
    desc.format = image_format_from_manifest(resource_config.format);
    desc.usage = image_usage_mask_from_manifest(resource_config.usage);

    const RenderGraphSizeConfig size =
        effective_resource_size(resource_config, m_config);

    if (size.mode == RenderGraphSizeMode::WindowRelative) {
      desc.extent.mode = ImageExtentMode::WindowRelative;
      desc.extent.scale_x = size.scale_x;
      desc.extent.scale_y = size.scale_y;
      desc.width = resolve_window_relative_extent(
          window_width, size.scale_x
      );
      desc.height = resolve_window_relative_extent(
          window_height, size.scale_y
      );
    } else {
      desc.extent.mode = ImageExtentMode::Absolute;
      desc.extent.width = size.width;
      desc.extent.height = size.height;
      desc.width = size.width;
      desc.height = size.height;
    }

    auto handle = builder.declare_image(
        resource_config.name,
        desc,
        lifetime_from_manifest(resource_config.lifetime)
    );
    auto [resource_it, inserted] = m_resource_indices.emplace(
        resource_config.name,
        builder.resolve_resource_index(handle)
    );
    ASTRA_ENSURE(
        !inserted,
        "Duplicate render graph resource declared: ",
        resource_config.name
    );
    (void)resource_it;
  }

  {
    const auto histogram_resource_index = builder.declare_storage_buffer(
        std::string(k_eye_adaptation_histogram_resource),
        k_eye_adaptation_histogram_bin_count * sizeof(uint32_t)
    );
    auto [resource_it, inserted] = m_resource_indices.emplace(
        std::string(k_eye_adaptation_histogram_resource),
        histogram_resource_index
    );
    ASTRA_ENSURE(
        !inserted,
        "Duplicate render graph resource declared: ",
        k_eye_adaptation_histogram_resource
    );
    (void)resource_it;
  }

  {
    const auto exposure_resource_index = builder.declare_storage_buffer(
        std::string(k_eye_adaptation_exposure_resource),
        sizeof(EyeAdaptationExposureData)
    );
    auto [resource_it, inserted] = m_resource_indices.emplace(
        std::string(k_eye_adaptation_exposure_resource),
        exposure_resource_index
    );
    ASTRA_ENSURE(
        !inserted,
        "Duplicate render graph resource declared: ",
        k_eye_adaptation_exposure_resource
    );
    (void)resource_it;
  }

  m_shadow_map_resource_index = resource_index_or_invalid(
      m_resource_indices, "shadow_map"
  );
  m_scene_color_resource_index = resource_index_or_invalid(
      m_resource_indices, "scene_color"
  );
  m_scene_depth_resource_index = resource_index_or_invalid(
      m_resource_indices, "scene_depth"
  );
  m_bloom_extract_resource_index = resource_index_or_invalid(
      m_resource_indices, "bloom_extract"
  );
  m_present_resource_index = resource_index_or_invalid(
      m_resource_indices, "present"
  );
  m_ssao_resource_index = resource_index_or_invalid(
      m_resource_indices, "ssao"
  );
  m_ssao_blur_resource_index = resource_index_or_invalid(
      m_resource_indices, "ssao_blur"
  );
  m_ssgi_resource_index = resource_index_or_invalid(
      m_resource_indices, "ssgi"
  );
  m_ssgi_blur_resource_index = resource_index_or_invalid(
      m_resource_indices, "ssgi_blur"
  );
  m_ssgi_temporal_resource_index = resource_index_or_invalid(
      m_resource_indices, "ssgi_temporal"
  );
  m_ssgi_history_resource_index = resource_index_or_invalid(
      m_resource_indices, "ssgi_history"
  );
  m_volumetric_fog_resource_index = resource_index_or_invalid(
      m_resource_indices, "volumetric_fog"
  );
  m_volumetric_blur_h_resource_index = resource_index_or_invalid(
      m_resource_indices, "volumetric_blur_h"
  );
  m_volumetric_blur_resource_index = resource_index_or_invalid(
      m_resource_indices, "volumetric_blur"
  );
  m_volumetric_temporal_resource_index = resource_index_or_invalid(
      m_resource_indices, "volumetric_temporal"
  );
  m_volumetric_history_resource_index = resource_index_or_invalid(
      m_resource_indices, "volumetric_history"
  );
  m_lens_flare_resource_index = resource_index_or_invalid(
      m_resource_indices, "lens_flare"
  );
  m_god_rays_resource_index = resource_index_or_invalid(
      m_resource_indices, "god_rays"
  );
  m_eye_adaptation_histogram_resource_index = resource_index_or_invalid(
      m_resource_indices, k_eye_adaptation_histogram_resource
  );
  m_eye_adaptation_exposure_resource_index = resource_index_or_invalid(
      m_resource_indices, k_eye_adaptation_exposure_resource
  );
  m_bloom_resource_index = resource_index_or_invalid(
      m_resource_indices, "bloom"
  );
  m_g_position_resource_index = resource_index_or_invalid(
      m_resource_indices, "g_position"
  );
  m_g_normal_resource_index = resource_index_or_invalid(
      m_resource_indices, "g_normal"
  );
  m_g_geometric_normal_resource_index = resource_index_or_invalid(
      m_resource_indices, "g_geometric_normal"
  );
  m_g_albedo_resource_index = resource_index_or_invalid(
      m_resource_indices, "g_albedo"
  );
  m_g_emissive_resource_index = resource_index_or_invalid(
      m_resource_indices, "g_emissive"
  );
  m_g_entity_id_resource_index = resource_index_or_invalid(
      m_resource_indices, "g_entity_id"
  );
  m_g_velocity_resource_index = resource_index_or_invalid(
      m_resource_indices, "g_velocity"
  );
  m_entity_pick_resource_index = resource_index_or_invalid(
      m_resource_indices, "entity_pick"
  );
}

void RenderSystem::build_passes_inline() {
  ASTRA_PROFILE_N("RenderSystem::build_passes_inline");
  RenderGraphBuilder pass_builder_graph;
  {
    ASTRA_PROFILE_N("build_graph_resources");
    build_graph_resources(pass_builder_graph);
  }

  {
    ASTRA_PROFILE_N("build_passes_from_config");
    for (const auto &pass_config : m_config.render_graph.passes) {
      const auto pass_type =
          resolve_builtin_pass_type(pass_config.type, pass_config.id);
      auto pass_builder = pass_builder_graph.add_pass(
          create_manifest_frame_pass(
              pass_type,
              m_skybox_cube,
              m_fullscreen_quad,
              &m_eye_adaptation_state,
              &m_entity_pick_readback_request
          ),
          builtin_pass_graph_type(pass_type)
      );

      for (const auto &dependency : pass_config.dependencies) {
        apply_manifest_dependency(pass_builder, dependency);
      }

      for (const auto &use : pass_config.uses) {
        const auto resource_index = require_resource_index(
            m_resource_indices,
            use.resource,
            pass_config.id.empty() ? pass_config.type : pass_config.id
        );
        pass_builder.use_image(
            resource_index,
            image_aspect_from_manifest(use.aspect),
            render_usage_from_manifest(use.usage)
        );
      }

      if (pass_config.present.has_value()) {
        const auto &present = *pass_config.present;
        const auto resource_index = require_resource_index(
            m_resource_indices,
            present.resource,
            pass_config.id.empty() ? pass_config.type : pass_config.id
        );
        pass_builder.present(
            resource_index,
            image_aspect_from_manifest(present.aspect)
        );
      }

      apply_eye_adaptation_resource_accesses(
          pass_builder, pass_type, m_resource_indices
      );
      apply_builtin_exports(pass_builder, pass_type, m_resource_indices);
    }
  }

  {
    ASTRA_PROFILE_N("RenderGraphBuilder::build");
    m_render_graph = pass_builder_graph.build();
  }
  m_render_graph->compile(m_render_target);
}

void RenderSystem::finalize_after_pass_load() {
  rebuild_render_image_exports();
}

void RenderSystem::prepare_for_pass_reload() {
  if (m_render_graph == nullptr)
    return;
  m_render_graph->prepare_for_pass_reload();
}

void RenderSystem::load_passes_from_module(const AstraModuleAPI *api) {
  if (m_render_graph == nullptr || api == nullptr)
    return;

  m_render_graph->prepare_for_pass_reload();

  RenderGraphBuilder temporary_builder;
  build_graph_resources(temporary_builder);

  RenderPassesBuildContext context{
      .builder = &temporary_builder,
      .pass_configs = &m_config.render_graph.passes,
      .resource_indices = &m_resource_indices,
      .skybox_cube = &m_skybox_cube,
      .fullscreen_quad = &m_fullscreen_quad,
      .eye_adaptation_state = &m_eye_adaptation_state,
      .entity_pick_request = &m_entity_pick_readback_request,
  };

  api->load(&context, sizeof(context));

  m_render_graph->replace_passes(temporary_builder.take_passes());
  m_render_graph->finalize_pass_reload();
  invalidate_ssgi_history();
  finalize_after_pass_load();

  LOG_INFO("RenderSystem: passes reloaded from module");
}

void RenderSystem::warm_async_pass_dependency_resources(
    RendererBackend backend
) {
  ASTRA_PROFILE_N("RenderSystem::warm_async_pass_dependency_resources");

  if (resource_manager()->get_descriptor_by_id<Texture2DDescriptor>(
          rendering::brdf_lut_texture_id()
      ) != nullptr ||
      m_brdf_lut_warmup_queued) {
    return;
  }

  auto *jobs = JobSystem::get();
  if (jobs == nullptr) {
    rendering::ensure_brdf_lut();
    resource_manager()->load_from_descriptors_by_ids<Texture2DDescriptor>(
        backend,
        {rendering::brdf_lut_texture_id()}
    );
    return;
  }

  LOG_DEBUG("[RenderSystem] scheduling async BRDF LUT warmup");
  m_brdf_lut_warmup_queued = true;
  m_brdf_lut_pixels = std::make_shared<std::vector<unsigned char>>();

  JobHandle generate_job = jobs->submit(
      [pixels = m_brdf_lut_pixels]() {
        *pixels = rendering::generate_brdf_lut_rgba();
      },
      JobQueue::Worker,
      JobPriority::Normal
  );

  const std::array<JobHandle, 1> dependencies = {generate_job};
  jobs->submit_after(
      dependencies,
      [backend, pixels = m_brdf_lut_pixels, generate_job]() {
        JobSystem::get()->wait(generate_job);

        if (resource_manager()->get_descriptor_by_id<Texture2DDescriptor>(
                rendering::brdf_lut_texture_id()
            ) == nullptr) {
          Texture2D::create(
              rendering::brdf_lut_texture_id(),
              rendering::make_brdf_lut_config(
                  rendering::k_brdf_lut_size,
                  rendering::k_brdf_lut_size,
                  pixels->data()
              )
          );
        }

        resource_manager()->load_from_descriptors_by_ids<Texture2DDescriptor>(
            backend,
            {rendering::brdf_lut_texture_id()}
        );
        LOG_DEBUG("[RenderSystem] async BRDF LUT ready");
      },
      JobQueue::Main,
      JobPriority::Normal
  );
}

void RenderSystem::start() {
  ASTRA_PROFILE_N("RenderSystem::start");
  {
    ASTRA_PROFILE_N("RenderTarget::create");
    m_render_target = RenderTarget::create(
        m_config.backend_to_api(), m_config.msaa_to_render_target_msaa(), m_config.window_id
    );
  }

  {
    ASTRA_PROFILE_N("RenderTarget::init");
    m_render_target->init();
  }

  {
    ASTRA_PROFILE_N("ensure_pass_dependency_descriptors");
    ensure_pass_dependency_descriptors();
  }
  const auto render_backend = m_render_target->backend();

  {
    ASTRA_PROFILE_N("warm_async_pass_dependency_resources");
    warm_async_pass_dependency_resources(render_backend);
  }

  {
    ASTRA_PROFILE_N("load_from_descriptors");
    switch (render_backend) {
      case RendererBackend::OpenGL: {
        resource_manager()->load_from_descriptors<ShaderDescriptor, Texture3DDescriptor, MaterialDescriptor, FontDescriptor, SvgDescriptor>(render_backend);
        break;
      }
      case RendererBackend::Vulkan: {
        resource_manager()->load_from_descriptors<ShaderDescriptor, Texture3DDescriptor, MaterialDescriptor, FontDescriptor, SvgDescriptor>(render_backend);
        break;
      }

      default: {
        ASTRA_EXCEPTION("You must define a renderer backend first")
      }
    }
  }

  reset_render_graph_state();
  ASTRA_ENSURE(
      !m_config.render_graph.is_defined(),
      "Render system requires a top-level render_graph definition in project.ax"
  );
  ASTRA_ENSURE(
      m_config.render_graph.resources.empty(),
      "Render graph must define at least one resource"
  );
  ASTRA_ENSURE(
      m_config.render_graph.passes.empty(),
      "Render graph must define at least one pass"
  );

  {
    ASTRA_PROFILE_N("create_skybox_cube_mesh");
    m_skybox_cube = rendering::create_skybox_cube_mesh(render_backend);
  }
  {
    ASTRA_PROFILE_N("create_fullscreen_quad_mesh");
    m_fullscreen_quad = rendering::create_fullscreen_quad_mesh(render_backend);
  }

  {
    ASTRA_PROFILE_N("build_passes_inline");
    build_passes_inline();
  }
  {
    ASTRA_PROFILE_N("finalize_after_pass_load");
    finalize_after_pass_load();
  }

#ifdef ASTRA_RENDERER_HOT_RELOAD
  {
    ASTRA_PROFILE_N("initialize_shader_watcher");
    m_shader_watcher = create_scope<ShaderWatcher>(ShaderWatcher::Config{});
    initialize_shader_watcher();
  }
#endif
}

void RenderSystem::fixed_update(double fixed_dt) {
  ASTRA_PROFILE_N("RenderSystem FixedUpdate");
};

void RenderSystem::pre_update(double dt) {
  ASTRA_PROFILE_N("RenderSystem PreUpdate");

#ifdef ASTRA_RENDERER_HOT_RELOAD
  poll_shader_reloads();
#endif

  auto window = window_manager()->active_window();

  if (window->was_resized) {
    const auto width = static_cast<uint32_t>(window->width());
    const auto height = static_cast<uint32_t>(window->height());

    if (m_render_target != nullptr && m_render_target->framebuffer() != nullptr) {
      m_render_target->framebuffer()->resize(width, height);
    }

    if (m_render_graph != nullptr) {
      m_render_graph->resize(width, height);
      invalidate_ssgi_history();
    }
  }

  m_render_target->bind(true);
};

void RenderSystem::update(double dt) {
  ASTRA_PROFILE_N("RenderSystem Update");

  drain_completed_entity_picks();

  if (m_render_graph) {
    auto *active_scene = SceneManager::get()->get_active_scene();
    if (active_scene) {
      ++m_render_frame_serial;
      auto scene_frame = rendering::build_scene_frame(
          active_scene->world(),
          m_render_target,
          m_render_runtime_store,
          m_camera_history
      );
      const auto &overrides = active_scene->render_overrides();

      const auto &ssgi = overrides.ssgi.value_or(m_config.ssgi);
      scene_frame.ssgi = rendering::SSGIFrameSettings{
          .enabled = ssgi.enabled,
          .full_resolution = ssgi.full_resolution,
          .temporal = ssgi.temporal,
          .intensity = ssgi.intensity,
          .radius = ssgi.radius,
          .thickness = ssgi.thickness,
          .directions = ssgi.directions,
          .steps_per_direction = ssgi.steps_per_direction,
          .max_distance = ssgi.max_distance,
          .history_weight = ssgi.history_weight,
          .normal_reject_dot = ssgi.normal_reject_dot,
          .position_reject_distance = ssgi.position_reject_distance,
      };

      const auto &ssr = overrides.ssr.value_or(m_config.ssr);
      scene_frame.ssr = rendering::SSRFrameSettings{
          .enabled = ssr.enabled,
          .intensity = ssr.intensity,
          .max_distance = ssr.max_distance,
          .thickness = ssr.thickness,
          .max_steps = ssr.max_steps,
          .stride = ssr.stride,
          .roughness_cutoff = ssr.roughness_cutoff,
      };

      const auto &volumetric = overrides.volumetric.value_or(m_config.volumetric);
      scene_frame.volumetric = rendering::VolumetricFogFrameSettings{
          .enabled = volumetric.enabled,
          .max_steps = volumetric.max_steps,
          .density = volumetric.density,
          .scattering = volumetric.scattering,
          .max_distance = volumetric.max_distance,
          .intensity = volumetric.intensity,
          .fog_base_height = volumetric.fog_base_height,
          .height_falloff_rate = volumetric.height_falloff_rate,
          .noise_scale = volumetric.noise_scale,
          .noise_weight = volumetric.noise_weight,
          .wind_direction = volumetric.wind_direction,
          .wind_speed = volumetric.wind_speed,
          .temporal_enabled = volumetric.temporal,
          .temporal_blend_weight = volumetric.temporal_blend_weight,
      };

      const auto &lens_flare = overrides.lens_flare.value_or(m_config.lens_flare);
      scene_frame.lens_flare = rendering::LensFlareFrameSettings{
          .enabled = lens_flare.enabled,
          .intensity = lens_flare.intensity,
          .threshold = lens_flare.threshold,
          .ghost_count = lens_flare.ghost_count,
          .ghost_dispersal = lens_flare.ghost_dispersal,
          .ghost_weight = lens_flare.ghost_weight,
          .halo_radius = lens_flare.halo_radius,
          .halo_weight = lens_flare.halo_weight,
          .halo_thickness = lens_flare.halo_thickness,
          .chromatic_aberration = lens_flare.chromatic_aberration,
      };

      const auto &eye_adaptation = overrides.eye_adaptation.value_or(m_config.eye_adaptation);
      scene_frame.eye_adaptation = rendering::EyeAdaptationFrameSettings{
          .enabled = eye_adaptation.enabled,
          .min_log_luminance = eye_adaptation.min_log_luminance,
          .max_log_luminance = eye_adaptation.max_log_luminance,
          .adaptation_speed_up = eye_adaptation.adaptation_speed_up,
          .adaptation_speed_down = eye_adaptation.adaptation_speed_down,
          .key_value = eye_adaptation.key_value,
          .low_percentile = eye_adaptation.low_percentile,
          .high_percentile = eye_adaptation.high_percentile,
      };

      const auto &motion_blur = overrides.motion_blur.value_or(m_config.motion_blur);
      scene_frame.motion_blur = rendering::MotionBlurFrameSettings{
          .enabled = motion_blur.enabled,
          .intensity = motion_blur.intensity,
          .max_samples = motion_blur.max_samples,
          .depth_threshold = motion_blur.depth_threshold,
      };

      const auto &chromatic_aberration = overrides.chromatic_aberration.value_or(m_config.chromatic_aberration);
      scene_frame.chromatic_aberration =
          rendering::ChromaticAberrationFrameSettings{
              .enabled = chromatic_aberration.enabled,
              .intensity = chromatic_aberration.intensity,
          };

      const auto &vignette = overrides.vignette.value_or(m_config.vignette);
      scene_frame.vignette = rendering::VignetteFrameSettings{
          .enabled = vignette.enabled,
          .intensity = vignette.intensity,
          .smoothness = vignette.smoothness,
          .roundness = vignette.roundness,
      };

      const auto &film_grain = overrides.film_grain.value_or(m_config.film_grain);
      scene_frame.film_grain = rendering::FilmGrainFrameSettings{
          .enabled = film_grain.enabled,
          .intensity = film_grain.intensity,
          .time = static_cast<float>(m_render_frame_serial),
      };

      const auto &depth_of_field = overrides.depth_of_field.value_or(m_config.depth_of_field);
      scene_frame.depth_of_field = rendering::DepthOfFieldFrameSettings{
          .enabled = depth_of_field.enabled,
          .focus_distance = depth_of_field.focus_distance,
          .focus_range = depth_of_field.focus_range,
          .max_blur_radius = depth_of_field.max_blur_radius,
          .sample_count = depth_of_field.sample_count,
      };

      const auto &god_rays = overrides.god_rays.value_or(m_config.god_rays);
      scene_frame.god_rays = rendering::GodRaysFrameSettings{
          .enabled = god_rays.enabled,
          .intensity = god_rays.intensity,
          .decay = god_rays.decay,
          .density = god_rays.density,
          .weight = god_rays.weight,
          .threshold = god_rays.threshold,
          .samples = god_rays.samples,
      };

      const auto &taa = overrides.taa.value_or(m_config.taa);
      scene_frame.taa = rendering::TAAFrameSettings{
          .enabled = taa.enabled,
          .blend_factor = taa.blend_factor,
      };

      const auto &cas = overrides.cas.value_or(m_config.cas);
      scene_frame.cas = rendering::CASFrameSettings{
          .enabled = cas.enabled,
          .sharpness = cas.sharpness,
          .contrast = cas.contrast,
          .sharpening_limit = cas.sharpening_limit,
      };

      const auto &tonemapping = overrides.tonemapping.value_or(m_config.tonemapping);
      scene_frame.tonemapping = rendering::TonemappingFrameSettings{
          .tonemap_operator = static_cast<int>(tonemapping.tonemap_operator),
          .gamma = tonemapping.gamma,
          .bloom_strength = tonemapping.bloom_strength,
      };

      active_scene->world().each<rendering::Light, rendering::LensFlare>(
          [&](EntityID entity_id, rendering::Light &, rendering::LensFlare &entity_flare) {
            if (!active_scene->world().active(entity_id)) {
              return;
            }
            scene_frame.lens_flare.enabled = entity_flare.enabled;
            scene_frame.lens_flare.intensity = entity_flare.intensity;
            scene_frame.lens_flare.threshold = entity_flare.threshold;
            scene_frame.lens_flare.ghost_count = entity_flare.ghost_count;
            scene_frame.lens_flare.ghost_dispersal = entity_flare.ghost_dispersal;
            scene_frame.lens_flare.ghost_weight = entity_flare.ghost_weight;
            scene_frame.lens_flare.halo_radius = entity_flare.halo_radius;
            scene_frame.lens_flare.halo_weight = entity_flare.halo_weight;
            scene_frame.lens_flare.halo_thickness = entity_flare.halo_thickness;
            scene_frame.lens_flare.chromatic_aberration = entity_flare.chromatic_aberration;
          }
      );

      m_entity_pick_readback_request = {};

      if (m_pending_entity_pick_request.has_value()) {
        const auto extent = entity_selection_extent();
        const auto &pixel = m_pending_entity_pick_request->pixel;

        if (extent.has_value() && pixel.x >= 0 && pixel.y >= 0 &&
            pixel.x < extent->x && pixel.y < extent->y) {
          m_pending_entity_pick_submissions.push_back(
              PendingEntityPickSubmission{
                  .frame_serial = m_render_frame_serial,
                  .pixel = pixel,
                  .pick_id_lut = std::make_shared<const std::vector<EntityID>>(
                      scene_frame.pick_id_lut
                  ),
              }
          );

          auto &submission = m_pending_entity_pick_submissions.back();
          m_entity_pick_readback_request = rendering::EntityPickReadbackRequest{
              .armed = true,
              .pixel = pixel,
              .out_value = &submission.raw_value,
              .out_ready = &submission.ready,
          };
        }

        m_pending_entity_pick_request.reset();
      }

      m_render_graph->execute(dt, &scene_frame);
      advance_temporal_history(scene_frame);
      m_entity_pick_readback_request = {};
      drain_completed_entity_picks();
    }
  }

  auto scheduler = EventScheduler::get();
  debug_draw()->advance(dt);
  scheduler->bind(SchedulerType::REALTIME);
};

std::optional<ResolvedRenderImage>
RenderSystem::resolve_render_image(RenderImageExportKey key) const {
  if (m_render_graph == nullptr) {
    return std::nullopt;
  }

  const auto &frame = m_render_graph->latest_compiled_frame();
  const auto *entry = frame.find_export(key);
  if (entry == nullptr) {
    return std::nullopt;
  }

  return ResolvedRenderImage{
      .available = true,
      .view = ImageViewRef{.image = entry->image},
      .width = entry->extent.width,
      .height = entry->extent.height,
  };
}

std::optional<glm::ivec2> RenderSystem::entity_selection_extent() const {
  if (m_render_graph == nullptr) {
    return std::nullopt;
  }

  const auto *resource = m_render_graph->resource_at(m_entity_pick_resource_index);
  if (resource == nullptr || resource->get_graph_image() == nullptr) {
    return std::nullopt;
  }

  const auto &desc = resource->get_graph_image()->desc;
  if (desc.width == 0 || desc.height == 0) {
    return std::nullopt;
  }

  return glm::ivec2(
      static_cast<int>(desc.width),
      static_cast<int>(desc.height)
  );
}

void RenderSystem::request_entity_pick(glm::ivec2 pixel) {
  m_pending_entity_pick_request = PendingEntityPickRequest{
      .pixel = pixel,
  };
}

std::optional<EntityPickResult> RenderSystem::consume_latest_entity_pick() {
  auto result = m_latest_entity_pick_result;
  m_latest_entity_pick_result.reset();
  return result;
}

void RenderSystem::set_ssgi_config(SSGIConfig config) {
  const bool rebuild_graph =
      m_config.ssgi.full_resolution != config.full_resolution;
  m_config.ssgi = std::move(config);
  invalidate_ssgi_history();

  if (!rebuild_graph || m_render_target == nullptr || !m_config.render_graph.is_defined()) {
    return;
  }

  reset_render_graph_state();
  build_passes_inline();
  finalize_after_pass_load();
}

void RenderSystem::set_ssr_config(SSRConfig config) {
  m_config.ssr = std::move(config);
}

void RenderSystem::set_volumetric_config(VolumetricFogConfig config) {
  m_config.volumetric = std::move(config);
}

void RenderSystem::set_lens_flare_config(LensFlareConfig config) {
  m_config.lens_flare = std::move(config);
}

void RenderSystem::set_eye_adaptation_config(EyeAdaptationConfig config) {
  const bool enabled_changed =
      m_config.eye_adaptation.enabled != config.enabled;
  m_config.eye_adaptation = std::move(config);

  if (!m_config.eye_adaptation.enabled || enabled_changed) {
    m_eye_adaptation_state.initialized = false;
  }
}

void RenderSystem::set_motion_blur_config(MotionBlurConfig config) {
  m_config.motion_blur = std::move(config);
}

void RenderSystem::set_chromatic_aberration_config(
    ChromaticAberrationConfig config
) {
  m_config.chromatic_aberration = std::move(config);
}

void RenderSystem::set_vignette_config(VignetteConfig config) {
  m_config.vignette = std::move(config);
}

void RenderSystem::set_film_grain_config(FilmGrainConfig config) {
  m_config.film_grain = std::move(config);
}

void RenderSystem::set_depth_of_field_config(DepthOfFieldConfig config) {
  m_config.depth_of_field = std::move(config);
}

void RenderSystem::set_god_rays_config(GodRaysConfig config) {
  m_config.god_rays = std::move(config);
}

void RenderSystem::set_taa_config(TAAConfig config) {
  m_config.taa = std::move(config);
}

void RenderSystem::set_cas_config(CASConfig config) {
  m_config.cas = std::move(config);
}

void RenderSystem::set_tonemapping_config(TonemappingConfig config) {
  m_config.tonemapping = std::move(config);
}

void RenderSystem::set_render_graph_config(RenderGraphConfig config) {
  m_config.render_graph = std::move(config);
  if (m_render_target == nullptr || !m_config.render_graph.is_defined()) {
    return;
  }
  reset_render_graph_state();
  build_passes_inline();
  finalize_after_pass_load();
}

void RenderSystem::invalidate_ssgi_history() {
  m_camera_history = {};
  for (auto &[entity_id, state] : m_render_runtime_store.entity_states) {
    (void)entity_id;
    state.has_previous_model = false;
  }
}

void RenderSystem::advance_temporal_history(
    const rendering::SceneFrame &scene_frame
) {
  if (scene_frame.main_camera.has_value()) {
    m_camera_history.entity_id = scene_frame.main_camera->entity_id;
    m_camera_history.previous_view = scene_frame.main_camera->view;
    m_camera_history.previous_projection =
        scene_frame.main_camera->projection;
    m_camera_history.valid = true;
  } else {
    m_camera_history = {};
  }

  for (const auto &surface : scene_frame.opaque_surfaces) {
    auto &state = m_render_runtime_store.entity_states[surface.entity_id];
    state.previous_model = surface.model;
    state.has_previous_model = true;
  }
}

void RenderSystem::rebuild_render_image_exports() {
  m_render_image_exports.clear();

  if (m_render_graph == nullptr) {
    return;
  }

  for (const auto &compiled_export : m_render_graph->compiled_exports()) {
    m_render_image_exports.push_back(RenderImageExportBinding{
        .key = compiled_export.key,
        .available = true,
    });
  }
}

void RenderSystem::drain_completed_entity_picks() {
  for (auto it = m_pending_entity_pick_submissions.begin();
       it != m_pending_entity_pick_submissions.end();) {
    if (!it->ready) {
      ++it;
      continue;
    }

    std::optional<EntityID> entity_id;
    if (it->raw_value > 0 && it->pick_id_lut != nullptr &&
        static_cast<size_t>(it->raw_value) <= it->pick_id_lut->size()) {
      entity_id = (*it->pick_id_lut)[static_cast<size_t>(it->raw_value - 1)];
    }

    m_latest_entity_pick_result = EntityPickResult{
        .frame_serial = it->frame_serial,
        .pixel = it->pixel,
        .entity_id = entity_id,
    };
    it = m_pending_entity_pick_submissions.erase(it);
  }
}

RenderSystem::~RenderSystem() {
#ifdef ASTRA_RENDERER_HOT_RELOAD
  if (m_shader_watcher) {
    m_shader_watcher->stop();
  }
#endif
}

#ifdef ASTRA_RENDERER_HOT_RELOAD
void RenderSystem::initialize_shader_watcher() {
  auto descriptors = resource_manager()->shader_descriptors();
  if (descriptors.empty())
    return;

  std::vector<std::filesystem::path> watch_directories;

  for (const auto &descriptor : descriptors) {
    auto register_path = [&](Ref<Path> shader_path) {
      if (shader_path == nullptr)
        return;
      auto resolved = PathManager::get()->resolve_source(shader_path);
      if (!std::filesystem::exists(resolved))
        return;

      m_shader_watcher->register_source(descriptor->id, resolved);

      auto parent = resolved.parent_path();
      if (std::find(watch_directories.begin(), watch_directories.end(), parent) ==
          watch_directories.end()) {
        watch_directories.push_back(parent);
      }
    };

    register_path(descriptor->vertex_path);
    register_path(descriptor->fragment_path);
    register_path(descriptor->geometry_path);

    auto shader = resource_manager()->get_by_descriptor_id<Shader>(descriptor->id);
    if (shader != nullptr) {
      for (const auto &dependency : shader->source_dependencies()) {
        auto source_path = PathManager::get()->remap_to_source(dependency);
        if (std::filesystem::exists(source_path)) {
          m_shader_watcher->register_source(descriptor->id, source_path);
        }
      }
    }
  }

  m_shader_watcher->start();
  LOG_INFO("ShaderWatcher: tracking", descriptors.size(), "shader descriptors");
}

void RenderSystem::poll_shader_reloads() {
  if (!m_shader_watcher)
    return;

  auto changed = m_shader_watcher->poll_changed();
  if (changed.empty())
    return;

  for (const auto &descriptor_id : changed) {
    LOG_INFO("ShaderWatcher: recompiling", descriptor_id);
    bool success = resource_manager()->reload_shader(descriptor_id);
    if (success) {
      LOG_INFO("ShaderWatcher: recompiled", descriptor_id);
      auto shader = resource_manager()->get_by_descriptor_id<Shader>(descriptor_id);
      if (shader != nullptr) {
        for (const auto &dependency : shader->source_dependencies()) {
          auto source_path = PathManager::get()->remap_to_source(dependency);
          if (std::filesystem::exists(source_path)) {
            m_shader_watcher->register_source(descriptor_id, source_path);
          }
        }
      }
    } else {
      LOG_ERROR("ShaderWatcher: failed to recompile", descriptor_id);
    }
  }
}
#endif

void RenderSystem::ensure_pass_dependency_descriptors() {
  ASTRA_PROFILE_N("ensure_pass_dependency_descriptors");
  {
    ASTRA_PROFILE_N("ensure_pbr_default_textures");
    rendering::ensure_pbr_default_textures();
  }
  {
    ASTRA_PROFILE_N("ensure_default_ibl_cubemap");
    rendering::ensure_default_ibl_cubemap();
  }
  {
    ASTRA_PROFILE_N("ensure_default_brdf_lut_fallback");
    rendering::ensure_default_brdf_lut_fallback();
  }

  if (resource_manager()->get_descriptor_by_id<ShaderDescriptor>(
          "shaders::lighting_forward"
      ) == nullptr) {
    ASTRA_PROFILE_N("Shader::create(lighting_forward)");
    Shader::create(
        "shaders::lighting_forward",
        "shaders/lighting-forward.axsl"_engine,
        "shaders/lighting-forward.axsl"_engine
    );
  }

  if (resource_manager()->get_descriptor_by_id<ShaderDescriptor>(
          "shaders::editor_gizmo"
      ) == nullptr) {
    ASTRA_PROFILE_N("Shader::create(editor_gizmo)");
    Shader::create(
        "shaders::editor_gizmo",
        "shaders/editor_gizmo.axsl"_engine,
        "shaders/editor_gizmo.axsl"_engine
    );
  }

  if (resource_manager()->get_descriptor_by_id<ShaderDescriptor>(
          "shaders::ui_polyline"
      ) == nullptr) {
    ASTRA_PROFILE_N("Shader::create(ui_polyline)");
    Shader::create(
        "shaders::ui_polyline",
        "shaders/ui/polyline.axsl"_engine,
        "shaders/ui/polyline.axsl"_engine
    );
  }

  if (resource_manager()->get_descriptor_by_id<ShaderDescriptor>(
          "shaders::ui_vector"
      ) == nullptr) {
    ASTRA_PROFILE_N("Shader::create(ui_vector)");
    Shader::create(
        "shaders::ui_vector",
        "shaders/ui/vector.axsl"_engine,
        "shaders/ui/vector.axsl"_engine
    );
  }

  if (resource_manager()->get_descriptor_by_id<ShaderDescriptor>(
          "shaders::terrain"
      ) == nullptr) {
    ASTRA_PROFILE_N("Shader::create(terrain)");
    Shader::create(
        "shaders::terrain",
        "shaders/terrain.axsl"_engine,
        "shaders/terrain.axsl"_engine
    );
  }

  if (resource_manager()->get_descriptor_by_id<Texture2DDescriptor>(
          "noise_texture"
      ) == nullptr) {
    ASTRA_PROFILE_N("Texture2D::create(noise_texture)");
    TextureConfig texture_config;
    texture_config.width = 4;
    texture_config.height = 4;
    texture_config.bitmap = false;
    texture_config.format = TextureFormat::RGB;
    texture_config.buffer =
        const_cast<unsigned char *>(ssao_noise_seed().data());
    texture_config.parameters = {
        {TextureParameter::WrapS, TextureValue::Repeat},
        {TextureParameter::WrapT, TextureValue::Repeat},
        {TextureParameter::MagFilter, TextureValue::Nearest},
        {TextureParameter::MinFilter, TextureValue::Nearest},
    };
    Texture2D::create("noise_texture", texture_config);
  }

  if (m_render_target != nullptr) {
    ASTRA_PROFILE_N("load_pbr_default_textures");
    for (const auto &id : rendering::default_pbr_texture_ids()) {
      resource_manager()->load_from_descriptors_by_ids<Texture2DDescriptor>(
          m_render_target->backend(), {id}
      );
    }
    resource_manager()->load_from_descriptors_by_ids<Texture2DDescriptor>(
        m_render_target->backend(),
        {rendering::default_brdf_lut_fallback_texture_id()}
    );
  }
}
} // namespace astralix
