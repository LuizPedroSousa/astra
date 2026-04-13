#pragma once

#include "graph/heightmap/heightmap-pass.hpp"

namespace astralix::terrain {

class MeshBuildPass : public HeightmapPass {
public:
  explicit MeshBuildPass(uint32_t clipmap_levels = 6, uint32_t ring_vertices = 64)
      : m_clipmap_levels(clipmap_levels), m_ring_vertices(ring_vertices) {}

  void process(HeightmapFrame &frame, const TerrainRecipeData &recipe) override;
  std::string_view name() const override { return "MeshBuild"; }
  std::span<const HeightmapField> reads() const override { return s_reads; }
  std::span<const HeightmapField> writes() const override { return s_writes; }

private:
  static constexpr HeightmapField s_reads[] = {HeightmapField::Heightmap, HeightmapField::NormalMap};
  static constexpr HeightmapField s_writes[] = {HeightmapField::MeshData};

  uint32_t m_clipmap_levels;
  uint32_t m_ring_vertices;
};

} // namespace astralix::terrain
