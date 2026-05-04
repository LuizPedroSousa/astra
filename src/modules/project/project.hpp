#pragma once
#include "assets/asset_binding.hpp"
#include "arena.hpp"
#include "assert.hpp"
#include "base.hpp"
#include "guid.hpp"
#include "renderer-api.hpp"
#include "serialization-context.hpp"
#include "serializers/project-serializer.hpp"
#include "targets/render-target.hpp"
#include <cstdint>
#include <filesystem>
#include <glm/ext/vector_float3.hpp>
#include <map>
#include <optional>
#include <string>
#include <functional>
#include <string_view>

namespace astralix {

class ProjectSerializer;
class AssetRegistry;

#define RESOURCE_INIT_PARAMS const ResourceHandle &id

struct ProjectSerializationConfig {
  SerializationFormat format = SerializationFormat::Json;

  SerializationFormat formatFromString(const std::string &format) {
    static const std::map<std::string, SerializationFormat> formats = {
        {"json", SerializationFormat::Json},
        {"toml", SerializationFormat::Toml},
        {"yaml", SerializationFormat::Yaml},
        {"xml", SerializationFormat::Xml},
    };

    auto it = formats.find(format);

    ASTRA_ENSURE_WITH_SUGGESTIONS(it == formats.end(), formats, format, "serialization format", "ProjectSerialization")

    return it->second;
  }
};

struct ProjectResourceConfig {
  std::string directory;
  std::vector<AssetBindingConfig> asset_bindings;
};

struct ProjectSceneEntryConfig {
  std::string id;
  std::string type;
  std::string source_path;
  std::string preview_path;
  std::string runtime_path;
};

enum class SceneStartupTarget {
  Source,
  Preview,
  Runtime,
};

struct ProjectScenesConfig {
  std::string startup;
  SceneStartupTarget startup_target = SceneStartupTarget::Source;
  std::vector<ProjectSceneEntryConfig> entries;
};

struct WindowConfig {
  WindowID id;
  std::string title;
  bool headless;
  int height;
  int width;
};

enum SystemType {
  Physics,
  Render,
  Audio,
  Terrain
};

struct PhysicsSystemConfig {
  std::string backend;
  glm::vec3 gravity;
  std::string pvd_host = "127.0.0.1";
  int pvd_port;
  int pvd_timeout = 5;
};

struct MSAAConfig {
  int samples;
  bool is_enabled;
};

struct SSAOConfig {
  bool full_resolution = true;
};

struct SSGIConfig {
  bool enabled = true;
  bool full_resolution = false;
  float intensity = 1.0f;
  float radius = 1.5f;
  float thickness = 0.15f;
  int directions = 8;
  int steps_per_direction = 2;
  float max_distance = 2.0f;
  bool temporal = true;
  float history_weight = 0.9f;
  float normal_reject_dot = 0.9f;
  float position_reject_distance = 0.2f;
};

struct SSRConfig {
  bool enabled = false;
  float intensity = 1.0f;
  float max_distance = 50.0f;
  float thickness = 0.1f;
  int max_steps = 64;
  float stride = 1.0f;
  float roughness_cutoff = 0.7f;
};

struct VolumetricFogConfig {
  bool enabled = false;
  int max_steps = 32;
  float density = 0.02f;
  float scattering = 0.7f;
  float max_distance = 50.0f;
  float intensity = 1.0f;
  float fog_base_height = 0.0f;
  float height_falloff_rate = 0.5f;
  float noise_scale = 0.02f;
  float noise_weight = 0.3f;
  glm::vec3 wind_direction = glm::vec3(1.0f, 0.0f, 0.0f);
  float wind_speed = 0.5f;
  bool temporal = true;
  float temporal_blend_weight = 0.85f;
};

struct LensFlareConfig {
  bool enabled = false;
  float intensity = 1.0f;
  float threshold = 0.8f;
  int ghost_count = 4;
  float ghost_dispersal = 0.35f;
  float ghost_weight = 0.5f;
  float halo_radius = 0.6f;
  float halo_weight = 0.25f;
  float halo_thickness = 0.1f;
  float chromatic_aberration = 0.02f;
};

struct EyeAdaptationConfig {
  bool enabled = false;
  float min_log_luminance = -8.0f;
  float max_log_luminance = 4.0f;
  float adaptation_speed_up = 3.0f;
  float adaptation_speed_down = 1.0f;
  float key_value = 0.18f;
  float low_percentile = 0.05f;
  float high_percentile = 0.95f;
};

struct MotionBlurConfig {
  bool enabled = false;
  float intensity = 1.0f;
  int max_samples = 16;
  float depth_threshold = 5.0f;
};

struct ChromaticAberrationConfig {
  bool enabled = false;
  float intensity = 0.005f;
};

struct VignetteConfig {
  bool enabled = false;
  float intensity = 0.3f;
  float smoothness = 0.5f;
  float roundness = 1.0f;
};

struct FilmGrainConfig {
  bool enabled = false;
  float intensity = 0.1f;
};

struct DepthOfFieldConfig {
  bool enabled = false;
  float focus_distance = 10.0f;
  float focus_range = 5.0f;
  float max_blur_radius = 5.0f;
  int sample_count = 16;
};

enum class TonemapOperator : int {
  Reinhard = 0,
  AgX = 1,
  ACES = 2,
};

struct TonemappingConfig {
  TonemapOperator tonemap_operator = TonemapOperator::AgX;
  float gamma = 2.2f;
  float bloom_strength = 0.12f;
};

struct CASConfig {
  bool enabled = false;
  float sharpness = 0.5f;
  float contrast = 0.0f;
  float sharpening_limit = 0.25f;
};

struct TAAConfig {
  bool enabled = false;
  float blend_factor = 0.1f;
};

struct GodRaysConfig {
  bool enabled = false;
  float intensity = 1.0f;
  float decay = 0.96f;
  float density = 0.5f;
  float weight = 0.4f;
  float threshold = 0.8f;
  int samples = 64;
};

struct SceneRenderOverrides {
  std::optional<SSGIConfig> ssgi;
  std::optional<SSRConfig> ssr;
  std::optional<VolumetricFogConfig> volumetric;
  std::optional<LensFlareConfig> lens_flare;
  std::optional<EyeAdaptationConfig> eye_adaptation;
  std::optional<MotionBlurConfig> motion_blur;
  std::optional<ChromaticAberrationConfig> chromatic_aberration;
  std::optional<VignetteConfig> vignette;
  std::optional<FilmGrainConfig> film_grain;
  std::optional<DepthOfFieldConfig> depth_of_field;
  std::optional<GodRaysConfig> god_rays;
  std::optional<CASConfig> cas;
  std::optional<TAAConfig> taa;
  std::optional<TonemappingConfig> tonemapping;

