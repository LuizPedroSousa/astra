#pragma once

#include "types.hpp"

namespace astralix::ui {

struct UIPathTessellationOptions {
  float curve_flatness = 0.25f;
};

struct UITessellatedPath {
  std::vector<UIPolylineVertex> triangle_vertices;

  [[nodiscard]] bool empty() const { return triangle_vertices.empty(); }
};

UITessellatedPath tessellate_path(
    const UIPathCommand &command,
    const UIPathTessellationOptions &options = {}
);

} // namespace astralix::ui
