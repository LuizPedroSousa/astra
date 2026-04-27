#pragma once

#include "graph/heightmap/heightmap-pass.hpp"

namespace astralix::terrain {

class SplatPass : public HeightmapPass {
public:
  void process(HeightmapFrame &frame, const TerrainRecipeData &recipe) override;
  std::string_view name() const override { return "Splat"; }
  std::span<const HeightmapField> reads() const override { return s_reads; }
  std::span<const HeightmapField> writes() const override { return s_writes; }

private:
  static constexpr HeightmapField s_reads[] = {HeightmapField::Heightmap, HeightmapField::NormalMap};
  static constexpr HeightmapField s_writes[] = {HeightmapField::SplatMap};
};

} // namespace astralix::terrain
