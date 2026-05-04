#pragma once

#include "components/material.hpp"
#include "components/mesh.hpp"
#include "components/tags.hpp"
#include "components/text.hpp"
#include "glm/glm.hpp"
#include "renderer-api.hpp"
#include "resources/font.hpp"
#include "resources/shader.hpp"
#include "resources/svg.hpp"
#include "resources/texture.hpp"
#include "types.hpp"
#include "world.hpp"
#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace astralix::rendering {

inline constexpr float k_default_directional_shadow_extent = 10.0f;
inline constexpr float k_default_directional_shadow_near_plane = 1.0f;
inline constexpr float k_default_directional_shadow_far_plane = 100.0f;
inline constexpr size_t k_max_point_lights = 4u;
inline constexpr size_t k_shadow_cascade_count = 4u;

struct CameraFrame {
  EntityID entity_id{};
  glm::vec3 position = glm::vec3(0.0f);
  glm::vec3 forward = glm::vec3(0.0f, 0.0f, -1.0f);
  glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
  glm::mat4 view = glm::mat4(1.0f);
  glm::mat4 projection = glm::mat4(1.0f);
  glm::mat4 previous_view = glm::mat4(1.0f);
  glm::mat4 previous_projection = glm::mat4(1.0f);
  bool has_history = false;
  bool orthographic = false;
  float fov_degrees = 45.0f;
  float orthographic_scale = 10.0f;
  float near_plane = 0.1f;
  float far_plane = 100.0f;
};

struct CameraHistoryState {
  EntityID entity_id{};
  glm::mat4 previous_view = glm::mat4(1.0f);
  glm::mat4 previous_projection = glm::mat4(1.0f);
  bool valid = false;
};

struct DirectionalLightPacket {
  bool valid = false;
  glm::vec3 position = glm::vec3(-4.0f, 8.0f, -3.0f);
  glm::vec3 direction = glm::vec3(0.0f, 0.0f, -1.0f);
  glm::vec3 ambient = glm::vec3(0.2f);
  glm::vec3 diffuse = glm::vec3(0.5f);
  glm::vec3 specular = glm::vec3(0.5f);
  glm::mat4 light_space_matrix = glm::mat4(1.0f);
  float near_plane = k_default_directional_shadow_near_plane;
  float far_plane = k_default_directional_shadow_far_plane;
  std::array<glm::mat4, k_shadow_cascade_count> cascade_matrices{};
  std::array<float, k_shadow_cascade_count> cascade_split_depths{};
  bool cascades_valid = false;
  float shadow_intensity = 1.0f;
};

struct PointLightPacket {
  bool valid = false;
  glm::vec3 position = glm::vec3(0.0f);
  glm::vec3 ambient = glm::vec3(0.0f);
  glm::vec3 diffuse = glm::vec3(0.0f);
  glm::vec3 specular = glm::vec3(0.0f);
  float constant = 1.0f;
  float linear = 0.045f;
  float quadratic = 0.0075f;
};

struct SpotLightPacket {
  bool valid = false;
  glm::vec3 position = glm::vec3(0.0f);
  glm::vec3 direction = glm::vec3(0.0f, 0.0f, -1.0f);
  glm::vec3 ambient = glm::vec3(0.0f);
  glm::vec3 diffuse = glm::vec3(0.0f);
  glm::vec3 specular = glm::vec3(0.0f);
  float inner_cutoff_cos = 0.0f;
  float outer_cutoff_cos = 0.0f;
  float constant = 1.0f;
  float linear = 0.045f;
  float quadratic = 0.0075f;
};

struct LightFrameData {
  DirectionalLightPacket directional;
  std::array<PointLightPacket, k_max_point_lights> point_lights{};
  SpotLightPacket spot;
};

struct ResolvedTextureBinding {
  ResourceDescriptorID descriptor_id;
  std::string name;
  Ref<Texture> texture = nullptr;
  bool cubemap = false;
};

