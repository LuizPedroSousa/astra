#pragma once

#include "assert.hpp"
#include "systems/render-system/core/render-types.hpp"

#include <cstdint>
#include <string_view>

namespace astralix {

enum class BuiltinPassType : uint8_t {
  Shadow,
  Forward,
  Geometry,
  GBufferBlend,
  SSAO,
  SSAOBlur,
  Lighting,
  SSR,
  SSRBlur,
  SSRComposite,
  SSGI,
  SSGIBlur,
  SSGITemporal,
  SSGIComposite,
  SSGIHistoryStore,
  DebugGBuffer,
  Terrain,
  Skybox,
  Grid,
  DebugOverlay,
  EntityPickReadback,
  Bloom,
  EyeAdaptationHistogram,
  EyeAdaptationAverage,
  DebugDraw,
  EditorGizmo,
  NavigationGizmo,
  PostProcess,
  UI,
  VolumetricFog,
  VolumetricBlurH,
  VolumetricBlurV,
  VolumetricTemporal,
  VolumetricHistoryStore,
  VolumetricComposite,
  LensFlare,
  LensFlareComposite,
  MotionBlur,
  MotionBlurComposite,
  ChromaticAberration,
  ChromaticAberrationComposite,
  Vignette,
  VignetteComposite,
  FilmGrain,
  FilmGrainComposite,
  DepthOfField,
  DepthOfFieldComposite,
  GodRays,
  GodRaysComposite,
  CAS,
  CASComposite,
  TAAResolve,
  TAAComposite,
  TAAHistoryStore,
};

inline BuiltinPassType builtin_pass_type_from_string(std::string_view type) {
  if (type == "builtin::shadow")
    return BuiltinPassType::Shadow;
  if (type == "builtin::forward")
    return BuiltinPassType::Forward;
  if (type == "builtin::geometry")
    return BuiltinPassType::Geometry;
  if (type == "builtin::gbuffer_blend")
    return BuiltinPassType::GBufferBlend;
  if (type == "builtin::ssao")
    return BuiltinPassType::SSAO;
  if (type == "builtin::ssao_blur")
    return BuiltinPassType::SSAOBlur;
  if (type == "builtin::lighting")
    return BuiltinPassType::Lighting;
  if (type == "builtin::ssr")
    return BuiltinPassType::SSR;
  if (type == "builtin::ssr_blur")
    return BuiltinPassType::SSRBlur;
  if (type == "builtin::ssr_composite")
    return BuiltinPassType::SSRComposite;
  if (type == "builtin::ssgi")
    return BuiltinPassType::SSGI;
  if (type == "builtin::ssgi_blur")
    return BuiltinPassType::SSGIBlur;
  if (type == "builtin::ssgi_temporal")
    return BuiltinPassType::SSGITemporal;
  if (type == "builtin::ssgi_composite")
    return BuiltinPassType::SSGIComposite;
  if (type == "builtin::ssgi_history_store")
    return BuiltinPassType::SSGIHistoryStore;
  if (type == "builtin::debug_gbuffer")
    return BuiltinPassType::DebugGBuffer;
  if (type == "builtin::terrain")
    return BuiltinPassType::Terrain;
  if (type == "builtin::skybox")
    return BuiltinPassType::Skybox;
  if (type == "builtin::grid")
    return BuiltinPassType::Grid;
  if (type == "builtin::debug_overlay")
    return BuiltinPassType::DebugOverlay;
  if (type == "builtin::entity_pick_readback")
    return BuiltinPassType::EntityPickReadback;
  if (type == "builtin::bloom")
    return BuiltinPassType::Bloom;
  if (type == "builtin::eye_adaptation_histogram")
    return BuiltinPassType::EyeAdaptationHistogram;
  if (type == "builtin::eye_adaptation_average")
    return BuiltinPassType::EyeAdaptationAverage;
  if (type == "builtin::debug_draw")
    return BuiltinPassType::DebugDraw;
  if (type == "builtin::editor_gizmo")
    return BuiltinPassType::EditorGizmo;
  if (type == "builtin::navigation_gizmo")
    return BuiltinPassType::NavigationGizmo;
  if (type == "builtin::post_process")
    return BuiltinPassType::PostProcess;
  if (type == "builtin::ui")
    return BuiltinPassType::UI;
  if (type == "builtin::volumetric_fog")
    return BuiltinPassType::VolumetricFog;
  if (type == "builtin::volumetric_blur_h")
    return BuiltinPassType::VolumetricBlurH;
  if (type == "builtin::volumetric_blur_v")
    return BuiltinPassType::VolumetricBlurV;
  if (type == "builtin::volumetric_temporal")
    return BuiltinPassType::VolumetricTemporal;
  if (type == "builtin::volumetric_history_store")
    return BuiltinPassType::VolumetricHistoryStore;
  if (type == "builtin::volumetric_composite")
    return BuiltinPassType::VolumetricComposite;
  if (type == "builtin::lens_flare")
    return BuiltinPassType::LensFlare;
  if (type == "builtin::lens_flare_composite")
    return BuiltinPassType::LensFlareComposite;
  if (type == "builtin::motion_blur")
    return BuiltinPassType::MotionBlur;
  if (type == "builtin::motion_blur_composite")
    return BuiltinPassType::MotionBlurComposite;
  if (type == "builtin::chromatic_aberration")
    return BuiltinPassType::ChromaticAberration;
  if (type == "builtin::chromatic_aberration_composite")
    return BuiltinPassType::ChromaticAberrationComposite;
  if (type == "builtin::vignette")
    return BuiltinPassType::Vignette;
  if (type == "builtin::vignette_composite")
    return BuiltinPassType::VignetteComposite;
  if (type == "builtin::film_grain")
    return BuiltinPassType::FilmGrain;
  if (type == "builtin::film_grain_composite")
    return BuiltinPassType::FilmGrainComposite;
  if (type == "builtin::depth_of_field")
    return BuiltinPassType::DepthOfField;
  if (type == "builtin::depth_of_field_composite")
    return BuiltinPassType::DepthOfFieldComposite;
  if (type == "builtin::god_rays")
    return BuiltinPassType::GodRays;
  if (type == "builtin::god_rays_composite")
    return BuiltinPassType::GodRaysComposite;
  if (type == "builtin::cas")
    return BuiltinPassType::CAS;
  if (type == "builtin::cas_composite")
    return BuiltinPassType::CASComposite;
  if (type == "builtin::taa_resolve")
    return BuiltinPassType::TAAResolve;
  if (type == "builtin::taa_composite")
    return BuiltinPassType::TAAComposite;
  if (type == "builtin::taa_history_store")
    return BuiltinPassType::TAAHistoryStore;
  ASTRA_EXCEPTION("Unknown builtin pass type: ", type);
}

inline BuiltinPassType resolve_builtin_pass_type(
    std::string_view type,
    std::string_view pass_id
) {
  if (type == "fullscreen") {
    if (pass_id == "ssao")
      return BuiltinPassType::SSAO;
    if (pass_id == "ssao_blur")
      return BuiltinPassType::SSAOBlur;
    if (pass_id == "lighting")
      return BuiltinPassType::Lighting;
    if (pass_id == "ssr")
      return BuiltinPassType::SSR;
    if (pass_id == "ssr_blur")
      return BuiltinPassType::SSRBlur;
    if (pass_id == "ssr_composite")
      return BuiltinPassType::SSRComposite;
    if (pass_id == "ssgi")
      return BuiltinPassType::SSGI;
    if (pass_id == "ssgi_blur")
      return BuiltinPassType::SSGIBlur;
    if (pass_id == "ssgi_temporal")
      return BuiltinPassType::SSGITemporal;
    if (pass_id == "ssgi_composite")
      return BuiltinPassType::SSGIComposite;
    if (pass_id == "ssgi_history_store")
      return BuiltinPassType::SSGIHistoryStore;
    if (pass_id == "bloom")
      return BuiltinPassType::Bloom;
    if (pass_id == "postprocess")
      return BuiltinPassType::PostProcess;
    ASTRA_EXCEPTION("Unsupported fullscreen render graph pass id: ", pass_id);
  }

  if (type == "builtin::postprocess") {
    return BuiltinPassType::PostProcess;
  }

  return builtin_pass_type_from_string(type);
}

inline RenderGraphPassType builtin_pass_graph_type(BuiltinPassType type) {
  switch (type) {
    case BuiltinPassType::EyeAdaptationHistogram:
    case BuiltinPassType::EyeAdaptationAverage:
      return RenderGraphPassType::Compute;
    default:
      return RenderGraphPassType::Graphics;
  }
}

} // namespace astralix
