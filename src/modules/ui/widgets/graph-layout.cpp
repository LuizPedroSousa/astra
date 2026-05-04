#include "widgets/graph-layout.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <utility>
#include <vector>

namespace astralix::ui {
namespace {

struct GraphNodeRecord {
  std::vector<size_t> predecessors;
  std::vector<size_t> successors;
  int layer = 0;
  bool fixed = false;
};

bool collides_with_occupied(
    float candidate_y,
    const std::vector<float> &occupied_y,
    float minimum_gap
) {
  for (const float y : occupied_y) {
    if (std::fabs(candidate_y - y) < minimum_gap) {
      return true;
    }
  }
  return false;
}

float resolve_free_y(
    float desired_y,
    std::vector<float> &occupied_y,
    const UIGraphDagLayoutOptions &options
) {
  const float minimum_gap = std::max(
      options.row_spacing - options.collision_padding,
      options.row_spacing * 0.5f
  );
  if (!collides_with_occupied(desired_y, occupied_y, minimum_gap)) {
    occupied_y.push_back(desired_y);
    return desired_y;
  }

  for (size_t step = 1u; step < 2048u; ++step) {
    const float below = desired_y + static_cast<float>(step) * options.row_spacing;
    if (!collides_with_occupied(below, occupied_y, minimum_gap)) {
      occupied_y.push_back(below);
      return below;
    }

    const float above = desired_y - static_cast<float>(step) * options.row_spacing;
    if (!collides_with_occupied(above, occupied_y, minimum_gap)) {
      occupied_y.push_back(above);
      return above;
    }
  }

  const float fallback =
      desired_y + static_cast<float>(occupied_y.size()) * options.row_spacing;
  occupied_y.push_back(fallback);
  return fallback;
}

} // namespace

void layout_graph_dag(
    UIGraphViewModel &model,
    const std::unordered_set<UIGraphId> &fixed_node_ids,
    const UIGraphDagLayoutOptions &options
) {
  if (model.nodes.empty()) {
    return;
  }

  std::unordered_map<UIGraphId, size_t> node_indices;
  node_indices.reserve(model.nodes.size());
  for (size_t index = 0u; index < model.nodes.size(); ++index) {
    node_indices.emplace(model.nodes[index].id, index);
  }

  std::unordered_map<UIGraphId, UIGraphId> port_nodes;
  port_nodes.reserve(model.ports.size());
  for (const auto &port : model.ports) {
    port_nodes.emplace(port.id, port.node_id);
  }

  std::vector<GraphNodeRecord> records(model.nodes.size());
  for (size_t index = 0u; index < model.nodes.size(); ++index) {
    records[index].fixed = fixed_node_ids.contains(model.nodes[index].id);
  }

  for (const auto &edge : model.edges) {
    const auto from_port_it = port_nodes.find(edge.from_port_id);
    const auto to_port_it = port_nodes.find(edge.to_port_id);
    if (from_port_it == port_nodes.end() || to_port_it == port_nodes.end()) {
      continue;
    }

    const auto from_node_it = node_indices.find(from_port_it->second);
    const auto to_node_it = node_indices.find(to_port_it->second);
    if (from_node_it == node_indices.end() || to_node_it == node_indices.end()) {
      continue;
    }

    const size_t from_index = from_node_it->second;
    const size_t to_index = to_node_it->second;
    if (from_index == to_index) {
      continue;
    }

    records[from_index].successors.push_back(to_index);
    records[to_index].predecessors.push_back(from_index);
  }

  std::vector<int> indegree(model.nodes.size(), 0);
  for (size_t index = 0u; index < records.size(); ++index) {
    indegree[index] = static_cast<int>(records[index].predecessors.size());
  }

  std::vector<size_t> queue;
  queue.reserve(model.nodes.size());
  for (size_t index = 0u; index < indegree.size(); ++index) {
    if (indegree[index] == 0) {
      queue.push_back(index);
    }
  }

  std::vector<bool> visited(model.nodes.size(), false);
  for (size_t head = 0u; head < queue.size(); ++head) {
    const size_t current = queue[head];
    visited[current] = true;

    for (const size_t successor : records[current].successors) {
      records[successor].layer = std::max(
          records[successor].layer,
          records[current].layer + 1
      );
      --indegree[successor];
      if (indegree[successor] == 0) {
        queue.push_back(successor);
      }
    }
  }

  for (size_t index = 0u; index < records.size(); ++index) {
    if (visited[index]) {
      continue;
    }

    int fallback_layer = 0;
    for (const size_t predecessor : records[index].predecessors) {
      fallback_layer = std::max(
          fallback_layer,
          records[predecessor].layer + 1
      );
    }
    records[index].layer = std::max(records[index].layer, fallback_layer);
  }

  int max_layer = 0;
  for (const auto &record : records) {
    max_layer = std::max(max_layer, record.layer);
  }

  std::vector<std::vector<size_t>> layers(static_cast<size_t>(max_layer + 1u));
  for (size_t index = 0u; index < records.size(); ++index) {
    layers[static_cast<size_t>(records[index].layer)].push_back(index);
  }

  std::vector<size_t> order_in_layer(model.nodes.size(), 0u);
  auto rebuild_order_index = [&]() {
    for (const auto &layer : layers) {
      for (size_t position = 0u; position < layer.size(); ++position) {
        order_in_layer[layer[position]] = position;
      }
    }
  };

  rebuild_order_index();

  auto sort_layer = [&](size_t layer_index, bool use_predecessors) {
    auto &layer = layers[layer_index];
    std::stable_sort(
        layer.begin(),
        layer.end(),
        [&](size_t lhs, size_t rhs) {
          auto barycenter_for = [&](size_t node_index) {
            const auto &neighbors = use_predecessors
                                        ? records[node_index].predecessors
                                        : records[node_index].successors;
            if (records[node_index].fixed) {
              return model.nodes[node_index].position.y / options.row_spacing;
            }

            if (neighbors.empty()) {
              return static_cast<float>(order_in_layer[node_index]);
            }

            float sum = 0.0f;
            for (const size_t neighbor : neighbors) {
              sum += static_cast<float>(order_in_layer[neighbor]);
            }
            return sum / static_cast<float>(neighbors.size());
          };

          const float lhs_key = barycenter_for(lhs);
          const float rhs_key = barycenter_for(rhs);
          if (lhs_key == rhs_key) {
            return order_in_layer[lhs] < order_in_layer[rhs];
          }
          return lhs_key < rhs_key;
        }
    );
    rebuild_order_index();
  };

  for (size_t iteration = 0u; iteration < options.ordering_iterations; ++iteration) {
    for (size_t layer = 1u; layer < layers.size(); ++layer) {
      sort_layer(layer, true);
    }
    for (size_t layer = layers.size(); layer-- > 1u;) {
      sort_layer(layer - 1u, false);
    }
  }

  float column_offset = 0.0f;
  size_t fixed_x_count = 0u;
  for (size_t index = 0u; index < model.nodes.size(); ++index) {
    if (!records[index].fixed) {
      continue;
    }

    const float canonical_x =
        options.origin.x +
        static_cast<float>(records[index].layer) * options.column_spacing;
    column_offset += model.nodes[index].position.x - canonical_x;
    ++fixed_x_count;
  }
  if (fixed_x_count > 0u) {
    column_offset /= static_cast<float>(fixed_x_count);
  }

  std::vector<float> layer_x(layers.size(), options.origin.x);
  for (size_t layer = 0u; layer < layers.size(); ++layer) {
    float fixed_sum = 0.0f;
    size_t fixed_count = 0u;
    for (const size_t node_index : layers[layer]) {
      if (!records[node_index].fixed) {
        continue;
      }

      fixed_sum += model.nodes[node_index].position.x;
      ++fixed_count;
    }

    if (fixed_count > 0u) {
      layer_x[layer] = fixed_sum / static_cast<float>(fixed_count);
    } else {
      layer_x[layer] = options.origin.x + column_offset +
                       static_cast<float>(layer) * options.column_spacing;
    }
  }

  for (size_t layer = 0u; layer < layers.size(); ++layer) {
    std::vector<float> occupied_y;
    occupied_y.reserve(layers[layer].size());

    for (const size_t node_index : layers[layer]) {
      if (!records[node_index].fixed) {
        continue;
      }
      occupied_y.push_back(model.nodes[node_index].position.y);
    }

    std::vector<std::pair<size_t, float>> pending_nodes;
    pending_nodes.reserve(layers[layer].size());

    for (size_t order = 0u; order < layers[layer].size(); ++order) {
      const size_t node_index = layers[layer][order];
      if (records[node_index].fixed) {
        continue;
      }

      float desired_y =
          options.origin.y + static_cast<float>(order) * options.row_spacing;
      float anchored_neighbor_y = 0.0f;
      size_t anchored_neighbor_count = 0u;
      for (const size_t predecessor : records[node_index].predecessors) {
        if (!records[predecessor].fixed) {
          continue;
        }
        anchored_neighbor_y += model.nodes[predecessor].position.y;
        ++anchored_neighbor_count;
      }
      for (const size_t successor : records[node_index].successors) {
        if (!records[successor].fixed) {
          continue;
        }
        anchored_neighbor_y += model.nodes[successor].position.y;
        ++anchored_neighbor_count;
      }
      if (anchored_neighbor_count > 0u) {
        const float neighbor_average =
            anchored_neighbor_y / static_cast<float>(anchored_neighbor_count);
        desired_y = desired_y * 0.65f + neighbor_average * 0.35f;
      }

      pending_nodes.emplace_back(node_index, desired_y);
    }

    std::sort(
        pending_nodes.begin(),
        pending_nodes.end(),
        [](const auto &lhs, const auto &rhs) {
          if (lhs.second == rhs.second) {
            return lhs.first < rhs.first;
          }
          return lhs.second < rhs.second;
        }
    );

    for (const auto &[node_index, desired_y] : pending_nodes) {
      model.nodes[node_index].position = glm::vec2(
          layer_x[layer],
          resolve_free_y(desired_y, occupied_y, options)
      );
    }
  }
}

} // namespace astralix::ui
