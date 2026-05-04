#pragma once

#include "widgets/graph-view.hpp"

#include <unordered_set>

namespace astralix::ui {

struct UIGraphDagLayoutOptions {
  glm::vec2 origin = glm::vec2(48.0f, 48.0f);
  float column_spacing = 320.0f;
  float row_spacing = 156.0f;
  size_t ordering_iterations = 4u;
  float collision_padding = 24.0f;
};

void layout_graph_dag(
    UIGraphViewModel &model,
    const std::unordered_set<UIGraphId> &fixed_node_ids = {},
    const UIGraphDagLayoutOptions &options = {}
);

} // namespace astralix::ui
