#pragma once

#include "terrain-subgraph.hpp"
#include "base.hpp"
#include <vector>

namespace astralix::terrain {

class TerrainGraph {
public:
  void add_subgraph(Scope<TerrainSubgraph> subgraph);
  void compile();
  void process();

  std::span<const Scope<TerrainSubgraph>> subgraphs() const;
  const TerrainSubgraph *find_subgraph(std::string_view name) const;

private:
  std::vector<Scope<TerrainSubgraph>> m_subgraphs;
  std::vector<size_t> m_execution_order;
  bool m_compiled = false;
};

} // namespace astralix::terrain