struct ResolvedMaterialData {
  ResourceDescriptorID material_id;
  ResourceDescriptorID base_color_descriptor_id;
  ResourceDescriptorID normal_descriptor_id;
  ResourceDescriptorID metallic_descriptor_id;
  ResourceDescriptorID roughness_descriptor_id;
  ResourceDescriptorID metallic_roughness_descriptor_id;
  ResourceDescriptorID occlusion_descriptor_id;
  ResourceDescriptorID emissive_descriptor_id;
  ResourceDescriptorID displacement_descriptor_id;
  Ref<Texture> base_color = nullptr;
  Ref<Texture> normal = nullptr;
  Ref<Texture> metallic = nullptr;
  Ref<Texture> roughness = nullptr;
  Ref<Texture> metallic_roughness = nullptr;
  Ref<Texture> occlusion = nullptr;
  Ref<Texture> emissive = nullptr;
  Ref<Texture> displacement = nullptr;
  glm::vec4 base_color_factor = glm::vec4(1.0f);
  glm::vec3 emissive_factor = glm::vec3(0.0f);
  float metallic_factor = 1.0f;
  float roughness_factor = 1.0f;
  float occlusion_strength = 1.0f;
  float normal_scale = 1.0f;
  float height_scale = 0.02f;
  float bloom_intensity = 0.0f;
  bool alpha_mask = false;
  bool alpha_blend = false;
  float alpha_cutoff = 0.5f;
  bool double_sided = false;
  std::vector<ResolvedTextureBinding> extra_textures;
};

struct ResolvedMeshDraw {
  MeshID mesh_id{};
  ResourceDescriptorID source_model_id;
  uint32_t submesh_index = 0;
  Ref<VertexArray> vertex_array = nullptr;
  RendererAPI::DrawPrimitive draw_type =
      RendererAPI::DrawPrimitive::TRIANGLES;
  uint32_t index_count = 0;
  AABB local_bounds;
};

struct SurfaceDrawItem {
  EntityID entity_id{};
  uint32_t pick_id = 0;
  ResourceDescriptorID shader_id;
  Ref<Shader> shader = nullptr;
  ResolvedMaterialData material{};
  ResolvedMeshDraw mesh{};
  glm::mat4 model = glm::mat4(1.0f);
  glm::mat4 previous_model = glm::mat4(1.0f);
  bool has_previous_model = false;
  bool bloom_enabled = false;
  int bloom_layer = 0;
  bool casts_shadow = true;
  uint64_t sort_key = 0;
};

struct ShadowDrawItem {
  EntityID entity_id{};
  glm::mat4 model = glm::mat4(1.0f);
  ResolvedMeshDraw mesh{};
  uint64_t sort_key = 0;
};

struct SkyboxFrame {
  EntityID entity_id{};
  ResourceDescriptorID shader_id;
  ResourceDescriptorID cubemap_id;
  Ref<Shader> shader = nullptr;
  Ref<Texture3D> cubemap = nullptr;
  Ref<Texture3D> irradiance_cubemap = nullptr;
};

struct TextDrawItem {
  EntityID entity_id{};
  TextSprite sprite;
  Ref<Font> font = nullptr;
  uint32_t glyph_pixel_size = 48;
};

struct UIRootDrawList {
  EntityID entity_id{};
  int sort_order = 0;
  std::vector<ui::UIDrawCommand> commands;
};

struct ResolvedUIResources {
  std::unordered_map<ResourceDescriptorID, Ref<Texture2D>> textures;
  std::unordered_map<ResourceDescriptorID, Ref<Svg>> svgs;
  std::unordered_map<ResourceDescriptorID, Ref<Font>> fonts;
};

struct SSGIFrameSettings {
  bool enabled = true;
  bool full_resolution = false;
  bool temporal = true;
  float intensity = 1.0f;
  float radius = 1.5f;
  float thickness = 0.15f;
  int directions = 8;
  int steps_per_direction = 2;
  float max_distance = 2.0f;
  float history_weight = 0.9f;
  float normal_reject_dot = 0.9f;
  float position_reject_distance = 0.2f;
};

struct SSRFrameSettings {
  bool enabled = false;
  float intensity = 1.0f;
  float max_distance = 50.0f;
  float thickness = 0.1f;
  int max_steps = 64;
  float stride = 1.0f;
  float roughness_cutoff = 0.7f;
};

struct VolumetricFogFrameSettings {
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
  bool temporal_enabled = true;
  float temporal_blend_weight = 0.85f;
};

