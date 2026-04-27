#pragma once

#include "graph/heightmap/heightmap-pass.hpp"

namespace astralix::terrain {

class NoisePass : public HeightmapPass {
public:
  void process(HeightmapFrame &frame, const TerrainRecipeData &recipe) override;
  std::string_view name() const override { return "Noise"; }
  std::span<const HeightmapField> reads() const override { return {}; }
  std::span<const HeightmapField> writes() const override { return s_writes; }

private:
  static constexpr HeightmapField s_writes[] = {HeightmapField::Heightmap};
};

} // namespace astralix::terrain
