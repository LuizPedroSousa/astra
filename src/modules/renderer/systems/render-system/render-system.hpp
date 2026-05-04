#pragma once

#include "glm/glm.hpp"
#include "project.hpp"
#include "systems/render-system/eye-adaptation.hpp"
#include "systems/render-system/passes/entity-pick-readback-pass.hpp"
#include "systems/render-system/passes/render-graph.hpp"
#include "systems/render-system/render-frame.hpp"
#include "systems/render-system/render-image-export.hpp"
#include "systems/render-system/render-passes-build-context.hpp"
#ifdef ASTRA_RENDERER_HOT_RELOAD
#include "systems/render-system/shader-watcher.hpp"
#endif
#include "systems/system.hpp"
#include <deque>
#include <limits>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

struct AstraModuleAPI;

namespace astralix {

struct EntityPickResult {
  uint64_t frame_serial = 0;
  glm::ivec2 pixel{};
  std::optional<EntityID> entity_id;
};

class RenderSystem : public System<RenderSystem> {
public:
  RenderSystem(RenderSystemConfig &config);
  ~RenderSystem();

  void start() override;
  void fixed_update(double fixed_dt) override;
  void pre_update(double dt) override;
  void update(double dt) override;

  std::optional<ResolvedRenderImage>
  resolve_render_image(RenderImageExportKey key) const;
  std::optional<glm::ivec2> entity_selection_extent() const;
  void request_entity_pick(glm::ivec2 pixel);
  std::optional<EntityPickResult> consume_latest_entity_pick();
  const SSGIConfig &ssgi_config() const noexcept { return m_config.ssgi; }
  void set_ssgi_config(SSGIConfig config);
  void invalidate_ssgi_history();
  const SSRConfig &ssr_config() const noexcept { return m_config.ssr; }
  void set_ssr_config(SSRConfig config);
  const VolumetricFogConfig &volumetric_config() const noexcept { return m_config.volumetric; }
  void set_volumetric_config(VolumetricFogConfig config);
  const LensFlareConfig &lens_flare_config() const noexcept { return m_config.lens_flare; }
  void set_lens_flare_config(LensFlareConfig config);
  const EyeAdaptationConfig &eye_adaptation_config() const noexcept {
    return m_config.eye_adaptation;
  }
  void set_eye_adaptation_config(EyeAdaptationConfig config);
  const MotionBlurConfig &motion_blur_config() const noexcept {
    return m_config.motion_blur;
  }
  void set_motion_blur_config(MotionBlurConfig config);
  const ChromaticAberrationConfig &chromatic_aberration_config() const noexcept {
    return m_config.chromatic_aberration;
  }
  void set_chromatic_aberration_config(ChromaticAberrationConfig config);
  const VignetteConfig &vignette_config() const noexcept {
    return m_config.vignette;
  }
  void set_vignette_config(VignetteConfig config);
  const FilmGrainConfig &film_grain_config() const noexcept {
    return m_config.film_grain;
  }
  void set_film_grain_config(FilmGrainConfig config);
  const DepthOfFieldConfig &depth_of_field_config() const noexcept {
    return m_config.depth_of_field;
  }
  void set_depth_of_field_config(DepthOfFieldConfig config);
  const GodRaysConfig &god_rays_config() const noexcept {
    return m_config.god_rays;
  }
  void set_god_rays_config(GodRaysConfig config);
  const TAAConfig &taa_config() const noexcept {
    return m_config.taa;
  }
  void set_taa_config(TAAConfig config);
  const TonemappingConfig &tonemapping_config() const noexcept {
    return m_config.tonemapping;
  }
  void set_tonemapping_config(TonemappingConfig config);
  const CASConfig &cas_config() const noexcept {
    return m_config.cas;
  }
  void set_cas_config(CASConfig config);
  void set_render_graph_config(RenderGraphConfig config);

  const std::vector<RenderImageExportBinding> &
  render_image_exports() const noexcept {
    return m_render_image_exports;
  }

  const FrameStats &current_frame_stats() const {
    static const FrameStats k_empty;
    if (m_render_graph == nullptr) {
      return k_empty;
    }
    return m_render_graph->latest_frame_stats();
  }

  void prepare_for_pass_reload();
  void load_passes_from_module(const AstraModuleAPI *api);

private:
  void build_passes_inline();
  void build_graph_resources(RenderGraphBuilder &builder);
  void finalize_after_pass_load();
  struct PendingEntityPickRequest {
    glm::ivec2 pixel{};
  };

  struct PendingEntityPickSubmission {
    uint64_t frame_serial = 0;
    glm::ivec2 pixel{};
    std::shared_ptr<const std::vector<EntityID>> pick_id_lut;
    int raw_value = 0;
    bool ready = false;
  };

