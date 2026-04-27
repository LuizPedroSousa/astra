#include "terrain-system.hpp"
#include "components/terrain-tile.hpp"
#include "components/material.hpp"
#include "components/mesh.hpp"
#include "components/tags.hpp"
#include "graph/heightmap/heightmap-subgraph.hpp"
#include "graph/heightmap/passes/erosion-pass.hpp"
#include "graph/heightmap/passes/mesh-build-pass.hpp"
#include "graph/heightmap/passes/noise-pass.hpp"
#include "graph/heightmap/passes/normal-pass.hpp"
#include "graph/heightmap/passes/splat-pass.hpp"
#include "managers/path-manager.hpp"
#include "managers/resource-manager.hpp"
#include "managers/scene-manager.hpp"
#include "resources/descriptors/terrain-recipe-descriptor.hpp"
#include "resources/texture.hpp"
#include "log.hpp"
#include "trace.hpp"

namespace astralix {

TerrainSystem::TerrainSystem(TerrainSystemConfig &config)
    : m_config(config) {}

void TerrainSystem::start() {
  ASTRA_PROFILE_N("TerrainSystem::start");

  auto heightmap = create_scope<terrain::HeightmapSubgraph>();
  heightmap->add_pass(create_scope<terrain::NoisePass>());
  heightmap->add_pass(create_scope<terrain::ErosionPass>());
  heightmap->add_pass(create_scope<terrain::NormalPass>());
  heightmap->add_pass(create_scope<terrain::SplatPass>());
  heightmap->add_pass(create_scope<terrain::MeshBuildPass>(
      m_config.clipmap_levels, 64));

  m_graph.add_subgraph(std::move(heightmap));
  m_graph.compile();
}

void TerrainSystem::fixed_update(double fixed_dt) {}

void TerrainSystem::pre_update(double dt) {}

void TerrainSystem::update(double dt) {
  ASTRA_PROFILE_N("TerrainSystem::update");

  auto active_scene = SceneManager::get()->get_active_scene();
  if (active_scene == nullptr) return;

  auto &world = active_scene->world();

  struct PendingAttach {
    EntityID entity_id;
    std::string recipe_id;
    float height_scale;
  };
  std::vector<PendingAttach> pending;

  world.each<terrain::TerrainTile>(
      [&](EntityID entity_id, terrain::TerrainTile &tile) {
        if (!tile.enabled || tile.recipe_id.empty()) return;
        if (!world.active(entity_id)) return;

        if (!m_terrains.contains(tile.recipe_id)) {
          generate_terrain(tile.recipe_id);
        }

        auto &generated = m_terrains[tile.recipe_id];
        if (!generated.gpu_uploaded) {
          auto *subgraph = heightmap_subgraph();
          if (subgraph != nullptr) {
            upload_textures(tile.recipe_id, *subgraph);
          }
        }

        auto entity = world.entity(entity_id);
        bool has_mesh_set = entity.get<rendering::MeshSet>() != nullptr;
        bool has_mesh = generated.mesh.has_value();

        if (!has_mesh_set && has_mesh) {
          pending.push_back({entity_id, tile.recipe_id, tile.height_scale});
        }
      });

  for (auto &entry : pending) {
    auto entity = world.entity(entry.entity_id);
    auto &generated = m_terrains[entry.recipe_id];

    entity.emplace<rendering::Renderable>();
    entity.emplace<rendering::ShadowCaster>();
    entity.emplace<rendering::MeshSet>(rendering::MeshSet{
        .meshes = {*generated.mesh},
    });
    entity.emplace<rendering::ShaderBinding>(rendering::ShaderBinding{
        .shader = "shaders::g_buffer",
    });
    entity.emplace<rendering::MaterialSlots>(rendering::MaterialSlots{
        .materials = {"materials::brick"},
    });
  }
}

void TerrainSystem::generate_terrain(const std::string &recipe_id) {
  ASTRA_PROFILE_N("TerrainSystem::generate_terrain");

  auto descriptor =
      resource_manager()->get_descriptor_by_id<TerrainRecipeDescriptor>(recipe_id);

  if (descriptor == nullptr) {
    LOG_ERROR("[TERRAIN] unknown recipe_id", recipe_id);
    return;
  }

  std::string resolved_path = path_manager()->resolve(descriptor->path).string();
  terrain::TerrainRecipeData recipe = terrain::parse_terrain_recipe(resolved_path);

  auto *subgraph = const_cast<terrain::HeightmapSubgraph *>(heightmap_subgraph());
  if (subgraph == nullptr) return;

  subgraph->set_recipe(recipe);

  m_graph.process();

  const auto &frame = subgraph->frame();

  GeneratedTerrain generated;
  generated.recipe = recipe;
  generated.heightmap_texture_id = "terrain::" + recipe_id + "::heightmap";
  generated.normalmap_texture_id = "terrain::" + recipe_id + "::normalmap";
  generated.splatmap_texture_id = "terrain::" + recipe_id + "::splatmap";
  generated.mesh = build_terrain_mesh(frame, frame.height_scale);

  m_terrains.emplace(recipe_id, std::move(generated));

  upload_textures(recipe_id, *subgraph);
}

Mesh TerrainSystem::build_terrain_mesh(const terrain::HeightmapFrame &frame,
                                        float height_scale) {
  ASTRA_PROFILE_N("TerrainSystem::build_terrain_mesh");

  uint32_t resolution = frame.resolution;
  float world_size = frame.tile_world_size;
  float texel_world = world_size / static_cast<float>(resolution - 1);

  std::vector<Vertex> vertices;
  vertices.reserve(static_cast<size_t>(resolution) * resolution);

  for (uint32_t y = 0; y < resolution; ++y) {
    for (uint32_t x = 0; x < resolution; ++x) {
      size_t index = y * resolution + x;

      float world_x = static_cast<float>(x) * texel_world - world_size * 0.5f;
      float world_z = static_cast<float>(y) * texel_world - world_size * 0.5f;
      float height = frame.heightmap[index] * height_scale;

      glm::vec3 normal = glm::vec3(frame.normal_map[index]);
      float u = static_cast<float>(x) / static_cast<float>(resolution - 1);
      float v = static_cast<float>(y) / static_cast<float>(resolution - 1);

      vertices.push_back(Vertex{
          .position = glm::vec3(world_x, height, world_z),
          .normal = normal,
          .texture_coordinates = glm::vec2(u, v),
          .tangent = glm::vec3(0.0f),
      });
    }
  }

  std::vector<unsigned int> indices;
  indices.reserve(static_cast<size_t>(resolution - 1) * (resolution - 1) * 6);

  for (uint32_t y = 0; y < resolution - 1; ++y) {
    for (uint32_t x = 0; x < resolution - 1; ++x) {
      unsigned int top_left = y * resolution + x;
      unsigned int top_right = top_left + 1;
      unsigned int bottom_left = (y + 1) * resolution + x;
      unsigned int bottom_right = bottom_left + 1;

      indices.push_back(top_left);
      indices.push_back(bottom_left);
      indices.push_back(top_right);

      indices.push_back(top_right);
      indices.push_back(bottom_left);
      indices.push_back(bottom_right);
    }
  }

  return Mesh(std::move(vertices), std::move(indices));
}

void TerrainSystem::upload_textures(const std::string &recipe_id,
                                     const terrain::HeightmapSubgraph &subgraph) {
  ASTRA_PROFILE_N("TerrainSystem::upload_textures");

  auto iterator = m_terrains.find(recipe_id);
  if (iterator == m_terrains.end()) return;

  auto &generated = iterator->second;
  const auto &frame = subgraph.frame();
  uint32_t resolution = frame.resolution;

  Texture2D::create(generated.heightmap_texture_id, TextureConfig{
      .width = resolution,
      .height = resolution,
      .bitmap = false,
      .format = TextureFormat::Red,
      .parameters = {
          {TextureParameter::WrapS, TextureValue::ClampToEdge},
          {TextureParameter::WrapT, TextureValue::ClampToEdge},
          {TextureParameter::MagFilter, TextureValue::Linear},
          {TextureParameter::MinFilter, TextureValue::Linear},
      },
      .buffer = reinterpret_cast<unsigned char *>(
          const_cast<float *>(frame.heightmap.data())),
  });

  Texture2D::create(generated.normalmap_texture_id, TextureConfig{
      .width = resolution,
      .height = resolution,
      .bitmap = false,
      .format = TextureFormat::RGBA,
      .parameters = {
          {TextureParameter::WrapS, TextureValue::ClampToEdge},
          {TextureParameter::WrapT, TextureValue::ClampToEdge},
          {TextureParameter::MagFilter, TextureValue::Linear},
          {TextureParameter::MinFilter, TextureValue::Linear},
      },
      .buffer = reinterpret_cast<unsigned char *>(
          const_cast<glm::vec4 *>(frame.normal_map.data())),
  });

  Texture2D::create(generated.splatmap_texture_id, TextureConfig{
      .width = resolution,
      .height = resolution,
      .bitmap = false,
      .format = TextureFormat::RGBA,
      .parameters = {
          {TextureParameter::WrapS, TextureValue::ClampToEdge},
          {TextureParameter::WrapT, TextureValue::ClampToEdge},
          {TextureParameter::MagFilter, TextureValue::Linear},
          {TextureParameter::MinFilter, TextureValue::Linear},
      },
      .buffer = reinterpret_cast<unsigned char *>(
          const_cast<glm::u8vec4 *>(frame.splat_map.data())),
  });

  generated.gpu_uploaded = true;
}

const terrain::TerrainGraph &TerrainSystem::graph() const { return m_graph; }

const terrain::HeightmapSubgraph *TerrainSystem::heightmap_subgraph() const {
  auto *subgraph = m_graph.find_subgraph("Heightmap");
  return static_cast<const terrain::HeightmapSubgraph *>(subgraph);
}

TerrainSystem::~TerrainSystem() {
  m_terrains.clear();
}

} // namespace astralix
