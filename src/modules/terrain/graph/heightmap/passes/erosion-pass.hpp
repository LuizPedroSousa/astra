#pragma once

#include "graph/heightmap/heightmap-pass.hpp"

namespace astralix::terrain {

class ErosionPass : public HeightmapPass {
public:
  void process(HeightmapFrame &frame, const TerrainRecipeData &recipe) override;
  std::string_view name() const override { return "Erosion"; }
  std::span<const HeightmapField> reads() const override { return s_reads; }
  std::span<const HeightmapField> writes() const override { return s_writes; }

private:
  static constexpr HeightmapField s_reads[] = {HeightmapField::Heightmap};
  static constexpr HeightmapField s_writes[] = {HeightmapField::Heightmap, HeightmapField::ErosionMap};
};

} // namespace astralix::terrain