  void rebuild_render_image_exports();
  void ensure_pass_dependency_descriptors();
  void reset_render_graph_state();
  void drain_completed_entity_picks();
  void advance_temporal_history(const rendering::SceneFrame &scene_frame);
#ifdef ASTRA_RENDERER_HOT_RELOAD
  void initialize_shader_watcher();
  void poll_shader_reloads();
#endif
  void warm_async_pass_dependency_resources(RendererBackend backend);

  Ref<RenderTarget> m_render_target;
  RenderSystemConfig m_config;
  Scope<RenderGraph> m_render_graph;
  std::vector<RenderImageExportBinding> m_render_image_exports;
  uint32_t m_shadow_map_resource_index = std::numeric_limits<uint32_t>::max();
  uint32_t m_scene_color_resource_index = std::numeric_limits<uint32_t>::max();
  uint32_t m_scene_depth_resource_index = std::numeric_limits<uint32_t>::max();
  uint32_t m_bloom_extract_resource_index = std::numeric_limits<uint32_t>::max();
  uint32_t m_present_resource_index = std::numeric_limits<uint32_t>::max();
  uint32_t m_ssao_resource_index = std::numeric_limits<uint32_t>::max();
  uint32_t m_ssao_blur_resource_index = std::numeric_limits<uint32_t>::max();
  uint32_t m_ssgi_resource_index = std::numeric_limits<uint32_t>::max();
  uint32_t m_ssgi_blur_resource_index = std::numeric_limits<uint32_t>::max();
  uint32_t m_ssgi_temporal_resource_index = std::numeric_limits<uint32_t>::max();
  uint32_t m_ssgi_history_resource_index = std::numeric_limits<uint32_t>::max();
  uint32_t m_volumetric_fog_resource_index = std::numeric_limits<uint32_t>::max();
  uint32_t m_volumetric_blur_h_resource_index = std::numeric_limits<uint32_t>::max();
  uint32_t m_volumetric_blur_resource_index = std::numeric_limits<uint32_t>::max();
  uint32_t m_volumetric_temporal_resource_index = std::numeric_limits<uint32_t>::max();
  uint32_t m_volumetric_history_resource_index = std::numeric_limits<uint32_t>::max();
  uint32_t m_lens_flare_resource_index = std::numeric_limits<uint32_t>::max();
  uint32_t m_god_rays_resource_index = std::numeric_limits<uint32_t>::max();
  uint32_t m_eye_adaptation_histogram_resource_index = std::numeric_limits<uint32_t>::max();
  uint32_t m_eye_adaptation_exposure_resource_index = std::numeric_limits<uint32_t>::max();
  uint32_t m_bloom_resource_index = std::numeric_limits<uint32_t>::max();
  uint32_t m_g_position_resource_index = std::numeric_limits<uint32_t>::max();
  uint32_t m_g_normal_resource_index = std::numeric_limits<uint32_t>::max();
  uint32_t m_g_geometric_normal_resource_index = std::numeric_limits<uint32_t>::max();
  uint32_t m_g_albedo_resource_index = std::numeric_limits<uint32_t>::max();
  uint32_t m_g_emissive_resource_index = std::numeric_limits<uint32_t>::max();
  uint32_t m_g_entity_id_resource_index = std::numeric_limits<uint32_t>::max();
  uint32_t m_g_velocity_resource_index = std::numeric_limits<uint32_t>::max();
  uint32_t m_entity_pick_resource_index = std::numeric_limits<uint32_t>::max();
  uint64_t m_render_frame_serial = 0;
  std::optional<PendingEntityPickRequest> m_pending_entity_pick_request;
  std::deque<PendingEntityPickSubmission> m_pending_entity_pick_submissions;
  std::optional<EntityPickResult> m_latest_entity_pick_result;
  rendering::EntityPickReadbackRequest m_entity_pick_readback_request;
  rendering::RenderRuntimeStore m_render_runtime_store;
  rendering::CameraHistoryState m_camera_history;
  EyeAdaptationState m_eye_adaptation_state;
  rendering::ResolvedMeshDraw m_skybox_cube{};
  rendering::ResolvedMeshDraw m_fullscreen_quad{};
  std::unordered_map<std::string, uint32_t> m_resource_indices;
  bool m_brdf_lut_warmup_queued = false;
  std::shared_ptr<std::vector<unsigned char>> m_brdf_lut_pixels;
#ifdef ASTRA_RENDERER_HOT_RELOAD
  Scope<ShaderWatcher> m_shader_watcher;
#endif
};

} // namespace astralix
