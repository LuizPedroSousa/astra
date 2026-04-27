#include "terrain-graph.hpp"
#include "assert.hpp"
#include "trace.hpp"
#include <unordered_map>
#include <unordered_set>

namespace astralix::terrain {

void TerrainGraph::add_subgraph(Scope<TerrainSubgraph> subgraph) {
  m_subgraphs.push_back(std::move(subgraph));
  m_compiled = false;
}

void TerrainGraph::compile() {
  ASTRA_PROFILE_N("TerrainGraph::compile");

  size_t count = m_subgraphs.size();

  for (auto &subgraph : m_subgraphs) {
    subgraph->compile();
  }

  std::unordered_map<std::string, size_t> output_producers;
  for (size_t index = 0; index < count; ++index) {
    for (const auto &port : m_subgraphs[index]->output_ports()) {
      output_producers[std::string(m_subgraphs[index]->name()) + "." + port.name] = index;
    }
  }

  std::vector<std::unordered_set<size_t>> dependencies(count);
  for (size_t index = 0; index < count; ++index) {
    for (const auto &port : m_subgraphs[index]->input_ports()) {
      auto iterator = output_producers.find(port.name);
      if (iterator != output_producers.end() && iterator->second != index) {
        dependencies[index].insert(iterator->second);
      }
    }
  }

  m_execution_order.clear();
  m_execution_order.reserve(count);
  std::vector<bool> visited(count, false);
  std::vector<bool> in_stack(count, false);

  auto topological_visit = [&](auto &self, size_t node) -> void {
    if (visited[node]) {
      return;
    }

    ASTRA_ENSURE(in_stack[node],
                  "TerrainGraph: cyclic subgraph dependency involving '%.*s'",
                  static_cast<int>(m_subgraphs[node]->name().size()),
                  m_subgraphs[node]->name().data());

    in_stack[node] = true;

    for (size_t dependency : dependencies[node]) {
      self(self, dependency);
    }

    in_stack[node] = false;
    visited[node] = true;
    m_execution_order.push_back(node);
  };

  for (size_t index = 0; index < count; ++index) {
    topological_visit(topological_visit, index);
  }

  m_compiled = true;
}

void TerrainGraph::process() {
  ASTRA_PROFILE_N("TerrainGraph::process");

  ASTRA_ENSURE(!m_compiled, "TerrainGraph::process called before compile()");

  std::unordered_map<std::string, const SubgraphOutputData *> available_outputs;

  for (size_t index : m_execution_order) {
    auto &subgraph = m_subgraphs[index];
    if (!subgraph->enabled) {
      continue;
    }

    std::unordered_map<std::string, const SubgraphOutputData *> resolved_inputs;
    for (const auto &port : subgraph->input_ports()) {
      auto iterator = available_outputs.find(port.name);
      if (iterator != available_outputs.end()) {
        resolved_inputs[port.name] = iterator->second;
      }
    }

    subgraph->process(resolved_inputs);

    for (const auto &port : subgraph->output_ports()) {
      std::string qualified_name =
          std::string(subgraph->name()) + "." + port.name;
      const auto *output = subgraph->get_output(port.name);
      if (output != nullptr) {
        available_outputs[qualified_name] = output;
      }
    }
  }
}

std::span<const Scope<TerrainSubgraph>> TerrainGraph::subgraphs() const {
  return m_subgraphs;
}

const TerrainSubgraph *TerrainGraph::find_subgraph(std::string_view name) const {
  for (const auto &subgraph : m_subgraphs) {
    if (subgraph->name() == name) {
      return subgraph.get();
    }
  }
  return nullptr;
}

} // namespace astralix::terrain
