#pragma once

#include "heightmap-frame.hpp"
#include "recipe/terrain-recipe-data.hpp"
#include <span>
#include <string_view>

namespace astralix::terrain {

class HeightmapPass {
public:
  virtual ~HeightmapPass() = default;

  virtual void process(HeightmapFrame &frame, const TerrainRecipeData &recipe) = 0;

  virtual std::string_view name() const = 0;
  virtual std::span<const HeightmapField> reads() const = 0;
  virtual std::span<const HeightmapField> writes() const = 0;

  bool enabled = true;
};

} // namespace astralix::terrain
