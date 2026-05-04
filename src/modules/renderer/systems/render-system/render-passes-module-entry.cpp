#include "module-api.h"

#include "render-passes-build-context.hpp"
#include "systems/render-system/builtin-pass-type.hpp"
#include "systems/render-system/passes/bloom-pass.hpp"
#include "systems/render-system/passes/debug-pass.hpp"
#include "systems/render-system/passes/debug-draw-pass.hpp"
#include "systems/render-system/passes/eye-adaptation-average-pass.hpp"
#include "systems/render-system/passes/eye-adaptation-histogram-pass.hpp"
#include "systems/render-system/passes/editor-gizmo-pass.hpp"
#include "systems/render-system/passes/entity-pick-readback-pass.hpp"
#include "systems/render-system/passes/forward-pass.hpp"
#include "systems/render-system/passes/gbuffer-blend-pass.hpp"
#include "systems/render-system/passes/geometry-pass.hpp"
#include "systems/render-system/passes/grid-pass.hpp"
#include "systems/render-system/passes/lighting-pass.hpp"
#include "systems/render-system/passes/navigation-gizmo-pass.hpp"
#include "systems/render-system/passes/post-process-pass.hpp"
#include "systems/render-system/passes/render-graph-builder.hpp"
#include "systems/render-system/passes/render-graph-usage.hpp"
#include "systems/render-system/passes/shadow-pass.hpp"
#include "systems/render-system/passes/skybox-pass.hpp"
#include "systems/render-system/passes/ssgi-blur-pass.hpp"
#include "systems/render-system/passes/ssgi-composite-pass.hpp"
#include "systems/render-system/passes/ssgi-history-store-pass.hpp"
#include "systems/render-system/passes/ssgi-pass.hpp"
#include "systems/render-system/passes/ssgi-temporal-pass.hpp"
#include "systems/render-system/passes/ssr-blur-pass.hpp"
#include "systems/render-system/passes/ssr-composite-pass.hpp"
#include "systems/render-system/passes/ssr-pass.hpp"
#include "systems/render-system/passes/ssao-blur-pass.hpp"
#include "systems/render-system/passes/ssao-pass.hpp"
#include "systems/render-system/passes/terrain-pass.hpp"
#include "systems/render-system/passes/ui-pass.hpp"
#include "systems/render-system/passes/volumetric-blur-pass.hpp"
#include "systems/render-system/passes/volumetric-composite-pass.hpp"
#include "systems/render-system/passes/volumetric-fog-pass.hpp"
#include "systems/render-system/passes/volumetric-history-store-pass.hpp"
#include "systems/render-system/passes/volumetric-temporal-pass.hpp"
#include "systems/render-system/passes/lens-flare-pass.hpp"
#include "systems/render-system/passes/lens-flare-composite-pass.hpp"
#include "systems/render-system/passes/motion-blur-pass.hpp"
#include "systems/render-system/passes/motion-blur-composite-pass.hpp"
#include "systems/render-system/passes/chromatic-aberration-pass.hpp"
#include "systems/render-system/passes/chromatic-aberration-composite-pass.hpp"
#include "systems/render-system/passes/depth-of-field-pass.hpp"
#include "systems/render-system/passes/depth-of-field-composite-pass.hpp"
#include "systems/render-system/passes/god-rays-pass.hpp"
#include "systems/render-system/passes/god-rays-composite-pass.hpp"
#include "systems/render-system/passes/cas-composite-pass.hpp"
#include "systems/render-system/passes/cas-pass.hpp"
#include "systems/render-system/passes/taa-composite-pass.hpp"
#include "systems/render-system/passes/taa-resolve-pass.hpp"
#include "systems/render-system/passes/taa-history-store-pass.hpp"
#include "systems/render-system/passes/vignette-pass.hpp"
#include "systems/render-system/passes/vignette-composite-pass.hpp"
#include "systems/render-system/passes/film-grain-pass.hpp"
#include "systems/render-system/passes/film-grain-composite-pass.hpp"
#include "project.hpp"
#include "assert.hpp"

#include <limits>
#include <string_view>
#include <unordered_map>