  bool empty() const noexcept {
    return !ssgi && !ssr && !volumetric && !lens_flare && !eye_adaptation &&
           !motion_blur && !chromatic_aberration && !vignette && !film_grain &&
           !depth_of_field && !god_rays && !cas && !taa && !tonemapping;
  }
};

enum class RenderGraphSizeMode {
  Absolute,
  WindowRelative,
};

enum class RenderGraphPassDependencyKind : uint8_t {
  Shader,
  Texture2D,
  Texture3D,
  Material,
  Model,
  Font,
  Svg,
  AudioClip,
  TerrainRecipe,
};

struct RenderGraphSizeConfig {
  RenderGraphSizeMode mode = RenderGraphSizeMode::Absolute;
  uint32_t width = 0;
  uint32_t height = 0;
  float scale_x = 1.0f;
  float scale_y = 1.0f;
  bool defined = false;
};

struct RenderGraphResourceConfig {
  std::string name;
  std::string format;
  RenderGraphSizeConfig size;
  std::vector<std::string> usage;
  std::string lifetime = "transient";
};

struct RenderGraphPassDependencyConfig {
  RenderGraphPassDependencyKind kind =
      RenderGraphPassDependencyKind::Shader;
  std::string slot;
  ResourceDescriptorID descriptor_id;
};

struct RenderGraphPassUseConfig {
  std::string resource;
  std::string aspect;
  std::string usage;
};

struct RenderGraphPassPresentConfig {
  std::string resource;
  std::string aspect = "color0";
};

struct RenderGraphPassConfig {
  std::string id;
  std::string type;
  std::vector<RenderGraphPassDependencyConfig> dependencies;
  std::vector<RenderGraphPassUseConfig> uses;
  std::optional<RenderGraphPassPresentConfig> present;
};

struct RenderGraphConfig {
  std::vector<RenderGraphResourceConfig> resources;
  std::vector<RenderGraphPassConfig> passes;

