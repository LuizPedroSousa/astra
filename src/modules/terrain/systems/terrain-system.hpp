#pragma once

#include "graph/terrain-graph.hpp"
#include "graph/heightmap/heightmap-subgraph.hpp"
#include "recipe/terrain-recipe-data.hpp"
#include "resources/mesh.hpp"
#include "project.hpp"
#include "systems/system.hpp"
#include <optional>
#include <string>
#include <unordered_map>

namespace astralix {

class TerrainSystem : public System<TerrainSystem> {
public:
  TerrainSystem(TerrainSystemConfig &config);
  ~TerrainSystem();

  void start() override;
  void fixed_update(double fixed_dt) override;
  void pre_update(double dt) override;
  void update(double dt) override;

  const terrain::TerrainGraph &graph() const;
  const terrain::HeightmapSubgraph *heightmap_subgraph() const;

private:
  void generate_terrain(const std::string &recipe_id);
  void upload_textures(const std::string &recipe_id, const terrain::HeightmapSubgraph &subgraph);
  Mesh build_terrain_mesh(const terrain::HeightmapFrame &frame, float height_scale);

  TerrainSystemConfig m_config;
  terrain::TerrainGraph m_graph;

  struct GeneratedTerrain {
    terrain::TerrainRecipeData recipe;
    bool gpu_uploaded = false;
    std::string heightmap_texture_id;
    std::string normalmap_texture_id;
    std::string splatmap_texture_id;
    std::optional<Mesh> mesh;
  };

  std::unordered_map<std::string, GeneratedTerrain> m_terrains;
};

} // namespace astralix