struct LensFlareFrameSettings {
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

struct EyeAdaptationFrameSettings {
  bool enabled = false;
  float min_log_luminance = -8.0f;
  float max_log_luminance = 4.0f;
  float adaptation_speed_up = 3.0f;
  float adaptation_speed_down = 1.0f;
  float key_value = 0.18f;
  float low_percentile = 0.05f;
  float high_percentile = 0.95f;
};

struct MotionBlurFrameSettings {
  bool enabled = false;
  float intensity = 1.0f;
  int max_samples = 16;
  float depth_threshold = 5.0f;
};

struct ChromaticAberrationFrameSettings {
  bool enabled = false;
  float intensity = 0.005f;
};

struct VignetteFrameSettings {
  bool enabled = false;
  float intensity = 0.3f;
  float smoothness = 0.5f;
  float roundness = 1.0f;
};

struct FilmGrainFrameSettings {
  bool enabled = false;
  float intensity = 0.1f;
  float time = 0.0f;
};

struct DepthOfFieldFrameSettings {
  bool enabled = false;
  float focus_distance = 10.0f;
  float focus_range = 5.0f;
  float max_blur_radius = 5.0f;
  int sample_count = 16;
};

struct CASFrameSettings {
  bool enabled = false;
  float sharpness = 0.5f;
  float contrast = 0.0f;
  float sharpening_limit = 0.25f;
};

struct TAAFrameSettings {
  bool enabled = false;
  float blend_factor = 0.1f;
  glm::vec2 jitter = glm::vec2(0.0f);
};

struct TonemappingFrameSettings {
  int tonemap_operator = 1;
  float gamma = 2.2f;
  float bloom_strength = 0.12f;
};

struct GodRaysFrameSettings {
  bool enabled = false;
  float intensity = 1.0f;
  float decay = 0.96f;
  float density = 0.5f;
  float weight = 0.4f;
  float threshold = 0.8f;
  int samples = 64;
};

struct SceneFrame {
  std::optional<CameraFrame> main_camera;
  LightFrameData light_frame{};
  std::optional<SkyboxFrame> skybox;
  std::vector<SurfaceDrawItem> opaque_surfaces;
  std::vector<SurfaceDrawItem> blend_surfaces;
  std::vector<ShadowDrawItem> shadow_draws;
  std::vector<TextDrawItem> text_items;
  std::vector<UIRootDrawList> ui_roots;
  ResolvedUIResources ui_resources;
  std::vector<EntityID> pick_id_lut;
  SSGIFrameSettings ssgi{};
  SSRFrameSettings ssr{};
  VolumetricFogFrameSettings volumetric{};
  LensFlareFrameSettings lens_flare{};
  EyeAdaptationFrameSettings eye_adaptation{};
  MotionBlurFrameSettings motion_blur{};
  ChromaticAberrationFrameSettings chromatic_aberration{};
  VignetteFrameSettings vignette{};
  FilmGrainFrameSettings film_grain{};
  DepthOfFieldFrameSettings depth_of_field{};
  GodRaysFrameSettings god_rays{};
  CASFrameSettings cas{};
  TAAFrameSettings taa{};
  TonemappingFrameSettings tonemapping{};

  [[nodiscard]] bool empty() const noexcept {
    return !main_camera.has_value() && !skybox.has_value() &&
           opaque_surfaces.empty() && shadow_draws.empty() &&
           text_items.empty() && ui_roots.empty();
  }
};

struct RenderRuntimeState {
  uint64_t surface_sort_key = 0;
  glm::mat4 previous_model = glm::mat4(1.0f);
  bool has_previous_model = false;
  bool initialized = false;
};

struct RenderRuntimeStore {
  std::unordered_map<EntityID, RenderRuntimeState> entity_states;

  void prune(const ecs::World &world) {
    std::erase_if(entity_states, [&](const auto &entry) {
      return !world.contains(entry.first);
    });
  }
};

inline size_t hash_combine(size_t seed, size_t value) {
  return seed ^ (value + 0x9e3779b9 + (seed << 6u) + (seed >> 2u));
}

inline uint64_t compute_surface_sort_key(const ResourceDescriptorID &shader_id, const ResourceDescriptorID &material_id, MeshID mesh_id) {
  size_t seed = std::hash<std::string>{}(shader_id);
  seed = hash_combine(seed, std::hash<std::string>{}(material_id));
  seed = hash_combine(seed, std::hash<size_t>{}(mesh_id));
  return static_cast<uint64_t>(seed);
}

} // namespace astralix::rendering