  bool is_defined() const noexcept {
    return !resources.empty() || !passes.empty();
  }
};

struct RenderSystemConfig {
  std::string backend;
  std::string strategy = "deferred";
  MSAAConfig msaa;
  SSAOConfig ssao;
  SSGIConfig ssgi;
  SSRConfig ssr;
  VolumetricFogConfig volumetric;
  LensFlareConfig lens_flare;
  EyeAdaptationConfig eye_adaptation;
  MotionBlurConfig motion_blur;
  ChromaticAberrationConfig chromatic_aberration;
  VignetteConfig vignette;
  FilmGrainConfig film_grain;
  DepthOfFieldConfig depth_of_field;
  GodRaysConfig god_rays;
  CASConfig cas;
  TAAConfig taa;
  TonemappingConfig tonemapping;
  std::string window_id;
  bool headless = false;
  RenderGraphConfig render_graph;

  RendererBackend backend_to_api() {
    if (backend == "opengl")
      return RendererBackend::OpenGL;
    if (backend == "vulkan")
      return RendererBackend::Vulkan;

    return RendererBackend::None;
  }

  RenderTarget::MSAA msaa_to_render_target_msaa() {
    return {.samples = msaa.samples, .is_enabled = msaa.is_enabled};
  }
};

struct AudioSystemConfig {
  std::string backend = "miniaudio";
  float master_gain = 1.0f;
};

struct TerrainSystemConfig {
  uint32_t default_resolution = 1025;
  uint32_t clipmap_levels = 6;
  float tile_world_size = 256.0f;
};

struct SystemConfig {
  std::string name;
  SystemType type;
  std::variant<std::monostate, PhysicsSystemConfig, RenderSystemConfig, AudioSystemConfig, TerrainSystemConfig> content;
};

struct ProjectConfig {
  std::string name = "Untitled";
  std::string directory;

  std::string manifest;

  std::vector<WindowConfig> windows;

  std::vector<SystemConfig> systems;

  ProjectResourceConfig resources;
  ProjectSerializationConfig serialization;
  ProjectScenesConfig scenes;
};

class Project {
public:
  ProjectID get_project_id() const { return m_project_id; }

  static Ref<Project> create(ProjectConfig config);
  AssetRegistry *asset_registry() const { return m_asset_registry.get(); }
  ProjectConfig &get_config() { return m_config; }
  const ProjectConfig &get_config() const { return m_config; }
  void save(ElasticArena &arena);
  void load(ElasticArena &arena);
  void reload_manifest();
  std::filesystem::path manifest_path() const;
  std::filesystem::path resolve_path(
      const std::filesystem::path &relative_path
  ) const;
  const ProjectSceneEntryConfig *find_scene_entry(std::string_view id) const;
  Project(ProjectConfig config);

  using ManifestReloadCallback = std::function<void(const ProjectConfig &)>;
  void on_manifest_reload(ManifestReloadCallback callback);

private:
  ProjectConfig m_config;
  ProjectID m_project_id;
  Scope<ProjectSerializer> m_serializer;
  Scope<AssetRegistry> m_asset_registry;
  std::vector<ManifestReloadCallback> m_manifest_reload_callbacks;
};

} // namespace astralix