namespace {

using namespace astralix;
using enum BuiltinPassType;

constexpr uint32_t k_invalid_resource_index =
    std::numeric_limits<uint32_t>::max();

Scope<FramePass> create_manifest_frame_pass(
    BuiltinPassType pass_type,
    const rendering::ResolvedMeshDraw &skybox_cube,
    const rendering::ResolvedMeshDraw &fullscreen_quad,
    EyeAdaptationState *eye_adaptation_state,
    rendering::EntityPickReadbackRequest *entity_pick_request
) {
  switch (pass_type) {
    case Shadow: return create_scope<ShadowPass>();
    case Forward: return create_scope<ForwardPass>();
    case Geometry: return create_scope<GeometryPass>();
    case GBufferBlend: return create_scope<GBufferBlendPass>();
    case SSAO: return create_scope<SSAOPass>(fullscreen_quad);
    case SSAOBlur: return create_scope<SSAOBlurPass>(fullscreen_quad);
    case Lighting: return create_scope<LightingPass>(fullscreen_quad);
    case SSR: return create_scope<SSRPass>(fullscreen_quad);
    case SSRBlur: return create_scope<SSRBlurPass>(fullscreen_quad);
    case SSRComposite: return create_scope<SSRCompositePass>(fullscreen_quad);
    case SSGI: return create_scope<SSGIPass>(fullscreen_quad);
    case SSGIBlur: return create_scope<SSGIBlurPass>(fullscreen_quad);
    case SSGITemporal: return create_scope<SSGITemporalPass>(fullscreen_quad);
    case SSGIComposite: return create_scope<SSGICompositePass>(fullscreen_quad);
    case SSGIHistoryStore: return create_scope<SSGIHistoryStorePass>(fullscreen_quad);
    case DebugGBuffer: return create_scope<DebugGBufferPass>(fullscreen_quad);
    case Terrain: return create_scope<TerrainRenderPass>();
    case Skybox: return create_scope<SkyboxPass>(skybox_cube);
    case Grid: return create_scope<GridPass>(fullscreen_quad);
    case DebugOverlay: return create_scope<DebugOverlayPass>(fullscreen_quad);
    case EntityPickReadback: return create_scope<EntityPickReadbackPass>(entity_pick_request);
    case Bloom: return create_scope<BloomPass>(fullscreen_quad);
    case EyeAdaptationHistogram: return create_scope<EyeAdaptationHistogramPass>();
    case EyeAdaptationAverage: return create_scope<EyeAdaptationAveragePass>(eye_adaptation_state);
    case DebugDraw: return create_scope<DebugDrawPass>();
    case EditorGizmo: return create_scope<EditorGizmoPass>();
    case NavigationGizmo: return create_scope<NavigationGizmoPass>();
    case PostProcess: return create_scope<PostProcessPass>(fullscreen_quad, eye_adaptation_state);
    case UI: return create_scope<UIPass>(fullscreen_quad);
    case VolumetricFog: return create_scope<VolumetricFogPass>(fullscreen_quad);
    case VolumetricBlurH: return create_scope<VolumetricBlurPass>(VolumetricBlurPass::Direction::Horizontal, fullscreen_quad);
    case VolumetricBlurV: return create_scope<VolumetricBlurPass>(VolumetricBlurPass::Direction::Vertical, fullscreen_quad);
    case VolumetricTemporal: return create_scope<VolumetricTemporalPass>(fullscreen_quad);
    case VolumetricHistoryStore: return create_scope<VolumetricHistoryStorePass>(fullscreen_quad);
    case VolumetricComposite: return create_scope<VolumetricCompositePass>(fullscreen_quad);
    case LensFlare: return create_scope<LensFlarePass>(fullscreen_quad);
    case LensFlareComposite: return create_scope<LensFlareCompositePass>(fullscreen_quad);
    case MotionBlur: return create_scope<MotionBlurPass>(fullscreen_quad);
    case MotionBlurComposite: return create_scope<MotionBlurCompositePass>(fullscreen_quad);
    case ChromaticAberration: return create_scope<ChromaticAberrationPass>(fullscreen_quad);
    case ChromaticAberrationComposite: return create_scope<ChromaticAberrationCompositePass>(fullscreen_quad);
    case Vignette: return create_scope<VignettePass>(fullscreen_quad);
    case VignetteComposite: return create_scope<VignetteCompositePass>(fullscreen_quad);
    case FilmGrain: return create_scope<FilmGrainPass>(fullscreen_quad);
    case FilmGrainComposite: return create_scope<FilmGrainCompositePass>(fullscreen_quad);
    case DepthOfField: return create_scope<DepthOfFieldPass>(fullscreen_quad);
    case DepthOfFieldComposite: return create_scope<DepthOfFieldCompositePass>(fullscreen_quad);
    case GodRays: return create_scope<GodRaysPass>(fullscreen_quad);
    case GodRaysComposite: return create_scope<GodRaysCompositePass>(fullscreen_quad);
    case TAAResolve: return create_scope<TAAResolvePass>(fullscreen_quad);
    case TAAComposite: return create_scope<TAACompositePass>(fullscreen_quad);
    case TAAHistoryStore: return create_scope<TAAHistoryStorePass>(fullscreen_quad);
    case CAS: return create_scope<CASPass>(fullscreen_quad);
    case CASComposite: return create_scope<CASCompositePass>(fullscreen_quad);
  }
  ASTRA_EXCEPTION("Unsupported render graph pass type");
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
  if (aspect == "color0") return ImageAspect::Color0;
  if (aspect == "color1") return ImageAspect::Color1;
  if (aspect == "color2") return ImageAspect::Color2;
  if (aspect == "color3") return ImageAspect::Color3;
  if (aspect == "depth") return ImageAspect::Depth;
  if (aspect == "stencil") return ImageAspect::Stencil;
  ASTRA_EXCEPTION("Unknown render graph image aspect: ", aspect);
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

    case Forward:
    case Lighting:
    case SSGIComposite:
    case SSRComposite:
    case VolumetricComposite:
    case LensFlareComposite:
    case GodRaysComposite:
    case MotionBlurComposite:
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

    case MotionBlur: {
      const auto motion_blur =
          resource_index_or_invalid(resource_indices, "motion_blur");
      if (motion_blur != k_invalid_resource_index) {
        builder.export_image(make_motion_blur_render_image_export(motion_blur));
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

    case TAAResolve: {
      const auto taa_output =
          resource_index_or_invalid(resource_indices, "taa_output");
      if (taa_output != k_invalid_resource_index) {
        builder.export_image(make_taa_output_render_image_export(taa_output));
      }
      return;
    }

    case TAAComposite:
    case DepthOfFieldComposite: {
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

    case CAS: {
      const auto cas =
          resource_index_or_invalid(resource_indices, "cas");
      if (cas != k_invalid_resource_index) {
        builder.export_image(make_cas_render_image_export(cas));
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

void render_passes_load(void *config, uint32_t config_size) {
  (void)config_size;
  auto *context = static_cast<RenderPassesBuildContext *>(config);

  for (const auto &pass_config : *context->pass_configs) {
    const auto pass_type =
        resolve_builtin_pass_type(pass_config.type, pass_config.id);
    auto pass_builder = context->builder->add_pass(
        create_manifest_frame_pass(
            pass_type,
            *context->skybox_cube,
            *context->fullscreen_quad,
            context->eye_adaptation_state,
            context->entity_pick_request
        ),
        builtin_pass_graph_type(pass_type)
    );

    for (const auto &dependency : pass_config.dependencies) {
      apply_manifest_dependency(pass_builder, dependency);
    }

    for (const auto &use : pass_config.uses) {
      const auto resource_index = require_resource_index(
          *context->resource_indices,
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
          *context->resource_indices,
          present.resource,
          pass_config.id.empty() ? pass_config.type : pass_config.id
      );
      pass_builder.present(
          resource_index,
          image_aspect_from_manifest(present.aspect)
      );
    }

    apply_eye_adaptation_resource_accesses(
        pass_builder, pass_type, *context->resource_indices
    );
    apply_builtin_exports(pass_builder, pass_type, *context->resource_indices);
  }
}

void render_passes_unload() {}

} // namespace

extern "C" __attribute__((visibility("default")))
const AstraModuleAPI *astra_get_module_api() {
  static AstraModuleAPI api{
      ASTRA_MODULE_API_VERSION,
      render_passes_load,
      render_passes_unload,
  };
  return &api;
}
